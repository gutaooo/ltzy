#include<iostream>
#include<winsock.h>
#include <thread>
#include <openssl/des.h>
#pragma comment(lib,"ws2_32.lib")
using namespace std;

//#include "encode.h"
#include "message_struct.h"
#include "utils.h"

extern char* G2U(const char* gb2312);
extern char* U2G(const char* utf8);

char _des_key[] = "neteasy_ltzyhhhh";

string enc_fun(char *src, char *keystring) {
	//char *keystring = new char[100];  // ����ռ�
	DES_cblock key;
	DES_key_schedule key_schedule;
	//����һ��key
	DES_string_to_key(keystring, &key);
	//�ж�����key_schedule�Ƿ�ɹ�
	if (DES_set_key_checked(&key, &key_schedule) != 0) {
		printf("key_schedule failed.\n");
		//return -1;
	}

	size_t len = (strlen(src) + 7) / 8 * 8;
	unsigned char *output = new unsigned char[len + 1];
	//���ú�������
	DES_cblock ivec;

	//IV����Ϊ0x0000000000000000
	memset((char*)&ivec, 0, sizeof(ivec));

	//����
	DES_ncbc_encrypt((unsigned char *)src, output, strlen(src), &key_schedule, &ivec, DES_ENCRYPT);//DES_ENCRYPT����1����ʾ����

	//��������Ժ������
	/*printf("���ܺ�Ľ����:");
	for (int i = 0; i < len; ++i)
		printf("%02x", output[i]);*/

	printf("src: ", output);
	printf("output: ", output);

	char *_s = (char *)output;
	string res = "";
	for (int i = 0; i < strlen(_s); i++) {
		res += _s[i];
	}
	return res;
}


string dec_fun(char *src, char *keystring) {
	//���н���
	DES_cblock key;
	DES_key_schedule key_schedule;
	DES_cblock ivec;
	size_t len = (strlen(src) + 7) / 8 * 8;
	unsigned char *output = new unsigned char[len + 1];

	//����һ��key
	DES_string_to_key(keystring, &key);
	//�ж�����key_schedule�Ƿ�ɹ�
	if (DES_set_key_checked(&key, &key_schedule) != 0) {
		printf("key_schedule failed.\n");
		//return -1;
	}

	//IV����Ϊ0x0000000000000000
	memset((char*)&ivec, 0, sizeof(ivec));
	//����
	DES_ncbc_encrypt((unsigned char *)src, (unsigned char *)output, len, &key_schedule, &ivec, 0);

	printf("src: ", output);
	printf("output: ", output);

	char *_s = (char *)output;
	string res = "";
	for (int i = 0; i < strlen(_s); i++) {
		res += _s[i];
	}

	return res;
}

string fun(char *src, char *keystring) {
	//char *keystring = new char[100];  // ����ռ�
	DES_cblock key;
	DES_key_schedule key_schedule;
	//����һ��key
	DES_string_to_key(keystring, &key);
	//�ж�����key_schedule�Ƿ�ɹ�
	if (DES_set_key_checked(&key, &key_schedule) != 0) {
		printf("key_schedule failed.\n");
		//return -1;
	}

	size_t len = (strlen(src) + 7) / 8 * 8;
	unsigned char *output = new unsigned char[len + 1];
	//���ú�������
	DES_cblock ivec;

	//IV����Ϊ0x0000000000000000
	memset((char*)&ivec, 0, sizeof(ivec));

	//����
	DES_ncbc_encrypt((unsigned char *)src, output, strlen(src), &key_schedule, &ivec, DES_ENCRYPT);//DES_ENCRYPT����1����ʾ����

	//��������Ժ������
	printf("���ܺ�Ľ����:");
	for (int i = 0; i < len; ++i)
		printf("%02x", output[i]);
	// printf("\n%d,%d,%d",len,strlen(output),strlen(input));//���ù���һ�仰
	printf("\n");


	//���н���
	DES_cblock key1;
	//����һ��key
	DES_string_to_key(keystring, &key);
	//�ж�����key_schedule�Ƿ�ɹ�
	if (DES_set_key_checked(&key, &key_schedule) != 0) {
		printf("key_schedule failed.\n");
		//return -1;
	}

	//IV����Ϊ0x0000000000000000
	memset((char*)&ivec, 0, sizeof(ivec));
	//����
	DES_ncbc_encrypt(output, (unsigned char *)src, len, &key_schedule, &ivec, 0);

	printf("���ܺ�Ľ���ǣ�%s\n", src);

	string res = "";
	for (int i = 0; i < strlen(src); i++) {
		res += src[i];
	}

	free(output);
	return res;
}


int _send_seq = 0;
void test() {
	char send_mess[4096];
	
	while (1) {
		string _s = "��������" + to_string(_send_seq);
		_send_seq += 1;

		for (int i = 0; i < _s.length(); i++) {
			send_mess[i] = _s[i];
		}
		send_mess[_s.length()] = '\0';  // 202.193.59.136:54321-���(windows->linux)--0
		printf("\n\nsend_mess: %d, %s\n", strlen(send_mess), send_mess);

		Sleep(100);
		//cout << "�����뷢����Ϣ:";
		//cin >> send_mess;

		// ת��
		char *t_sm = G2U(send_mess);
		printf("t_sm: %d, %s\n", strlen(t_sm), t_sm);
		//t_sm[strlen(t_sm)] = '\0';

		// ����
		string _cipherText = enc_fun(t_sm, _des_key);
		char *enc_str = new char[_cipherText.length()];
		memcpy(enc_str, _cipherText.c_str(), _cipherText.length());
		enc_str[strlen(enc_str)] = '\0';
		printf("enc_str: %d, %s\n", _cipherText.length(), _cipherText.c_str());

		// ����
		string _decryptText = dec_fun(enc_str, _des_key);
		char *_dec_str = new char[_decryptText.length()];
		memcpy(_dec_str, _decryptText.c_str(), _decryptText.length());
		//_dec_str[strlen(_dec_str)] = '\0';
		printf("dec_str: %d, %s\n", _decryptText.length(), _decryptText.c_str());
		printf("tmp dec_str: %d %s\n", strlen(_dec_str), _dec_str);
		// ת��
		char *_t_sm = U2G(_dec_str);
		_t_sm[strlen(_t_sm)] = '\0';
		printf("tmp _t_sm: %d, %s\n", strlen(_t_sm), _t_sm);
	}
	
}


int main_openssl(int argc, char** argv)
{
	test();
	return EXIT_SUCCESS;
}