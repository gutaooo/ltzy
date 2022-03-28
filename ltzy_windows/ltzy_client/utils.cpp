#include <iostream>    
#include <cassert>  
#include <string>    
#include <vector>    
#include "openssl/md5.h"    
#include "openssl/sha.h"    
#include "openssl/des.h"    
#include "openssl/rsa.h"    
#include "openssl/pem.h" 
#include <string.h>
using namespace std;

#pragma comment(lib,"libssl.lib")
#pragma comment(lib,"libcrypto.lib")


// ���� ecbģʽ    
string des_encrypt(const string &clearText, const string &key)
{
	string cipherText; // ����    

	DES_cblock keyEncrypt;
	memset(keyEncrypt, 0, 8);

	// ���첹������Կ    
	if (key.length() <= 8)
		memcpy(keyEncrypt, key.c_str(), key.length());
	else
		memcpy(keyEncrypt, key.c_str(), 8);

	// ��Կ�û�    
	DES_key_schedule keySchedule;
	DES_set_key_unchecked(&keyEncrypt, &keySchedule);

	// ѭ�����ܣ�ÿ8�ֽ�һ��    
	const_DES_cblock inputText;
	DES_cblock outputText;
	vector<unsigned char> vecCiphertext;
	unsigned char tmp[8];

	for (int i = 0; i < clearText.length() / 8; i++)
	{
		memcpy(inputText, clearText.c_str() + i * 8, 8);
		DES_ecb_encrypt(&inputText, &outputText, &keySchedule, DES_ENCRYPT);
		memcpy(tmp, outputText, 8);

		for (int j = 0; j < 8; j++)
			vecCiphertext.push_back(tmp[j]);
	}

	if (clearText.length() % 8 != 0)
	{
		int tmp1 = clearText.length() / 8 * 8;
		int tmp2 = clearText.length() - tmp1;
		memset(inputText, 0, 8);
		memcpy(inputText, clearText.c_str() + tmp1, tmp2);
		// ���ܺ���    
		DES_ecb_encrypt(&inputText, &outputText, &keySchedule, DES_ENCRYPT);
		memcpy(tmp, outputText, 8);

		for (int j = 0; j < 8; j++)
			vecCiphertext.push_back(tmp[j]);
	}

	cipherText.clear();
	cipherText.assign(vecCiphertext.begin(), vecCiphertext.end());

	return cipherText;
}

// ���� ecbģʽ    
string des_decrypt(const string &cipherText, const string &key)
{
	string clearText; // ����    

	DES_cblock keyEncrypt;
	memset(keyEncrypt, 0, 8);

	if (key.length() <= 8)
		memcpy(keyEncrypt, key.c_str(), key.length());
	else
		memcpy(keyEncrypt, key.c_str(), 8);

	DES_key_schedule keySchedule;
	DES_set_key_unchecked(&keyEncrypt, &keySchedule);

	const_DES_cblock inputText;
	DES_cblock outputText;
	vector<unsigned char> vecCleartext;
	unsigned char tmp[8];

	for (int i = 0; i < cipherText.length() / 8; i++)
	{
		memcpy(inputText, cipherText.c_str() + i * 8, 8);
		DES_ecb_encrypt(&inputText, &outputText, &keySchedule, DES_DECRYPT);
		memcpy(tmp, outputText, 8);

		for (int j = 0; j < 8; j++)
			vecCleartext.push_back(tmp[j]);
	}

	if (cipherText.length() % 8 != 0)
	{
		int tmp1 = cipherText.length() / 8 * 8;
		int tmp2 = cipherText.length() - tmp1;
		memset(inputText, 0, 8);
		memcpy(inputText, cipherText.c_str() + tmp1, tmp2);
		// ���ܺ���    
		DES_ecb_encrypt(&inputText, &outputText, &keySchedule, DES_DECRYPT);
		memcpy(tmp, outputText, 8);

		for (int j = 0; j < 8; j++)
			vecCleartext.push_back(tmp[j]);
	}

	clearText.clear();
	clearText.assign(vecCleartext.begin(), vecCleartext.end());

	return clearText;
}


