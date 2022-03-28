#include <iostream>
#include <sys/socket.h> 
#include <sys/epoll.h>
#include <netinet/in.h> 
#include <arpa/inet.h>
#include <fcntl.h> 
#include <unistd.h> 
#include <string.h>
#include <stdio.h>
#include <vector>
#include <map>
#include <queue>
#include <thread>
#include <mutex>
#include <memory>
#include <functional>
#include <condition_variable>
#include <sqlite3.h>
#include <hiredis/hiredis.h>

using namespace std;

#include "message_struct.h"
#include "safe_queue.h"
#include "thread_pool.h"
#include "utils.h"

void close_socks();  // 关闭socket的函数

#define TMP_BUF_MAX 4096
#define CLIENT_BUF_MAX 4096 * 1024
#define OPEN_MAX 100
#define LISTENQ 20  // 监听队列最大长度
#define SERV_PORT 12345  // 服务器端口
#define INFTIM 1000


char des_key[] = "neteasy_ltzyhhhh";

const string REDIS_ONLINE_IPSET = "online_ipset";   // redis缓存的在线的ip集合
const string db_name = "ltdb";  // sqlite数据库存储的消息记录的数据库
const string prem_table = "pst_mess";  // 永久记录
const string tmp_table = "tmp_mess";  // 临时记录

// sqlite对象
sqlite3 *sqlite_db;
char *zErrMsg = 0;
//vector<TranMessage> sqlite_select_mess;  // 全局查询对象  从sqlite中查找的数据
char sqlite_select_mess[TMP_BUF_MAX * 2014];
int sqlite_select_mess_len;
//vector<int> sqlite_select_ids;  // 全局查询对象的ID  从sqlite中查找的数据
int sqlite_select_ids[TMP_BUF_MAX];
int sqlite_select_ids_num;  // 全局查询对象数量
int sqlite_insert(const char *sql_chs, sqlite3 *db, char *zErrMsg);  // 插入操作
string gen_insert_sql_chs(TranMessage *handle_mess);  // 生成insert sql
string gen_selip_sql_chs(string _table_name, string _to_ip);  // 生成select sql
string gen_delid_sql_chs(string _table_name);  // 生成delete sql
int sqlite_query(const char *sql_chs, sqlite3 *db, char *zErrMsg);  // 查询操作
int sqlite_delete(const char *sql_chs, sqlite3 *db, char *zErrMsg);  // 删除操作



// socket的客户端对象
class ClientSock
{
private:
	int client_recv_sockfd;  // 该客户端使用的读socket
	char client_msg_buf[CLIENT_BUF_MAX];  // 客户端使用的缓存
	int cur_msgbuf_len;  // 当前消息缓存的长度

public:
	ClientSock(int recv_fd) {
		this->client_recv_sockfd = recv_fd;
		memset(this->client_msg_buf, 0, CLIENT_BUF_MAX);
		this->cur_msgbuf_len = 0;
	}

	int get_recv_socket() {
		return this->client_recv_sockfd;
	}

	char *get_msg_buf() {
		return this->client_msg_buf;
	}

	int get_cur_msgbuf_len() {
		return this->cur_msgbuf_len;
	}

	void set_msgbuf_len(int len) {
		this->cur_msgbuf_len = len;
	}
};

// socket与ip地址的映射
mutex recv_mpip_mux;  // 接收套接字与ip、port的映射 的信号量
map<int, string> recv_sockfd_mp_ip;

// ip与socket的映射
//map<string, int> send_ip_mp_sockfd;
mutex recv_mpmx_mux;  // 接收socket的mutex  的信号量
map<int, mutex*> recv_sockfd_mp_mux;

// 聊天室  聊天室id-成员sockets
map<string, vector<string>> lt_room;


// 查找socket映射的ip port
int my_getpeername_recv(int cur_sockfd, string *close_ip, int *close_port) {
	int f = 0;
	map<int, string>::iterator it = recv_sockfd_mp_ip.find(cur_sockfd);
	if (it != recv_sockfd_mp_ip.end()) {
		*close_ip = it->second;
		f += 1;
	}

	if (f == 1) return 0;
	return -1;
}


