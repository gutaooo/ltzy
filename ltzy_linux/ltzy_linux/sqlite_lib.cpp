#include <stdio.h>
#include <sqlite3.h>
#include <string>
#include <vector>
using namespace std;

#define TMP_BUF_MAX 4096

#include "message_struct.h"
#include "safe_queue.h"


const string db_name = "ltdb";
const string prem_table = "pst_mess";
const string tmp_table = "tmp_mess";

extern char sqlite_select_mess[TMP_BUF_MAX];  // 全局查询对象  从sqlite中查找的数据
extern int sqlite_select_mess_len;
extern int sqlite_select_ids_num;
extern int sqlite_select_ids[TMP_BUF_MAX];

TranMessage t;
static int callback(void *NotUsed, int argc, char **argv, char **azColName) {
	
	/*for (int i = 0; i < argc; i++) {
		printf("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
	}
	printf("\n");*/

	sqlite_select_ids[sqlite_select_ids_num++] = atoi(argv[0]);  

	// 将数据全部拷贝到字符数组中
	TranMessage _t_mess;
	_t_mess.head_length = sizeof(TranMessage);
	_t_mess.message_type = atoi(argv[1]);  // MESSAGE_TYPE
	int potocol_length = _t_mess.head_length + strlen(argv[2]);  // 设置数据总长度
	_t_mess.message.content = argv[2];
	_t_mess.protocol_length = potocol_length;  // potocol_length

	memcpy(_t_mess.message.send_time, argv[3], strlen(argv[3]) + 1);  // TIME

	memcpy(_t_mess.from.username, argv[4], strlen(argv[4]) + 1);  // FROM_USER
	memcpy(_t_mess.from.user_ip, argv[5], strlen(argv[5]) + 1);  // FROM_IP
	_t_mess.from.user_port = atoi(argv[6]);  // FROM_PORT

	memcpy(_t_mess.to.username, argv[7], strlen(argv[7]) + 1);  // FROM_USER
	memcpy(_t_mess.to.user_ip, argv[8], strlen(argv[8]) + 1);  // FROM_IP
	_t_mess.to.user_port = atoi(argv[9]);  // FROM_PORT

	// TranMessage对象写到字符串数组中
	memcpy(sqlite_select_mess + sqlite_select_mess_len, (char *)(&_t_mess), _t_mess.head_length);
	sqlite_select_mess_len += _t_mess.head_length;
	memcpy(sqlite_select_mess + sqlite_select_mess_len, argv[2], strlen(argv[2]));
	sqlite_select_mess_len += strlen(argv[2]);

	return 0;
}


void create_table(char *sql_chs, sqlite3 *db, char *zErrMsg) {
	// 创建持久化表  (ID，消息类型，发送方信息，接收方信息，处理时间,消息内容)
	sql_chs = "CREATE TABLE pst_mess("  \
		"ID             INTEGER PRIMARY KEY  AUTOINCREMENT," \
		"MESSAGE_TYPE   INT," \
		"CONTENT        TEXT," \
		"TIME           CHAR(50)," \
		"FROM_USER      CHAR(50)," \
		"FROM_IP        CHAR(50)," \
		"FROME_PORT     INT," \
		"TO_USER        CHAR(50)," \
		"TO_IP          CHAR(50),"\
		"TO_PORT        INT)";
	int rc = sqlite3_exec(db, sql_chs, callback, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
	else {
		fprintf(stdout, "Table created successfully\n");
	}

	// 创建暂存表
	sql_chs = "CREATE TABLE tmp_mess("  \
		"ID             INTEGER PRIMARY KEY  AUTOINCREMENT," \
		"MESSAGE_TYPE   INT," \
		"CONTENT        TEXT," \
		"TIME           CHAR(50)," \
		"FROM_USER      CHAR(50)," \
		"FROM_IP        CHAR(50)," \
		"FROME_PORT     INT," \
		"TO_USER        CHAR(50)," \
		"TO_IP          CHAR(50),"\
		"TO_PORT        INT)";
	rc = sqlite3_exec(db, sql_chs, callback, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
	else {
		fprintf(stdout, "Table created successfully\n");
	}
}


int sqlite_insert(const char *sql_chs, sqlite3 *db, char *zErrMsg) {
	/*sql_chs = "INSERT INTO tmp_record(ID, CONTENT, SEND_TIME, RECV_TIME, FROM_IP, FROME_PORT, TO_IP, TO_PORT, STATE)"  \
		"VALUES(2, 'hello', '2020-3-10', '2020-3-10', '0.0.0.0', 12345, '0.0.0.0', 12345, 1)";*/
	
	int rc = sqlite3_exec(db, sql_chs, callback, 0, &zErrMsg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return -1;
	}
	else {
		fprintf(stdout, "Records created successfully\n");
		return 0;
	}
}

int sqlite_query(const char *sql_chs, sqlite3 *db, char *zErrMsg) {
	/* Create SQL statement */
	/*sql_chs = "SELECT * from tmp_record";*/

	//sqlite_select_mess.clear();
	//sqlite_select_ids.clear();
	const char* data = "Callback function called";
	/* Execute SQL statement */
	int rc = sqlite3_exec(db, sql_chs, callback, (void*)data, &zErrMsg);
	printf("query done.\n");
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return -1;
	}
	else {
		fprintf(stdout, "Operation done successfully\n");
		return 0;
	}
}

void sqlite_update(char *sql_chs, sqlite3 *db, char *zErrMsg) {
	/* Create merged SQL statement */
	sql_chs = "UPDATE tmp_record set send_time = '2020-3-6' where ID=1; ";

	/* Execute SQL statement */
	const char* data = "Callback function called";
	int rc = sqlite3_exec(db, sql_chs, callback, (void*)data, &zErrMsg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
	}
	else {
		fprintf(stdout, "Operation done successfully\n");
	}
}


int sqlite_delete(const char *sql_chs, sqlite3 *db, char *zErrMsg) {
	/* Create merged SQL statement */
	/*sql_chs = "DELETE from tmp_record where ID=2;";*/
	char *data = "DELETE DATA";
	/* Execute SQL statement */
	int rc = sqlite3_exec(db, sql_chs, callback, (void*)data, &zErrMsg);
	if (rc != SQLITE_OK) {
		fprintf(stderr, "SQL error: %s\n", zErrMsg);
		sqlite3_free(zErrMsg);
		return -1;
	}
	else {
		fprintf(stdout, "Operation done successfully\n");
		return 0;
	}
}

int main_sqlite(int argc, char* argv[])
{
	sqlite3 *sqlit_db;  // sqlite3数据库对象
	char *sql_chs;
	char *zErrMsg = 0;

	// 打开数据库
	int rc = sqlite3_open((db_name + string(".db")).c_str(), &sqlit_db);  // (db_name + string(".db")).c_str()   /home/owlh/Desktop/gutao/sqlite3/db/test.db
	if (rc) {
		fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(sqlit_db));
		return 0;
	}
	else {
		fprintf(stderr, "Opened database successfully\n");
	}

	create_table(sql_chs, sqlit_db, zErrMsg);

	sqlite3_close(sqlit_db);

	printf("ok\n");

	return 0;
}