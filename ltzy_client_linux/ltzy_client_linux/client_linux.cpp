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
#include <ctime>

using namespace std;

#include "message_struct.h"
#include "utils.h"

#define TMP_BUF_MAX 4096
#define CLIENT_BUF_MAX 4096 * 1024
#define OPEN_MAX 100
#define LISTENQ 20  // 监听队列最大长度
#define SERV_PORT 12345  // 服务器端口
#define INFTIM 1000

void recv_process();
void send_process();

int sendfd;
struct sockaddr_in seraddr;
struct sockaddr_in cliaddr;
char recv_buff[TMP_BUF_MAX * 100] = {};  // recv缓存
int data_buff_cur_len = 0;
char data_buff[CLIENT_BUF_MAX] = {};  // 第二层缓存

int send_len, recv_len;
int exit_flag = 0;

char *ip_str = "202.193.58.110";

char des_key[] = "neteasy_ltzyhhhh";

int post_num = 54321;

// 关闭套接字的函数
void close_sockfd();

// 连接服务器
int connect_server() {
	struct sockaddr_in  servaddr;
	// 创建套接字
	if ((sendfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("create socket error: %s(errno: %d)\n", strerror(errno), errno);
		return -1;
	}

	// 设置ip、端口到中
	memset(&servaddr, 0, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(12345);  // 设置端口   (主机序转网络序)
	if (inet_pton(AF_INET, ip_str, &servaddr.sin_addr) <= 0) {  // 设置ip   （ip字符串转网络序）
		printf("inet_pton error for %s\n", ip_str);
		close_sockfd();
		return -1;
	}

	if (connect(sendfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
		printf("connect error: %s(errno: %d)\n", strerror(errno), errno);
		close_sockfd();
		return -1;
	}
	else {
		printf("connect %s successul\n", inet_ntoa(servaddr.sin_addr));
	}
	return 0;
}




int main(int argc, char** argv) 
{

	// 连接服务器
	if (connect_server() != 0) {
		perror("connect server error!");
	}

	// 创建发送数据的线程
	thread s(send_process);
	s.detach();
	printf("send_process success. \n");

	// 创建接收数据的线程
	thread t(recv_process);
	t.detach();
	printf("recv_process success. \n");

	while (!exit_flag) {}
	

	close_sockfd();

	system("pause");
	return 0;
}

void send_process() {
	int send_seq = 0;
	char from_ip[] = "202.193.58.110";

	while (1) {
		usleep(100000);
		
		string _s = string(from_ip) + ":" + to_string(post_num) + "--hel(linux->windows)--" + to_string(send_seq);
		send_seq += 1;
		char send_mess[400];  // 存发送的实际信息
		for (int i = 0; i < _s.length(); i++) {
			send_mess[i] = _s[i];
		}
		send_mess[_s.length()] = '\0';
		//printf("\n\nsend_mess: %d, %s\n", strlen(send_mess), send_mess);
		cout << "input:";
		cin >> send_mess;

		if (strcmp(send_mess, "exit") == 0) {
			close_sockfd();
			exit_flag = 1;
			break;
		}

		// 准备发送数据
		TranMessage cli_mess;
		cli_mess.message_type = GUP_COMM;  // 设置聊天方式
		char to_ip[] = "20219358110";  // 20219358110  202.193.59.136
		memcpy(cli_mess.to.user_ip, to_ip, strlen(to_ip) + 1);
		memcpy(cli_mess.from.user_ip, from_ip, strlen(from_ip) + 1);
		time_t now = time(0);
		char* now_chs = ctime(&now);
		memcpy(cli_mess.message.send_time, now_chs, strlen(now_chs) + 1);
		
		// 加密
		string _cipherText = des_encrypt(send_mess, des_key);
		//char *enc_str = new char[strlen(_cipherText.c_str()) + 1];
		//printf("enc len: %d %d\n", strlen(_cipherText.c_str()), _cipherText.length());
		char *enc_str = new char[_cipherText.length() + 1];
		memcpy(enc_str, _cipherText.c_str(), _cipherText.length());
		enc_str[_cipherText.length()] = '\0';
		//printf("_cipherText: %d, %s\n", _cipherText.length(), _cipherText.c_str());
		//printf("enc_str: %d, %s\n", strlen(enc_str), enc_str);


		// 解密
		/*
		string str_enc_str = enc_str;
		string _decryptText = des_decrypt(str_enc_str, des_key);
		char *_dec_str = new char[_decryptText.length()];
		memcpy(_dec_str, _decryptText.c_str(), _decryptText.length());
		_dec_str[_decryptText.length()] = '\0';
		printf("dec_str: %d, %s\n", _decryptText.length(), _decryptText.c_str());
		printf("tmp dec_str: %d %s\n", strlen(_dec_str), _dec_str);
		*/


		//memcpy(cli_mess.message.content, send_mess, strlen(send_mess) + 1);
		cli_mess.message.content = enc_str;
		cli_mess.set_data_length();  // 更新数据部分的长度

		// 生成发送的数据
		char sock_send_sock[TMP_BUF_MAX];
		memcpy(sock_send_sock, (char *)(&cli_mess), cli_mess.head_length);
		memcpy(sock_send_sock + cli_mess.head_length, cli_mess.message.content, strlen(cli_mess.message.content));

		//printf("send_mess: %d, %s\n", strlen(send_mess), send_mess);
		//printf("send content: %s %s\n", cli_mess.from.user_ip, cli_mess.message.content);

		send_len = send(sendfd, sock_send_sock, cli_mess.protocol_length, 0);
		if (send_len < 0) {
			cout << "send error! ret_len: " << send_len << " " << errno << endl;
			perror("send error: ");
			close_sockfd();
			break;
		}

	}
}


char sock_tmp_buf[TMP_BUF_MAX];
void recv_process() {
	
	// 一直接收数据
	while (1) {
		int recv_len = recv(sendfd, sock_tmp_buf, sizeof(sock_tmp_buf), 0);
		//printf("sizeof recv_buf: %d, recv_len: %d\n", sizeof(sock_tmp_buf), recv_len);
		if (recv_len > 0) {  // 正常读到数据
			// 将recv的数据复制client缓存中
			memcpy(data_buff + data_buff_cur_len, sock_tmp_buf, recv_len);
			data_buff_cur_len += recv_len;
			// 处理client缓存中的数据
			//printf("recv len: %d %d\n", recv_len, int(sizeof(TranMessage)));
			while (data_buff_cur_len >= int(sizeof(TranMessage))) {
				TranMessage *recv_mh = (TranMessage *)data_buff;
				//printf("recv_mh->protocol_length: %d\n", recv_mh->protocol_length);
				int old_protocol_length = recv_mh->protocol_length;
				if (data_buff_cur_len >= old_protocol_length) {
					// 输出消息内容即可
					TranMessage *mess = (TranMessage*)recv_mh;
					
					// 处理数据部分 - 数据复制到消息空间中
					int _len = mess->protocol_length - mess->head_length;
					char *data_mess = new char[_len + 1];  // +1?
					memcpy(data_mess, data_buff + mess->head_length, mess->protocol_length - mess->head_length);  // +1?
					data_mess[_len] = '\0';
					
					//printf("data_mess: %d %s\n", strlen(data_mess), data_mess);

					// 解密
					string decryptText = des_decrypt(data_mess, des_key);
					char *dec_str = new char[decryptText.length() + 1];
					memcpy(dec_str, decryptText.c_str(), decryptText.length());
					dec_str[decryptText.length()] = '\0';

					//printf("dec_str: %d %s\n", strlen(dec_str), dec_str);

					mess->message.content = dec_str;
					mess->set_data_length();
					
					//memcpy(mess->message.content, decryptText.c_str(), decryptText.length() + 1);
					printf("\nrecv mess: from [%s], to [%s], time [%s], content [%s]\n", mess->from.user_ip, mess->to.user_ip, mess->message.send_time, mess->message.content);

					// 修改缓存
					int rem_size = data_buff_cur_len - old_protocol_length;  // 剩下未处理的数据
					memmove(data_buff, data_buff + old_protocol_length, rem_size);
					data_buff_cur_len = rem_size;
				}
				else {
					//close_sockfd();
					break;
				}
			}
		}
		else if (recv_len == 0) {  // 对方关闭连接
			struct sockaddr_in obj_addr;
			int _len = sizeof(obj_addr);
			getpeername(sendfd, (struct sockaddr *)&obj_addr, (socklen_t *)&_len);
			char *close_ip = inet_ntoa(obj_addr.sin_addr);
			printf("obj close = %s:%d\n", close_ip, ntohs(obj_addr.sin_port));

			close_sockfd();
			break;
		}
		else {
			if (recv_len == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
				// 把sockfdrecv完了，跳出循环
				break;
			}
			else {
				// 出错
				printf("obj con exit, no close\n");
				perror("recv error: ");
				cout << "error: " << errno << endl;
				close_sockfd();
				break;
			}
		}
	}
}


// 关闭套接字的函数
void close_sockfd() {
	close(sendfd);
}