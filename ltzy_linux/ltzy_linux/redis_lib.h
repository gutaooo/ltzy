#pragma once
#include <string>
using namespace std;

#include <hiredis/hiredis.h>

void redis_test()
{
	//连接redis，若出错redisContext.err会设置为1，redisContext.errstr会包含描述错误信息
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

	// 操作redis
	// 创建redisReply对象、调用redisCommand函数
	string redis_cmd = "SADD test_set test1";
	redisReply *redis_reply = (redisReply *)redisCommand(redis_handle, redis_cmd.c_str());
	freeReplyObject(redis_reply);  // 使用reply以后必须要进行释放
	if (redis_reply == NULL) {  // reply为null，可能是长时间不连接redis而断开
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
	freeReplyObject(redis_reply);  // 使用reply以后必须要进行释放


}