string enc_des(char *src, char *keystring) {
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

	char *_s = (char *)output;
	string res = "";
	for (int i = 0; i < strlen(_s); i++) {
		res += _s[i];
	}
	return res;
}


string dec_des(char *src, char *keystring) {
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

	//printf("src: ", output);
	//printf("output: ", output);

	char *_s = (char *)output;
	string res = "";
	for (int i = 0; i < strlen(_s); i++) {
		res += _s[i];
	}

	return res;
}


// ���� ecbģʽ      string des_encrypt_chs(const string &clearText, const string &key)
unsigned char* des_encrypt_chs(char *clearText, char *key)
{  

	DES_cblock keyEncrypt;
	memset(keyEncrypt, 0, 8);

	// ���첹������Կ    
	if (strlen(key) <= 8)
		memcpy(keyEncrypt, key, strlen(key));
	else
		memcpy(keyEncrypt, key, 8);

	// ��Կ�û�    
	DES_key_schedule keySchedule;
	DES_set_key_unchecked(&keyEncrypt, &keySchedule);

	// ѭ�����ܣ�ÿ8�ֽ�һ��    
	const_DES_cblock inputText;
	DES_cblock outputText;
	vector<unsigned char> vecCiphertext;
	unsigned char tmp[8];

	for (int i = 0; i < strlen(clearText) / 8; i++)
	{
		memcpy(inputText, clearText + i * 8, 8);
		DES_ecb_encrypt(&inputText, &outputText, &keySchedule, DES_ENCRYPT);
		memcpy(tmp, outputText, 8);

		for (int j = 0; j < 8; j++)
			vecCiphertext.push_back(tmp[j]);
	}

	if (strlen(clearText) % 8 != 0)
	{
		int tmp1 = strlen(clearText) / 8 * 8;
		int tmp2 = strlen(clearText) - tmp1;
		memset(inputText, 0, 8);
		memcpy(inputText, clearText + tmp1, tmp2);
		// ���ܺ���    
		DES_ecb_encrypt(&inputText, &outputText, &keySchedule, DES_ENCRYPT);
		memcpy(tmp, outputText, 8);

		for (int j = 0; j < 8; j++)
			vecCiphertext.push_back(tmp[j]);
	}

	unsigned char *cipherText = new unsigned char[vecCiphertext.size()]; // ����  

	for (int i = 0; i < vecCiphertext.size(); i++) {
		cipherText[i] = vecCiphertext[i];
	}

	return cipherText;
}


// ���� ecbģʽ     string des_decrypt(const string &cipherText, const string &key)
char* des_decrypt_chs(unsigned char *cipherText, char *key)
{   

	DES_cblock keyEncrypt;
	memset(keyEncrypt, 0, 8);

	if (strlen(key) <= 8)
		memcpy(keyEncrypt, key, strlen(key));
	else
		memcpy(keyEncrypt, key, 8);

	DES_key_schedule keySchedule;
	DES_set_key_unchecked(&keyEncrypt, &keySchedule);

	const_DES_cblock inputText;
	DES_cblock outputText;
	vector<unsigned char> vecCleartext;
	unsigned char tmp[8];

	int _len = strlen((char *)cipherText);

	for (int i = 0; i < _len / 8; i++)
	{
		memcpy(inputText, cipherText + i * 8, 8);
		DES_ecb_encrypt(&inputText, &outputText, &keySchedule, DES_DECRYPT);
		memcpy(tmp, outputText, 8);

		for (int j = 0; j < 8; j++)
			vecCleartext.push_back(tmp[j]);
	}

	if (_len % 8 != 0)
	{
		int tmp1 = _len / 8 * 8;
		int tmp2 = _len - tmp1;
		memset(inputText, 0, 8);
		memcpy(inputText, cipherText + tmp1, tmp2);
		// ���ܺ���    
		DES_ecb_encrypt(&inputText, &outputText, &keySchedule, DES_DECRYPT);
		memcpy(tmp, outputText, 8);

		for (int j = 0; j < 8; j++)
			vecCleartext.push_back(tmp[j]);
	}

	char *clearText = new char[vecCleartext.size()];  // ����

	for (int i = 0; i < vecCleartext.size(); i++) {
		clearText[i] = vecCleartext[i];
	}



	return clearText;
}