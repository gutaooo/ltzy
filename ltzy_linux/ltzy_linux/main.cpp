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

void close_socks();  // �ر�socket�ĺ���

#define TMP_BUF_MAX 4096
#define CLIENT_BUF_MAX 4096 * 1024
#define OPEN_MAX 100
#define LISTENQ 20  // ����������󳤶�
#define SERV_PORT 12345  // �������˿�
#define INFTIM 1000


char des_key[] = "neteasy_ltzyhhhh";

const string REDIS_ONLINE_IPSET = "online_ipset";   // redis��������ߵ�ip����
const string db_name = "ltdb";  // sqlite���ݿ�洢����Ϣ��¼�����ݿ�
const string prem_table = "pst_mess";  // ���ü�¼
const string tmp_table = "tmp_mess";  // ��ʱ��¼

// sqlite����
sqlite3 *sqlite_db;
char *zErrMsg = 0;
//vector<TranMessage> sqlite_select_mess;  // ȫ�ֲ�ѯ����  ��sqlite�в��ҵ�����
char sqlite_select_mess[TMP_BUF_MAX * 2014];
int sqlite_select_mess_len;
//vector<int> sqlite_select_ids;  // ȫ�ֲ�ѯ�����ID  ��sqlite�в��ҵ�����
int sqlite_select_ids[TMP_BUF_MAX];
int sqlite_select_ids_num;  // ȫ�ֲ�ѯ��������
int sqlite_insert(const char *sql_chs, sqlite3 *db, char *zErrMsg);  // �������
string gen_insert_sql_chs(TranMessage *handle_mess);  // ����insert sql
string gen_selip_sql_chs(string _table_name, string _to_ip);  // ����select sql
string gen_delid_sql_chs(string _table_name);  // ����delete sql
int sqlite_query(const char *sql_chs, sqlite3 *db, char *zErrMsg);  // ��ѯ����
int sqlite_delete(const char *sql_chs, sqlite3 *db, char *zErrMsg);  // ɾ������



