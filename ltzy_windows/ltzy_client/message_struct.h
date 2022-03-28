#ifndef _message_struct_h_
#define _message_struct_h_

#include <string>
#include <string.h>
using namespace std;

#pragma pack(8)

const int MAX_OBJ_NUM = 200;

// 消息类型
enum MessageType
{
	SING_COMM,  // 单聊
	GUP_COMM,   // 群聊

};


/* 实际消息：消息头部+用户信息+消息信息 */


// 消息头部
struct MessageHeader
{
	MessageHeader(int _len, int _type) : head_length(_len), message_type(_type) {}
	MessageHeader() {
		head_length = sizeof(MessageHeader);
		message_type = SING_COMM;
	}

	int head_length;  // 头部长度
	int protocol_length;  // 协议全部长度
	int message_type;  // 消息功能类型
};


// 用户信息
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
	char user_ip[36];  // 用户的ip/群聊的id
	char username[12];
	char passwd[12];
};


// 消息信息
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
	int mess_type;  // 消息类型
	char *content;  // 最多499个字符,249个汉字
};


// 传输消息  最多200个人
struct TranMessage : public MessageHeader
{
	TranMessage() {
		// 消息头
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

	// 发送方消息
	UserInfo from;
	// 接收方消息
	UserInfo to;

	// 消息本身
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