// 插入socket映射的ip port
void my_insert_sockfd_map_recv(int _recvfd, char *_ip, int _port) {
	for (map<int, string>::iterator it = recv_sockfd_mp_ip.begin(); it != recv_sockfd_mp_ip.end(); it++) {
		if (strcmp(it->second.c_str(), _ip) == 0) {
			recv_sockfd_mp_ip.erase(it->first);
			break;
		}
	}
	recv_sockfd_mp_ip.insert(pair<int, string>(_recvfd, _ip));
}

// 去除socket映射的ip port
void my_erase_sockfd_map_recv(int _recv_sockfd) {
	if (recv_sockfd_mp_ip.find(_recv_sockfd) == recv_sockfd_mp_ip.end()) {
		cout << "error: no socket_map_ip" << endl;
		return;
	}
	// 找到_recv_sockfd对应的ip
	string _ip = recv_sockfd_mp_ip.find(_recv_sockfd)->second;
	recv_mpip_mux.lock();
	recv_sockfd_mp_ip.erase(_recv_sockfd);  // 删除socket
	printf("erase socket %d\n", _recv_sockfd);
	recv_mpip_mux.unlock();

	// 移除socket的mutex
	recv_mpmx_mux.lock();
	recv_sockfd_mp_mux.erase(_recv_sockfd);  // 删除socket
	printf("erase socket %d\n", _recv_sockfd);
	recv_mpmx_mux.unlock();

	printf("%s off-line\n", _ip.data());
}

// 设置套接字为非阻塞
void setnonblocking(int sock)
{
	int opts;
	opts = fcntl(sock, F_GETFL);
	if (opts < 0)
	{
		perror("fcntl(sock, GETFL)");
		exit(1);
	}

	opts = opts | O_NONBLOCK;
	if (fcntl(sock, F_SETFL, opts) < 0)
	{
		perror("fcntl(sock,SETFL,opts)");
		exit(1);
	}
}


// 创建一些socket
int listenfd, nfds, recvfd;


// 所有的已连接的客户端
vector<ClientSock*> read_clients;
// 客户端对象锁
mutex clients_mux;

// 生成用于处理accept的epoll专用的文件描述符, 指定生成描述符的最大范围为256 
int epfd = epoll_create(256);
// 声明epoll_event结构体的变量, ev用于注册事件, events数组用于回传要处理的事件
struct epoll_event ev, events[20];

// 存放消息的队列
safe_queue<TranMessage> mess_que;

// 处理redis的类
redisContext *redis_handle;

int handle_mess_que(int tid);
int handle_read_sock(int event_fd);
void test_lt_room(char *_ip);

int post110_num = 0;  // 与202.193.58.1110连接的数量

char tmp_buf[TMP_BUF_MAX];

string redis_cmd = "keys *";
socklen_t clilen;
// 网络地址结构体   协议簇、ip、端口
struct sockaddr_in clientaddr;  // 客户端
struct sockaddr_in serveraddr;

// 定义线程池
std::threadpool executor{ 16 };

