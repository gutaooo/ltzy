#pragma once
#include <string>
using namespace std;

#include <hiredis/hiredis.h>

void redis_test()
{
	//����redis��������redisContext.err������Ϊ1��redisContext.errstr���������������Ϣ
	redisContext *redis_handle = redisConnect("127.0.0.1", 6379);
	if (redis_handle == NULL || redis_handle->err) {
		if (redis_handle) {
			printf("Error:%s\n", redis_handle->errstr);
		}
		else {
			printf("can't allocate redis context\n");
		}
		return ;
	}

	// ����redis
	// ����redisReply���󡢵���redisCommand����
	string redis_cmd = "SADD test_set test1";
	redisReply *redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
	freeReplyObject(redis_reply);  // ʹ��reply�Ժ����Ҫ�����ͷ�
	if (redis_reply == NULL) {  // replyΪnull�������ǳ�ʱ�䲻����redis���Ͽ�
		printf("Error:%s\n", redis_handle->errstr);
		redisReconnect(redis_handle);
	}
	else {
		string redis_cmd = "SMEMBERS test_set";
		redisReply *redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
		if (redis_reply->type == REDIS_REPLY_ARRAY) {
			for (int i = 0; i < redis_reply->elements; i++) {
				printf("element: %s\n", redis_reply->element[i]->str);
			}
		}
		else {
			printf("no\n");
		}
	}
	freeReplyObject(redis_reply);  // ʹ��reply�Ժ����Ҫ�����ͷ�


}