// socket�Ŀͻ��˶���
class ClientSock
{
private:
	int client_recv_sockfd;  // �ÿͻ���ʹ�õĶ�socket
	char client_msg_buf[CLIENT_BUF_MAX];  // �ͻ���ʹ�õĻ���
	int cur_msgbuf_len;  // ��ǰ��Ϣ����ĳ���

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

// socket��ip��ַ��ӳ��
mutex recv_mpip_mux;  // �����׽�����ip��port��ӳ�� ���ź���
map<int, string> recv_sockfd_mp_ip;

// ip��socket��ӳ��
//map<string, int> send_ip_mp_sockfd;
mutex recv_mpmx_mux;  // ����socket��mutex  ���ź���
map<int, mutex*> recv_sockfd_mp_mux;

// ������  ������id-��Աsockets
map<string, vector<string>> lt_room;


// ����socketӳ���ip port
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


// ����socketӳ���ip port
void my_insert_sockfd_map_recv(int _recvfd, char *_ip, int _port) {
	for (map<int, string>::iterator it = recv_sockfd_mp_ip.begin(); it != recv_sockfd_mp_ip.end(); it++) {
		if (strcmp(it->second.c_str(), _ip) == 0) {
			recv_sockfd_mp_ip.erase(it->first);
			break;
		}
	}
	recv_sockfd_mp_ip.insert(pair<int, string>(_recvfd, _ip));
}

// ȥ��socketӳ���ip port
void my_erase_sockfd_map_recv(int _recv_sockfd) {
	if (recv_sockfd_mp_ip.find(_recv_sockfd) == recv_sockfd_mp_ip.end()) {
		cout << "error: no socket_map_ip" << endl;
		return;
	}
	// �ҵ�_recv_sockfd��Ӧ��ip
	string _ip = recv_sockfd_mp_ip.find(_recv_sockfd)->second;
	recv_mpip_mux.lock();
	recv_sockfd_mp_ip.erase(_recv_sockfd);  // ɾ��socket
	printf("erase socket %d\n", _recv_sockfd);
	recv_mpip_mux.unlock();

	// �Ƴ�socket��mutex
	recv_mpmx_mux.lock();
	recv_sockfd_mp_mux.erase(_recv_sockfd);  // ɾ��socket
	printf("erase socket %d\n", _recv_sockfd);
	recv_mpmx_mux.unlock();

	printf("%s off-line\n", _ip.data());
}

// �����׽���Ϊ������
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


// ����һЩsocket
int listenfd, nfds, recvfd;


// ���е������ӵĿͻ���
vector<ClientSock*> read_clients;
// �ͻ��˶�����
mutex clients_mux;

// �������ڴ���accept��epollר�õ��ļ�������, ָ�����������������ΧΪ256 
int epfd = epoll_create(256);
// ����epoll_event�ṹ��ı���, ev����ע���¼�, events�������ڻش�Ҫ������¼�
struct epoll_event ev, events[20];

// �����Ϣ�Ķ���
safe_queue<TranMessage> mess_que;

// ����redis����
redisContext *redis_handle;

int handle_mess_que(int tid);
int handle_read_sock(int event_fd);
void test_lt_room(char *_ip);

int post110_num = 0;  // ��202.193.58.1110���ӵ�����

char tmp_buf[TMP_BUF_MAX];

string redis_cmd = "keys *";
socklen_t clilen;
// �����ַ�ṹ��   Э��ء�ip���˿�
struct sockaddr_in clientaddr;  // �ͻ���
struct sockaddr_in serveraddr;

// �����̳߳�
std::threadpool executor{ 16 };

int main()
{
	recv_sockfd_mp_ip.clear();
	while (!mess_que.empty())
		mess_que.try_pop();

	// ���� �������ӵ��׽���  listenfd���ڼ���
	listenfd = socket(AF_INET, SOCK_STREAM, 0);
	setnonblocking(listenfd);       // �����ڼ�����socket����Ϊ��������ʽ
	ev.data.fd = listenfd;          // ������Ҫ������¼���ص��ļ�������
	ev.events = EPOLLIN | EPOLLET;  // ����Ҫ������¼�����
	epoll_ctl(epfd, EPOLL_CTL_ADD, listenfd, &ev);     // ע��epoll�¼�  ע��listenfd��epfd��
	//bzero(&serveraddr, sizeof(serveraddr));
	memset(&serveraddr, 0, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	char *local_addr = "202.193.58.110"; 
	inet_aton(local_addr, &(serveraddr.sin_addr));  // ����ip������socket��
	serveraddr.sin_port = htons(SERV_PORT);  // ����port������socket��
	
	// �� �����׽����ϵĵ�ַ
	bind(listenfd, (sockaddr *)&serveraddr, sizeof(serveraddr));

	// ����
	listen(listenfd, LISTENQ);
	printf("listen...\n");

	// ��ʼ���ͻ���socket
	clilen = sizeof(clientaddr);

	// ����redis��������redisContext.err������Ϊ1��redisContext.errstr���������������Ϣ
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

	// ��sqllite���ݿ�
	int rc = sqlite3_open((db_name + string(".db")).c_str(), &sqlite_db);  // (db_name + string(".db")).c_str()
	if (rc) {
		close_socks();
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(sqlite_db));
		return 0;
	}
	else {
		fprintf(stderr, "Opened database successfully\n");
	}
	

	// ���̳߳���8����Ϣ�����߳�
	for (int i = 0; i < 8; i++) {
		executor.commit(handle_mess_que, i);
	}

	for (; ; )
	{

		nfds = epoll_wait(epfd, events, 20, 500);  // �ȴ�epoll�¼��ķ���  �ȴ�ע�ᵽepfd�е��׽���׼����
		for (int i = 0; i < nfds; i++)                 // �����������������¼�
		{
			if (events[i].data.fd == listenfd)      // �����¼������ӣ�
			{
				// �������ӵĶ��׽���
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

				setnonblocking(recvfd);           // �ѿͻ��˵�socket����Ϊ��������ʽ**
				
				// �����µ����ӿͻ���
				clients_mux.lock();
				read_clients.push_back(new ClientSock(recvfd));  // ���һ���µĿͻ���
				clients_mux.unlock();

				// socket(recv)��ipӳ��
				recv_mpip_mux.lock();
				my_insert_sockfd_map_recv(recvfd, con_ip, con_port);
				recv_mpip_mux.unlock();
				
				// socket(recv/send)��mutex��ӳ��
				recv_mpmx_mux.lock();  // map����
				mutex *recv_mutex = new mutex;
				recv_sockfd_mp_mux.emplace(recvfd, recv_mutex);
				recv_mpmx_mux.unlock();


				// �û�����
				// ip���µ�redis��
				string redis_cmd = "SADD online_ipset " + string(con_ip);
				redisReply *redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
				if (redis_reply == NULL) {  // replyΪnull��������ִ���������Ҳ�����ǳ�ʱ�䲻����redis���Ͽ�
					printf("Error:%s, command:%s\n", redis_handle->errstr, redis_cmd.c_str());
					redisReconnect(redis_handle);
					printf("redisReconnect...\n");
					redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
				}
				else {
					printf("%s %d connected, in redis\n", con_ip, con_port);
				}
				freeReplyObject(redis_reply);  // ʹ��reply�Ժ����Ҫ�����ͷ�

				// �����ݿ������
				sqlite_select_mess_len = 0;
				sqlite_select_ids_num = 0;
				string sql_str = gen_selip_sql_chs("tmp_mess", con_ip);
				if (sqlite_query(sql_str.c_str(), sqlite_db, zErrMsg) != 0) {
					cout << "select sqlite error!" << endl;
				}
				//cout << "cur sqlite_select_mess len: " << sqlite_select_mess_len << ", size: " << sqlite_select_ids_num << endl;

				recv_mutex->lock();  // ʹ�øղŴ����ķ���socket������ʷ��Ϣ
				//for (int _i = 0; _i < sqlite_select_mess_num; _i++) {
				//	usleep(2000);

				//	// ���ɷ��͵�����
				//	char sock_send_sock[TMP_BUF_MAX];  // ������Ϣ�õĻ���
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

				if (sqlite_select_ids_num > 0) {  // ������ʱ���ݿ�
					sql_str = gen_delid_sql_chs("tmp_mess");
					if (sqlite_delete(sql_str.c_str(), sqlite_db, zErrMsg) != 0) {
						cout << "delete sqlite error!" << endl;
					}
				}

				//ע��ev�¼�
				ev.data.fd = recvfd;                // �������ڶ��������ļ�������
				ev.events = EPOLLIN | EPOLLET;      // ��������ע�ض��������¼�
				epoll_ctl(epfd, EPOLL_CTL_ADD, recvfd, &ev);
			}

			// �����¼���   ����Ҫ���������ˣ�
			else if (events[i].events & EPOLLIN)
			{
				//printf("\nepoll EPOLLIN\n");

				if (events[i].data.fd < 0)
				{
					printf("events.data.fd < 0\n");
					continue;
				}
				int e_fd = events[i].data.fd;
				
				// ʹ���̳߳�  ����ɶ�socket
				executor.commit(handle_read_sock, e_fd);
			}
		}
	}


	// �ͷ�redis����
	redisFree(redis_handle);

	// �ر�sqlite���ݿ�
	sqlite3_close(sqlite_db);

	return 0;

}

// �ر��׽���
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
// ������¼�
int handle_read_sock(int event_fd) {

	if (recv_sockfd_mp_mux.find(event_fd) == recv_sockfd_mp_mux.end()) {
		cout << "recv error, no socket..." << endl;
		return -1;
	}

	mutex *t_recv_mux = recv_sockfd_mp_mux.find(event_fd)->second;  // ��ȡ�����׽��ֵĻ�����  Ҳ�ɵ����ͻ��˵���
	
	for (int j = read_clients.size() - 1; j >= 0; j--) {
		ClientSock *t_c_sock = read_clients[j];  // �����ӿͻ��˵�socket
		if (read_clients[j]->get_recv_socket() == event_fd) {
			
			int cur_sockfd = t_c_sock->get_recv_socket();
			// recv��Ҫwhile
			while (1) {
				//printf("\n\nrun recv...\n");
				t_recv_mux->lock();
				int recv_len = recv(cur_sockfd, sock_tmp_buf, sizeof(sock_tmp_buf), 0);
				t_recv_mux->unlock();

				//printf("sizeof sock_tmp_buf: %d, recv_len: %d\n", sizeof(sock_tmp_buf), recv_len);
				if (recv_len > 0) {  // ������������
					t_recv_mux->lock();
					// ��recv�����ݸ���client������
					memcpy(t_c_sock->get_msg_buf() + t_c_sock->get_cur_msgbuf_len(), sock_tmp_buf, recv_len);
					t_c_sock->set_msgbuf_len(t_c_sock->get_cur_msgbuf_len() + recv_len);
					//printf("msgbuf_len: %d\n", t_c_sock->get_cur_msgbuf_len());  // �ͻ��˱������ݵĻ���
					// ����client�����е�����
					while (t_c_sock->get_cur_msgbuf_len() > int(sizeof(TranMessage))) {
						TranMessage *recv_mh = (TranMessage *)t_c_sock->get_msg_buf();
						if (t_c_sock->get_cur_msgbuf_len() >= recv_mh->protocol_length) {

							// �õ���Ϣͷ�� handle_msg(t_c_sock->get_socket(), recv_mh);
							TranMessage *mess = new TranMessage();
							mess = recv_mh;
							
							// �������ݲ��� - ���ݸ��Ƶ���Ϣ�ռ���
							int _content_len = mess->protocol_length - mess->head_length;
							char *data_mess = new char[_content_len];
							data_mess[_content_len] = '\0';
							memcpy(data_mess, t_c_sock->get_msg_buf() + mess->head_length, mess->protocol_length - mess->head_length);
							//printf("data_mess before: %s \n", data_mess);
							
							// ����
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

							// �޸�ClientSock����Ϣ����
							int rem_size = t_c_sock->get_cur_msgbuf_len() - recv_mh->protocol_length;  // �ͻ��˻���ʣ��δ���������
							memmove(t_c_sock->get_msg_buf(), t_c_sock->get_msg_buf() + mess->protocol_length, rem_size);
							t_c_sock->set_msgbuf_len(rem_size);
						}
						else {
							break;
						}
					}
					t_recv_mux->unlock();
				}
				else if (recv_len == 0) {  // �Է��ر�����
					// ���˳���ip����redis���Ƴ�
					string close_ip;
					int close_port;
					if (!my_getpeername_recv(cur_sockfd, &close_ip, &close_port))
					{
						string redis_cmd = "SREM online_ipset " + close_ip;
						redisReply *redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
						if (redis_reply == NULL) {  // replyΪnull��������ִ���������Ҳ�����ǳ�ʱ�䲻����redis���Ͽ�
							printf("Error:%s, command:%s\n", redis_handle->errstr, redis_cmd.c_str());
							redisReconnect(redis_handle);
							printf("redisReconnect...\n");
							redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
						}
						else {
							printf("%s close, not in redis\n", close_ip.c_str());
						}
						freeReplyObject(redis_reply);  // ʹ��reply�Ժ����Ҫ�����ͷ�
					}
					else {
						printf("one connection closed, no ip\n");
					}

					printf("recv=0, obj %d close con\n", cur_sockfd);
					close(cur_sockfd);

					// ���cur_sockfd ��Ӧ��ip
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
						// ��sockfdrecv���ˣ�����ѭ��
						break;
					}
					else {
						string close_ip;
						int close_port;
						if (!my_getpeername_recv(cur_sockfd, &close_ip, &close_port))
						{
							string redis_cmd = "SREM online_ipset " + close_ip;
							redisReply *redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
							if (redis_reply == NULL) {  // replyΪnull��������ִ���������Ҳ�����ǳ�ʱ�䲻����redis���Ͽ�
								printf("Error:%s, command:%s\n", redis_handle->errstr, redis_cmd.c_str());
								redisReconnect(redis_handle);
								printf("redisReconnect...\n");
								redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
							}
							else {
								printf("%s close, not in redis\n", close_ip.c_str());
							}
							freeReplyObject(redis_reply);  // ʹ��reply�Ժ����Ҫ�����ͷ�
						}
						else {
							printf("recv<0, one connection closed, no ip\n");
						}

						// ����
						//close_socks();
						perror("recv < 0:");
						printf("%d error, %d\n", cur_sockfd, errno);
						printf("obj(sock)  %d con exit, no close\n", cur_sockfd);
						
						// �Ƴ�socket_map_�е�Ԫ��
						my_erase_sockfd_map_recv(cur_sockfd);
						close(cur_sockfd);

						// �Ƴ��ͻ���
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
	return 0;  // ��������  �߳̽���
}


// ������Ϣ���� ת����Ϣ
int handle_mess_que(int tid) {
	while (1) {

		TranMessage* handle_mess = mess_que.wait_and_pop();  // ����Ϣ���ն�����ȡ��Ϣ
		string to_ip = handle_mess->to.user_ip;
		//string to_ip = "202.193.58.110";
		//printf("mess from %s, to %s, content %s\n", handle_mess->from.user_ip, handle_mess->to.user_ip, handle_mess->message.content);

		// ��������
		if (handle_mess->message_type == SING_COMM) {
			// ����
			int _send_sock = -1;
			recv_mpip_mux.lock();
			for (map<int, string>::iterator it = recv_sockfd_mp_ip.begin(); it != recv_sockfd_mp_ip.end(); it++) {
				
				if (strcmp(it->second.c_str(), to_ip.c_str()) == 0) {
					_send_sock = it->first;
					//break;
				}
			}
			recv_mpip_mux.unlock();

			if (_send_sock == -1) {  // �Է�������
				
				// �����ݿ�
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
				mutex *t_send_mux = recv_sockfd_mp_mux.find(_send_sock)->second;  // �������ݵ�socket (�����socket��ͬһ��)
				t_send_mux->lock();

				// ���ɷ��͵�����
				char sock_send_sock[TMP_BUF_MAX];  // ������Ϣ�õĻ���
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
					else {  // ����ʧ��
						// ����Ͽ����ӵĽ��
						my_erase_sockfd_map_recv(_send_sock);

						// �����ݿ�
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
		else if (handle_mess->message_type == GUP_COMM) {  // Ⱥ��
			if (lt_room.find(handle_mess->to.user_ip) == lt_room.end()) {
				cout << "no room" << endl;
				continue;
			} 
			vector<string> lt_room_ips = lt_room.find(handle_mess->to.user_ip)->second;  // ������Ϣ��Ŀ�ĵ�ַ��������

			for (int _i = 0; _i < lt_room_ips.size(); _i++) {
				string _ip = lt_room_ips[_i];

				int _send_sock = -1;
				// �������������ж���ƥ�䷢��socket
				for (map<int, string>::iterator it = recv_sockfd_mp_ip.begin(); it != recv_sockfd_mp_ip.end(); it++) {  
					if (strcmp(it->second.c_str(), _ip.c_str()) == 0) {
						_send_sock = it->first;
						break;
					}
				}

				if (_send_sock == -1) {  // �����Ҷ�������
					// �����ݿ�
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
					// ʹ�÷���socket��������  ���뵥�����ƣ�
					if (recv_sockfd_mp_mux.find(_send_sock) == recv_sockfd_mp_mux.end()) {
						cout << "GUP send error, no socket..." << endl;
						break;
					}
					mutex *t_send_mux = recv_sockfd_mp_mux.find(_send_sock)->second;  // �������ݵ�socket 
					t_send_mux->lock();

					// ���ɷ��͵�����
					char sock_send_sock[TMP_BUF_MAX];  // ������Ϣ�õĻ���
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
						else {  // ����ʧ��
							// ����Ͽ����ӵĽ��
							my_erase_sockfd_map_recv(_send_sock);

							// �����ݿ�
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


// ���������ҹ���
void test_lt_room(char *_ip) {
	char room_id[] = "20219358110";
	vector<string> members;
	members.push_back("202.193.59.136");
	members.push_back("202.193.58.110");
	members.push_back("202.193.59.127");
	lt_room.insert(pair<string, vector<string>>(room_id, members));
}

// ����insert sql���
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

// ����select sql���
string gen_selip_sql_chs(string _table_name, string _to_ip)
{
	/*sql_chs = "SELECT * from tmp_record";*/
	string sql_str = "SELECT * from " + _table_name + " where to_ip='" + _to_ip + "'";
	cout << "select sql: " << sql_str << endl;
	return sql_str;
}


// ����delete sql���
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
	