int main()
{
	recv_sockfd_mp_ip.clear();
	while (!mess_que.empty())
		mess_que.try_pop();

	// 创建 监听连接的套接字  listenfd用于监听
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	setnonblocking(listenfd);       // 把用于监听的socket设置为非阻塞方式
	ev.data.fd = listenfd;          // 设置与要处理的事件相关的文件描述符
	ev.events = EPOLLIN | EPOLLET;  // 设置要处理的事件类型
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);     // 注册epoll事件  注册listenfd到epfd中
	//bzero(&serveraddr, sizeof(serveraddr));
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	char *local_addr = "202.193.58.110"; 
	inet_aton(local_addr, &(serveraddr.sin_addr));  // 设置ip到监听socket中
	serveraddr.sin_port = htons(SERV_PORT);  // 设置port到监听socket中
	
	// 绑定 监听套接字上的地址
	bind(listenfd, (sockaddr *)&serveraddr, sizeof(serveraddr));

	// 监听
	listen(listenfd, LISTENQ);
	printf("listen...\n");

	// 初始化客户端socket
	clilen = sizeof(clientaddr);

	// 连接redis，若出错redisContext.err会设置为1，redisContext.errstr会包含描述错误信息
	redis_handle = redisConnect("127.0.0.1", 6379);
	if (redis_handle == NULL || redis_handle->err) {
		if (redis_handle) {
			printf("Error:%s\n", redis_handle->errstr);
		}
		else {
			printf("can't allocate redis context\n");
		}
		close_socks();
		return -1;
	}

	// 打开sqllite数据库
	int rc = sqlite3_open((db_name + string(".db")).c_str(), &sqlite_db);  // (db_name + string(".db")).c_str()
	if (rc) {
		close_socks();
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(sqlite_db));
		return 0;
	}
	else {
		fprintf(stderr, "Opened database successfully\n");
	}
	

	// 从线程池中8个消息处理线程
	for (int i = 0; i < 8; i++) {
		executor.commit(handle_mess_que, i);
	}

	for (; ; )
	{

		nfds = epoll_wait(epfd, events, 20, 500);  // 等待epoll事件的发生  等待注册到epfd中的套接字准备好
		for (int i = 0; i < nfds; i++)                 // 处理所发生的所有事件
		{
			if (events[i].data.fd == listenfd)      // 监听事件（连接）
			{
				// 建立连接的读套接字
				recvfd = accept(listenfd, (sockaddr *)&clientaddr, &clilen);
				char *con_ip = inet_ntoa(clientaddr.sin_addr);
				//char *con_ip = "202.193.58.110";
				int con_port = ntohs(clientaddr.sin_port);
				if (recvfd < 0)
				{
					//close_socks();
					printf("%s\n", strerror(errno));
					perror("connfd<0");
					//exit(1);
				}
				
				test_lt_room(con_ip);
				sleep(1);
				//printf("sleep done. \n");

				setnonblocking(recvfd);           // 把客户端的socket设置为非阻塞方式**
				
				// 创建新的连接客户端
				clients_mux.lock();
				read_clients.push_back(new ClientSock(recvfd));  // 添加一个新的客户端
				clients_mux.unlock();

				// socket(recv)的ip映射
				recv_mpip_mux.lock();
				my_insert_sockfd_map_recv(recvfd, con_ip, con_port);
				recv_mpip_mux.unlock();
				
				// socket(recv/send)与mutex的映射
				recv_mpmx_mux.lock();  // map加锁
				mutex *recv_mutex = new mutex;
				recv_sockfd_mp_mux.emplace(recvfd, recv_mutex);
				recv_mpmx_mux.unlock();


				// 用户上线
				// ip更新到redis中
				string redis_cmd = "SADD online_ipset " + string(con_ip);
				redisReply *redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
				if (redis_reply == NULL) {  // reply为null，可能是执行命令错误，也可能是长时间不连接redis而断开
					printf("Error:%s, command:%s\n", redis_handle->errstr, redis_cmd.c_str());
					redisReconnect(redis_handle);
					printf("redisReconnect...\n");
					redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
				}
				else {
					printf("%s %d connected, in redis\n", con_ip, con_port);
				}
				freeReplyObject(redis_reply);  // 使用reply以后必须要进行释放

				// 从数据库读数据
				sqlite_select_mess_len = 0;
				sqlite_select_ids_num = 0;
				string sql_str = gen_selip_sql_chs("tmp_mess", con_ip);
				if (sqlite_query(sql_str.c_str(), sqlite_db, zErrMsg) != 0) {
					cout << "select sqlite error!" << endl;
				}
				//cout << "cur sqlite_select_mess len: " << sqlite_select_mess_len << ", size: " << sqlite_select_ids_num << endl;

				recv_mutex->lock();  // 使用刚才创建的发送socket发送历史消息
				//for (int _i = 0; _i < sqlite_select_mess_num; _i++) {
				//	usleep(2000);

				//	// 生成发送的数据
				//	char sock_send_sock[TMP_BUF_MAX];  // 发送消息用的缓存
				//	memcpy(sock_send_sock, (char *)(&sqlite_select_mess[_i]), sqlite_select_mess[_i].head_length);
				//	memcpy(sock_send_sock + sqlite_select_mess[_i].head_length, sqlite_select_mess[_i].message.content, strlen(sqlite_select_mess[_i].message.content) + 1);

				//	printf("login send: %s %s\n", sqlite_select_mess[_i].from.user_ip, sqlite_select_mess[_i].message.content);
				//	printf("sock_send_sock: %s\n", sock_send_sock + sqlite_select_mess[_i].head_length);

				//	int send_len = send(recvfd, sock_send_sock, sqlite_select_mess[_i].protocol_length, 0);
				//	printf("login recv_len: %d\n", send_len);
				//}

				while (sqlite_select_mess_len >= int(sizeof(TranMessage))) {
					TranMessage *_t_mess = (TranMessage*)sqlite_select_mess;
					if (sqlite_select_mess_len >= _t_mess->protocol_length) {
						//printf("login send: to %s, content %s\n", _t_mess->to.user_ip, sqlite_select_mess + _t_mess->head_length);
						int _recv_len = send(recvfd, sqlite_select_mess, _t_mess->protocol_length, 0);
						usleep(2000);
						int _rem = sqlite_select_mess_len - _t_mess->protocol_length;
						memmove(sqlite_select_mess, sqlite_select_mess + _t_mess->protocol_length, _rem);
						sqlite_select_mess_len = _rem; 
					}
					else {
						break;
					}
				}
				if (sqlite_select_mess_len != 0) {
					cout << "sqlite_select_mess_len: " << sqlite_select_mess_len << endl;
					cout << "sqlite_select_mess_len != 0, sqlite select handle error!" << endl;
				}

				recv_mutex->unlock();

				if (sqlite_select_ids_num > 0) {  // 清理临时数据库
					sql_str = gen_delid_sql_chs("tmp_mess");
					if (sqlite_delete(sql_str.c_str(), sqlite_db, zErrMsg) != 0) {
						cout << "delete sqlite error!" << endl;
					}
				}

				//注册ev事件
				ev.data.fd = recvfd;                // 设置用于读操作的文件描述符
				ev.events = EPOLLIN | EPOLLET;      // 设置用于注重读操作的事件
				epoll_ctl(epfd, EPOLL_CTL_ADD, recvfd, &ev);
			}

			// 【读事件】   （需要接收数据了）
			else if (events[i].events & EPOLLIN)
			{
				//printf("\nepoll EPOLLIN\n");

				if (events[i].data.fd < 0)
				{
					printf("events.data.fd < 0\n");
					continue;
				}
				int e_fd = events[i].data.fd;
				
				// 使用线程池  处理可读socket
				executor.commit(handle_read_sock, e_fd);
			}
		}
	}


	// 释放redis连接
	redisFree(redis_handle);

	// 关闭sqlite数据库
	sqlite3_close(sqlite_db);

	return 0;

}

