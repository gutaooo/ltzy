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

using namespace std;


#include "message_struct.h"

#define TMP_BUF_MAX 4096
#define CLIENT_BUF_MAX 4096 * 1024
#define OPEN_MAX 100
#define LISTENQ 20  // ����������󳤶�
#define SERV_PORT 12345  // �������˿�
#define INFTIM 1000


int main_serv()
{
	int listenfd, connfd;
	struct sockaddr_in seraddr;
	struct sockaddr_in cliaddr;
	char recv_buff[TMP_BUF_MAX] = {};
	int len;

	// ��������socket
	if (-1 == (listenfd = socket(PF_INET, SOCK_STREAM, 0)))  
	{
		cout << "create socket failed!" << endl;
		return 0;
	}
	else
	{
		cout << "create socket success!" << endl;
	}

	memset(&seraddr, 0, sizeof(seraddr));
	seraddr.sin_family = AF_INET;
	seraddr.sin_addr.s_addr = htonl(INADDR_ANY);
	seraddr.sin_port = htons(54321);

	// ����socket ��ip��ַ
	if (-1 == bind(listenfd, (struct sockaddr*)&seraddr, sizeof(seraddr)))
	{
		cout << "bind socket failed!" << endl;
		return 0;
	}
	else
	{
		cout << "bind socket success!" << endl;
	}

	// ����
	if (-1 == listen(listenfd, 10))
	{
		cout << "listen socket failed!" << endl;
		return 0;
	}
	else
	{
		cout << "listen socket success!" << endl;
	}

	// ����
	socklen_t clilen = sizeof(cliaddr);
	if (-1 == (connfd = accept(listenfd, (struct sockaddr*)&cliaddr, &clilen)))
	{
		cout << "accept socket failed!" << endl;
		return 0;
	}
	else
	{
		cout << "accept socket success!" << endl;
	}

	// һֱ��������
	while ((len = recv(connfd, recv_buff, TMP_BUF_MAX, 0) > 0))
	{
		printf("recv_buff: %s\n", recv_buff);
	}



	close(connfd);
	close(listenfd);
	return 0;
}

