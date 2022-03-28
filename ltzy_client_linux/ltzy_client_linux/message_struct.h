#ifndef _message_struct_h_
#define _message_struct_h_

#include <string>
#include <string.h>
using namespace std;

#pragma pack(8)

const int MAX_OBJ_NUM = 200;

// ��Ϣ����
enum MessageType
{
	SING_COMM,  // ����
	GUP_COMM,   // Ⱥ��

};


/* ʵ����Ϣ����Ϣͷ��+�û���Ϣ+��Ϣ��Ϣ */


// ��Ϣͷ��
struct MessageHeader
{
	MessageHeader(int _len, int _type) : head_length(_len), message_type(_type) {}
	MessageHeader() {
		head_length = sizeof(MessageHeader);
		message_type = SING_COMM;
	}

	int head_length;  // ͷ������
	int protocol_length;  // Э��ȫ������
	int message_type;  // ��Ϣ��������
};


// �û���Ϣ
struct UserInfo
{
	UserInfo(char _ip[], int _port) {
		memcpy(user_ip, _ip, strlen(_ip) + 1);
		user_port = _port;
	}
	UserInfo() {
		char _ip[] = "1.1.1.1";
		memcpy(user_ip, _ip, strlen(_ip) + 1);
		user_port = 0;
		char _null[] = "null";
		memcpy(username, _null, strlen(_null) + 1);
		memcpy(passwd, _null, strlen(_null) + 1);
	}

	int user_port;
	char user_ip[36];  // �û���ip/Ⱥ�ĵ�id
	char username[12];
	char passwd[12];
};


// ��Ϣ��Ϣ
struct MessInfo
{
	MessInfo(int _type, char _time[], char _content[]) {
		mess_type = _type;
		memcpy(send_time, _time, strlen(_time) + 1);
		content = _content;
	}

	MessInfo() {
		mess_type = SING_COMM;
		char _st[] = "2022-3-13 10:26";
		memcpy(send_time, _st, strlen(_st) + 1);
		content = new char(10);
	}

	char send_time[48];
	int mess_type;  // ��Ϣ����
	char *content;  // ���499���ַ�,249������
};


// ������Ϣ  ���200����
struct TranMessage : public MessageHeader
{
	TranMessage() {
		// ��Ϣͷ
		head_length = sizeof(TranMessage);
		message.content = new char(10);
	}

	TranMessage(const TranMessage &_t) {
		printf("TranMessage con\n");
		head_length = _t.head_length;
		protocol_length = _t.protocol_length;
		message_type = _t.message_type;

		from = _t.from;
		to = _t.to;

		message.mess_type = _t.message.mess_type;
		memcpy(message.send_time, _t.message.send_time, strlen(_t.message.send_time));
		message.content = new char(strlen(_t.message.content));
		memcpy(message.content, _t.message.content, strlen(_t.message.content));
	}

	// ���ͷ���Ϣ
	UserInfo from;
	// ���շ���Ϣ
	UserInfo to;

	// ��Ϣ����
	MessInfo message;

	string to_string() {
		printf("username:\n");
		string _s = "TranMessage: ";
		_s += "from: " + string(this->from.username) + "-" + string(this->from.user_ip) + ", ";
		_s += "to: " + string(this->to.username) + "-" + string(this->to.user_ip) + ", ";
		_s += "time: " + string(this->message.send_time) + ", ";
		_s += "content: " + string(this->message.content);
		return _s;
	}

	void set_data_length() {
		int _len = head_length + strlen(message.content);
		protocol_length = _len;
	}

};

#endif