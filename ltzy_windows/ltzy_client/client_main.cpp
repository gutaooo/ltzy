#include <iostream>
#include <winsock.h>
#include <thread>
#include <ctime>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

#include "encode.h"
#include "message_struct.h"
#include "utils.h"



void sock_noblocking(SOCKET s) {
	unsigned long ul = 1;

	int ret = ioctlsocket(s, FIONBIO, (unsigned long *)&ul);//���óɷ�����ģʽ�� 

	if (ret == SOCKET_ERROR)//����ʧ�ܡ�  
	{
		cout << "no block error!" << endl;
	}
}


void sock_initialization() {
	//��ʼ���׽��ֿ�
	WORD w_req = MAKEWORD(2, 2);//�汾��
	WSADATA wsadata;
	int err;
	err = WSAStartup(w_req, &wsadata);
	if (err != 0) {
		cout << "��ʼ���׽��ֿ�ʧ�ܣ�" << endl;
	}
	else {
		cout << "��ʼ���׽��ֿ�ɹ���" << endl;
	}
	//���汾��
	if (LOBYTE(wsadata.wVersion) != 2 || HIBYTE(wsadata.wHighVersion) != 2) {
		cout << "�׽��ֿ�汾�Ų�����" << endl;
		WSACleanup();
	}
	else {
		cout << "�׽��ֿ�汾��ȷ��" << endl;
	}
	//������˵�ַ��Ϣ

}


#define TMP_BUF_MAX 4096
#define CLIENT_BUF_MAX 4096 * 1024
#define OPEN_MAX 100
#define LISTENQ 20  // ����������󳤶�
#define SERV_PORT 12345  // �������˿�
#define INFTIM 1000

void recv_process();
void send_process();

SOCKET sendfd;
struct sockaddr_in seraddr;
struct sockaddr_in cliaddr;
char tmp_send_buf[TMP_BUF_MAX] = {};  // send����
char recv_buff[TMP_BUF_MAX] = {};  // recv����
int data_buff_cur_len = 0;
char data_buff[CLIENT_BUF_MAX] = {};  // �ڶ��㻺��

char des_key[] = "neteasy_ltzyhhhh";


//���峤�ȱ���
int send_len = 0;
int recv_len = 0;

int exit_flag = 0;

int post_num = 54321;


int connect_server() {
	//���������Ϣ
	seraddr.sin_family = AF_INET;
	seraddr.sin_addr.S_un.S_addr = inet_addr("202.193.58.110");  // ����ip   ��ip�ַ���ת������
	seraddr.sin_port = htons(12345);  // ����port   ��16λת��������

	printf("%s\n", inet_ntoa(seraddr.sin_addr));
	printf("%u\n", ntohs(seraddr.sin_port));

	//�����׽���
	sendfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sendfd == INVALID_SOCKET) {
		fprintf(stderr, " socket creation failed\n");
		return -1;
	}

	if (connect(sendfd, (SOCKADDR *)&seraddr, sizeof(SOCKADDR)) == SOCKET_ERROR) {
		printf("SOCKET_ERROR:%d\n", SOCKET_ERROR);
		printf("errno=%d\n", errno);
		cout << "����������ʧ�ܣ�" << endl;
		printf("WSAGetLastError:%d\n", WSAGetLastError());
		WSACleanup();
		return -1;
	}
	else {
		cout << "���������ӳɹ���" << endl;
	}

	return 0;
}


int main() 
{

	sock_initialization();
	
	if (connect_server() != 0) {
		cout << "connect server error!" << endl;
		printf("err info: %s, %d, %d\n", strerror(errno), errno, WSAGetLastError());
	}
	
	// �����������ݵ��߳�
	thread s(send_process);
	s.detach();
	printf("send_process success. \n");

	// �����������ݵ��߳�
	thread t(recv_process);
	t.detach();
	printf("recv_process success. \n");

	while (!exit_flag) {}

	//�ر��׽���
	closesocket(sendfd);
	//�ͷ�DLL��Դ
	WSACleanup();

	system("pause");
	return 0;
}

int send_seq = 0;