// 关闭套接字
void close_socks() {
	close(listenfd);
	close(epfd);
	close(recvfd);

	for (int i = 0; i < read_clients.size(); i++) {
		close(read_clients[i]->get_recv_socket());
	}

	for (map<int, string>::iterator it = recv_sockfd_mp_ip.begin(); it != recv_sockfd_mp_ip.end(); it++) {
		close(it->first);
	}
}


char sock_tmp_buf[TMP_BUF_MAX];
// 处理读事件
int handle_read_sock(int event_fd) {

	if (recv_sockfd_mp_mux.find(event_fd) == recv_sockfd_mp_mux.end()) {
		cout << "recv error, no socket..." << endl;
		return -1;
	}

	mutex *t_recv_mux = recv_sockfd_mp_mux.find(event_fd)->second;  // 获取接收套接字的互斥量  也可当作客户端的锁
	
	for (int j = read_clients.size() - 1; j >= 0; j--) {
		ClientSock *t_c_sock = read_clients[j];  // 已连接客户端的socket
		if (read_clients[j]->get_recv_socket() == event_fd) {
			
			int cur_sockfd = t_c_sock->get_recv_socket();
			// recv需要while
			while (1) {
				//printf("\n\nrun recv...\n");
				t_recv_mux->lock();
				int recv_len = recv(cur_sockfd, sock_tmp_buf, sizeof(sock_tmp_buf), 0);
				t_recv_mux->unlock();

				//printf("sizeof sock_tmp_buf: %d, recv_len: %d\n", sizeof(sock_tmp_buf), recv_len);
				if (recv_len > 0) {  // 正常读到数据
					t_recv_mux->lock();
					// 将recv的数据复制client缓存中
					memcpy(t_c_sock->get_msg_buf() + t_c_sock->get_cur_msgbuf_len(), sock_tmp_buf, recv_len);
					t_c_sock->set_msgbuf_len(t_c_sock->get_cur_msgbuf_len() + recv_len);
					//printf("msgbuf_len: %d\n", t_c_sock->get_cur_msgbuf_len());  // 客户端保存数据的缓存
					// 处理client缓存中的数据
					while (t_c_sock->get_cur_msgbuf_len() > int(sizeof(TranMessage))) {
						TranMessage *recv_mh = (TranMessage *)t_c_sock->get_msg_buf();
						if (t_c_sock->get_cur_msgbuf_len() >= recv_mh->protocol_length) {

							// 得到消息头部 handle_msg(t_c_sock->get_socket(), recv_mh);
							TranMessage *mess = new TranMessage();
							mess = recv_mh;
							
							// 处理数据部分 - 数据复制到消息空间中
							int _content_len = mess->protocol_length - mess->head_length;
							char *data_mess = new char[_content_len];
							data_mess[_content_len] = '\0';
							memcpy(data_mess, t_c_sock->get_msg_buf() + mess->head_length, mess->protocol_length - mess->head_length);
							//printf("data_mess before: %s \n", data_mess);
							
							// 解密
							/*
							string decryptText = des_decrypt(data_mess, des_key);
							char *dec_str = new char[decryptText.length()];
							memcpy(dec_str, decryptText.c_str(), decryptText.length());
							dec_str[strlen(dec_str)] = '\0';
							printf("dec_str: %s\n", dec_str);
							*/
							
							mess->message.content = data_mess;
							mess->set_data_length();
							//printf("mess protocol_len(server): %d", mess->protocol_length);

							mess_que.push(mess);
							//printf("message head: %d %d\n", recv_mh->data_length, recv_mh->message_type);
							//printf("recv message, from:%s to:%s content:%s\n", mess->from.user_ip, mess->to.user_ip, mess->message.content);
							//printf("data_mess after: %s \n", data_mess);

							// 修改ClientSock的消息缓存
							int rem_size = t_c_sock->get_cur_msgbuf_len() - recv_mh->protocol_length;  // 客户端缓存剩下未处理的数据
							memmove(t_c_sock->get_msg_buf(), t_c_sock->get_msg_buf() + mess->protocol_length, rem_size);
							t_c_sock->set_msgbuf_len(rem_size);
						}
						else {
							break;
						}
					}
					t_recv_mux->unlock();
				}
				else if (recv_len == 0) {  // 对方关闭连接
					// 将退出的ip，从redis中移除
					string close_ip;
					int close_port;
					if (!my_getpeername_recv(cur_sockfd, &close_ip, &close_port))
					{
						string redis_cmd = "SREM online_ipset " + close_ip;
						redisReply *redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
						if (redis_reply == NULL) {  // reply为null，可能是执行命令错误，也可能是长时间不连接redis而断开
							printf("Error:%s, command:%s\n", redis_handle->errstr, redis_cmd.c_str());
							redisReconnect(redis_handle);
							printf("redisReconnect...\n");
							redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
						}
						else {
							printf("%s close, not in redis\n", close_ip.c_str());
						}
						freeReplyObject(redis_reply);  // 使用reply以后必须要进行释放
					}
					else {
						printf("one connection closed, no ip\n");
					}

					printf("recv=0, obj %d close con\n", cur_sockfd);
					close(cur_sockfd);

					// 清除cur_sockfd 对应的ip
					my_erase_sockfd_map_recv(cur_sockfd);

					auto it = read_clients.begin() + j;
					clients_mux.lock();
					read_clients.erase(it);
					printf("erase client...\n");
					clients_mux.unlock();
					break;
				}
				else {
					if (recv_len == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
						// 把sockfdrecv完了，跳出循环
						break;
					}
					else {
						string close_ip;
						int close_port;
						if (!my_getpeername_recv(cur_sockfd, &close_ip, &close_port))
						{
							string redis_cmd = "SREM online_ipset " + close_ip;
							redisReply *redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
							if (redis_reply == NULL) {  // reply为null，可能是执行命令错误，也可能是长时间不连接redis而断开
								printf("Error:%s, command:%s\n", redis_handle->errstr, redis_cmd.c_str());
								redisReconnect(redis_handle);
								printf("redisReconnect...\n");
								redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
							}
							else {
								printf("%s close, not in redis\n", close_ip.c_str());
							}
							freeReplyObject(redis_reply);  // 使用reply以后必须要进行释放
						}
						else {
							printf("recv<0, one connection closed, no ip\n");
						}

						// 出错
						//close_socks();
						perror("recv < 0:");
						printf("%d error, %d\n", cur_sockfd, errno);
						printf("obj(sock)  %d con exit, no close\n", cur_sockfd);
						
						// 移除socket_map_中的元素
						my_erase_sockfd_map_recv(cur_sockfd);
						close(cur_sockfd);

						// 移除客户端
						auto it = read_clients.begin() + j;
						clients_mux.lock();
						read_clients.erase(it);
						clients_mux.unlock();
						break;
					}
				}
			}
			break;
		}
	}
	return 0;  // 函数结束  线程结束
}


