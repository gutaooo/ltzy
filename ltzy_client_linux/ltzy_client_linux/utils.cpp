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

	//��������Ժ������
	/*printf("���ܺ�Ľ����:");
	for (int i = 0; i < len; ++i)
		printf("%02x", output[i]);*/

		//printf("src: ", output);
		//printf("output: ", output);

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