// ������Ϣ�߳�
void send_process() {
	char send_mess[400];  // �淢�͵�ʵ����Ϣ
	char to_ip[] = "20219358110";  // 202.193.58.110  20219358110
	char from_ip[] = "202.193.59.136";

	//����,��������
	while (1) {
		
		string _s = string(from_ip) + ":" + to_string(post_num) + "--���(win->linux)--" + to_string(send_seq);  
		//string _s = "he���" + to_string(send_seq);
		send_seq += 1;
		
		for (int i = 0; i < _s.length(); i++) {
			send_mess[i] = _s[i];
		}
		send_mess[_s.length()] = '\0';  // 202.193.59.136:54321-���(windows->linux)--0
		//printf("\n\nsend_mess: %d, %s\n", strlen(send_mess), send_mess);
		
		Sleep(100);
		cout << "����:";
		cin >> send_mess;

		if (strcmp(send_mess, "exit") == 0) {
			closesocket(sendfd);
			exit_flag = 1;
			break;
		}

		// ׼����������
		TranMessage cli_mess;
		cli_mess.message_type = GUP_COMM;  // �������췽ʽ
		
		memcpy(cli_mess.to.user_ip, to_ip, strlen(to_ip) + 1);
		memcpy(cli_mess.from.user_ip, from_ip, strlen(from_ip) + 1);
		time_t now = time(0);
		char* now_chs = ctime(&now);
		memcpy(cli_mess.message.send_time, now_chs, strlen(now_chs) + 1);
		cli_mess.from.user_port = post_num;
		
		// ת��
		char *t_sm = G2U(send_mess);
		//printf("t_sm: %d, %s\n", strlen(t_sm), t_sm);
		t_sm[strlen(t_sm)] = '\0';

		// ����
		string _cipherText = des_encrypt(t_sm, des_key);
		_cipherText[_cipherText.length()] = '\0';
		char *enc_str = new char[_cipherText.length() + 1];
		memcpy(enc_str, _cipherText.data(), _cipherText.length());
		enc_str[strlen(enc_str)] = '\0';
		//printf("enc_str: %d, %s\n", _cipherText.length(), _cipherText.c_str());

		
		// ����
		/*
		string _s_enc = enc_str;
		string _decryptText = des_decrypt(_s_enc, des_key);  // ����_cipherText  ����
		char *_dec_str = new char[_decryptText.length()];
		memcpy(_dec_str, _decryptText.c_str(), _decryptText.length());
		_dec_str[_decryptText.length()] = '\0';
		printf("dec_str: %d, %s\n", _decryptText.length(), _decryptText.c_str());
		printf("tmp dec_str: %d %s\n", strlen(_dec_str), _dec_str);
		printf("_decryptText: %s\n", _decryptText);
		// ת��
		char *_t_sm = U2G(_dec_str);
		_t_sm[strlen(_t_sm)] = '\0';
		printf("tmp _t_sm: %d, %s\n", strlen(_t_sm), _t_sm);
		*/
		

		//memcpy(cli_mess.message.content, _cipherText.c_str(), _cipherText.length() + 1);  // ����������Ϣ����Ϣ������
		cli_mess.message.content = enc_str;
		cli_mess.set_data_length();
		//printf("mess protocol_len: %d\n", cli_mess.protocol_length);
		
		// ���ɷ��͵�����
		char sock_send_sock[TMP_BUF_MAX];
		memcpy(sock_send_sock, (char *)(&cli_mess), cli_mess.head_length);  // ͷ��
		memcpy(sock_send_sock + cli_mess.head_length, enc_str, strlen(enc_str));  // ���ݲ���  cli_mess.message.content

		//printf("send mess: %s, %s\n", cli_mess.from.user_ip, sock_send_sock + cli_mess.head_length);

		send_len = send(sendfd, sock_send_sock, cli_mess.head_length + strlen(enc_str), 0);
		if (send_len < 0) {
			//if (errno == 0) continue;
			cout << "����ʧ��! ret: " << send_len << endl;
			printf("err info: %s, %d, %d\n", strerror(errno), errno, WSAGetLastError());
			break;
		}

	}
}

char sock_tmp_buf[TMP_BUF_MAX] = { 0 };
void recv_process() {
	
	// һֱ��������
	while (1) {
		int recv_len = recv(sendfd, sock_tmp_buf, TMP_BUF_MAX, 0);
		if (recv_len > 0) {  // ������������
			// ��recv�����ݸ���client������
			memcpy(data_buff + data_buff_cur_len, sock_tmp_buf, recv_len);  // ������memmove???
			data_buff_cur_len += recv_len;
			// ����client�����е�����
			while (data_buff_cur_len >= int(sizeof(TranMessage))) {
				TranMessage *recv_mh = (TranMessage *)data_buff;
				int old_protocol_length = recv_mh->protocol_length;
				if (data_buff_cur_len >= old_protocol_length) {
					// �����Ϣ���ݼ���
					TranMessage *mess = (TranMessage*)recv_mh;

					// �������ݲ��� - ���ݸ��Ƶ���Ϣ�ռ���
					int _len = mess->protocol_length - mess->head_length;
					char *data_mess = new char[_len + 1];
					memcpy(data_mess, data_buff + mess->head_length, _len);
					data_mess[_len] = '\0';

					// ����
					string decryptText = des_decrypt(data_mess, des_key);
					char *dec_str = new char[decryptText.length() + 1];
					memcpy(dec_str, decryptText.c_str(), decryptText.length());
					//dec_str[strlen(dec_str)] = '\0';
					//printf("dec_str: %s\n", dec_str);

					// �����������
					char *t_sm = U2G(dec_str);  // U2G(mess->message.content)
					t_sm[strlen(t_sm)] = '\0';
					mess->message.content = t_sm;
					mess->set_data_length();

					printf("\n����: from [%s], to [%s], time [%s], content [%s]\n", mess->from.user_ip, mess->to.user_ip, mess->message.send_time, mess->message.content);

					// �޸Ļ���
					int rem_size = data_buff_cur_len - old_protocol_length;  // ʣ��δ���������
					memmove(data_buff, data_buff + old_protocol_length, rem_size);
					data_buff_cur_len = rem_size;
				}
				else {
					break;
				}
			}
		}
		else if (recv_len == 0) {  // �Է��ر�����
			struct sockaddr_in obj_addr;
			int _len = sizeof(obj_addr);
			getpeername(sendfd, (struct sockaddr *)&obj_addr, &_len);
			char *close_ip = inet_ntoa(obj_addr.sin_addr);
			printf("obj close = %s:%d\n", close_ip, ntohs(obj_addr.sin_port));

			//closesocket(recvfd);
			break;
		}
		else {
			if (errno == 0) {
				//cout << "recv_ret == -1, errno == 0???" << endl;
			}
			else {
				printf("WSAGetLastError:%d\n", WSAGetLastError());
				if (recv_len == -1 && (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)) {
					// ��sockfd recv���ˣ�����ѭ��
					//break;
				}
				else if (recv_len == -1 && (errno == ENOBUFS))
				{
					cout << "no bufs......." << endl;
					//break;
				}
				else {
					// ����
					printf("obj con exit, no close\n");
					//closesocket(recvfd);
					perror("recv error: ");
					cout << "error: " << errno << endl;
					//break;
				}
			}
		}
	}
}