// 处理消息队列 转发消息
int handle_mess_que(int tid) {
	while (1) {

		TranMessage* handle_mess = mess_que.wait_and_pop();  // 从消息接收队列中取消息
		string to_ip = handle_mess->to.user_ip;
		//string to_ip = "202.193.58.110";
		//printf("mess from %s, to %s, content %s\n", handle_mess->from.user_ip, handle_mess->to.user_ip, handle_mess->message.content);

		// 发送数据
		if (handle_mess->message_type == SING_COMM) {
			// 单聊
			int _send_sock = -1;
			recv_mpip_mux.lock();
			for (map<int, string>::iterator it = recv_sockfd_mp_ip.begin(); it != recv_sockfd_mp_ip.end(); it++) {
				
				if (strcmp(it->second.c_str(), to_ip.c_str()) == 0) {
					_send_sock = it->first;
					//break;
				}
			}
			recv_mpip_mux.unlock();

			if (_send_sock == -1) {  // 对方不在线
				
				// 存数据库
				string sql_chs = gen_insert_sql_chs(handle_mess); 
				if (sqlite_insert(sql_chs.c_str(), sqlite_db, zErrMsg) != 0) {
					cout << "sqlite inster error..." << endl;
				}
				else {
					printf("saved in sqlite\n");
				}

			}
			else {
				if (recv_sockfd_mp_mux.find(_send_sock) == recv_sockfd_mp_mux.end()) {
					cout << "SING send error, no socket..." << endl;
					break;
				}
				mutex *t_send_mux = recv_sockfd_mp_mux.find(_send_sock)->second;  // 发送数据的socket (与接收socket是同一个)
				t_send_mux->lock();

				// 生成发送的数据
				char sock_send_sock[TMP_BUF_MAX];  // 发送消息用的缓存
				memcpy(sock_send_sock, (char *)(handle_mess), handle_mess->head_length);
				memcpy(sock_send_sock + handle_mess->head_length, handle_mess->message.content, strlen(handle_mess->message.content) + 1);

				int send_len = send(_send_sock, sock_send_sock, handle_mess->protocol_length, 0);
				//cout << "send_len: " << send_len << endl;
				if (send_len <= 0) {
					printf("errrno: %d\n", errno);
					perror("server send error: ");
					if ((errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
						printf("handle mess_que, send error\n");
						perror("server send error: ");
						printf("errrno: %d\n", errno);
					}
					else {  // 发送失败
						// 处理断开连接的结果
						my_erase_sockfd_map_recv(_send_sock);

						// 存数据库
						string sql_chs = gen_insert_sql_chs(handle_mess);
						if (sqlite_insert(sql_chs.c_str(), sqlite_db, zErrMsg) != 0) {
							cout << "sqlite inster error..." << endl;
						}
						else {
							printf("saved in sqlite\n");
						}
					}
				}
				t_send_mux->unlock();
			}

		}
		else if (handle_mess->message_type == GUP_COMM) {  // 群聊
			if (lt_room.find(handle_mess->to.user_ip) == lt_room.end()) {
				cout << "no room" << endl;
				continue;
			} 
			vector<string> lt_room_ips = lt_room.find(handle_mess->to.user_ip)->second;  // 根据消息的目的地址找聊天室

			for (int _i = 0; _i < lt_room_ips.size(); _i++) {
				string _ip = lt_room_ips[_i];

				int _send_sock = -1;
				// 对聊天室中所有对象匹配发送socket
				for (map<int, string>::iterator it = recv_sockfd_mp_ip.begin(); it != recv_sockfd_mp_ip.end(); it++) {  
					if (strcmp(it->second.c_str(), _ip.c_str()) == 0) {
						_send_sock = it->first;
						break;
					}
				}

				if (_send_sock == -1) {  // 聊天室对象不在线
					// 存数据库
					TranMessage *t_mess(handle_mess);
					memcpy(t_mess->to.user_ip, _ip.c_str(), _ip.length());
					string sql_chs = gen_insert_sql_chs(t_mess);
					if (sqlite_insert(sql_chs.c_str(), sqlite_db, zErrMsg) != 0) {
						cout << "sqlite inster error..." << endl;
					}
					else {
						printf("saved in sqlite\n");
					}
				}
				else {
					// 使用发送socket发送数据  （与单聊类似）
					if (recv_sockfd_mp_mux.find(_send_sock) == recv_sockfd_mp_mux.end()) {
						cout << "GUP send error, no socket..." << endl;
						break;
					}
					mutex *t_send_mux = recv_sockfd_mp_mux.find(_send_sock)->second;  // 发送数据的socket 
					t_send_mux->lock();

					// 生成发送的数据
					char sock_send_sock[TMP_BUF_MAX];  // 发送消息用的缓存
					memcpy(sock_send_sock, (char *)(handle_mess), handle_mess->head_length);
					memcpy(sock_send_sock + handle_mess->head_length, handle_mess->message.content, strlen(handle_mess->message.content) + 1);

					int send_len = send(_send_sock, sock_send_sock, handle_mess->protocol_length, 0);
					if (send_len <= 0) {
						printf("errrno: %d\n", errno);
						perror("server send error: ");
						if ((errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
							printf("handle mess_que, send error\n");
							perror("server send error: ");
							printf("errrno: %d\n", errno);
						}
						else {  // 发送失败
							// 处理断开连接的结果
							my_erase_sockfd_map_recv(_send_sock);

							// 存数据库
							TranMessage *t_mess(handle_mess);
							memcpy(t_mess->to.user_ip, _ip.c_str(), _ip.length());
							string sql_chs = gen_insert_sql_chs(t_mess);
							if (sqlite_insert(sql_chs.c_str(), sqlite_db, zErrMsg) != 0) {
								cout << "sqlite inster error..." << endl;
							}
							else {
								printf("saved in sqlite\n");
							}
						}
					}
					t_send_mux->unlock();
				}
			}
		}
		
	}
	return 0;
}


// 测试聊天室功能
void test_lt_room(char *_ip) {
	char room_id[] = "20219358110";
	vector<string> members;
	members.push_back("202.193.59.136");
	members.push_back("202.193.58.110");
	members.push_back("202.193.59.127");
	lt_room.insert(pair<string, vector<string>>(room_id, members));
}

// 生成insert sql语句
string gen_insert_sql_chs(TranMessage *handle_mess)
{
	/*
	sql_chs = "INSERT INTO tmp_record(ID, CONTENT, SEND_TIME, RECV_TIME, FROM_IP, FROME_PORT, TO_IP, TO_PORT, STATE)"  \
		"VALUES(2, 'hello', '2020-3-10', '2020-3-10', '0.0.0.0', 12345, '0.0.0.0', 12345, 1)";
	*/

	string sql_str = "INSERT INTO tmp_mess(ID, MESSAGE_TYPE, CONTENT, TIME, FROM_USER, FROM_IP, FROME_PORT, TO_USER, TO_IP, TO_PORT) VALUES(";

	sql_str += "NULL,";  // ID
	sql_str += to_string(handle_mess->message_type) + ",";  // MESSAGE_TYPE
	if (handle_mess->message.content == "" || strlen(handle_mess->message.content) == 0) {
		sql_str += string("''");  // CONTENT
	}
	else {
		sql_str += string("'") + string(handle_mess->message.content) + "',";  // CONTENT
	}

	if (handle_mess->message.send_time == "" || strlen(handle_mess->message.send_time) == 0) {
		sql_str += string("''");  // TIME
	}
	else {
		sql_str += string("'") + string(handle_mess->message.send_time) + "',";  // TIME
	}

	//printf("string(handle_mess->from.username): %s\n", string(handle_mess->from.username).c_str());
	if (handle_mess->from.username == "" || strlen(handle_mess->from.username) == 0) {
		sql_str += string("''");
	}
	else {
		sql_str += string("'") + string(handle_mess->from.username) + "',";  // FROM_USER
	}
	if (handle_mess->from.user_ip == "" || strlen(handle_mess->from.user_ip) == 0) {
		sql_str += string("''");
	}
	else {
		sql_str += string("'") + string(handle_mess->from.user_ip) + "',";  // FROM_IP
	}
	sql_str += to_string(handle_mess->from.user_port) + ",";  // FROM_USER

	//printf("string(handle_mess->to.username): %s\n", string(handle_mess->to.username).c_str());
	if (handle_mess->to.username == "" || strlen(handle_mess->to.username) == 0) {
		sql_str += string("''");
	}
	else {
		sql_str += string("'") + string(handle_mess->to.username) + "',";  // TO_USER
	}
	if (handle_mess->to.user_ip == "" || strlen(handle_mess->to.user_ip) == 0) {
		sql_str += string("''");
	}
	else {
		sql_str += string("'") + string(handle_mess->to.user_ip) + "',";  // TO_IP
	}
	sql_str += to_string(handle_mess->to.user_port);  // TO_USER

	sql_str += ")";

	cout << "insert sql: " << sql_str << endl;

	return sql_str;
}

// 生成select sql语句
string gen_selip_sql_chs(string _table_name, string _to_ip)
{
	/*sql_chs = "SELECT * from tmp_record";*/
	string sql_str = "SELECT * from " + _table_name + " where to_ip='" + _to_ip + "'";
	cout << "select sql: " << sql_str << endl;
	return sql_str;
}


// 生成delete sql语句
string gen_delid_sql_chs(string _table_name)
{
	string sql_str = "DELETE from " + _table_name + " where ";
	sql_str += "ID=" + to_string(sqlite_select_ids[0]);
	int _size = sqlite_select_ids_num;
	if (_size > 1) {
		for (int i = 1; i < _size; i++) {
			sql_str += " OR ID=" + to_string(sqlite_select_ids[i]);
		}
	}
	//cout << "delete sql: " << sql_str << endl;
	return sql_str;
}
	
