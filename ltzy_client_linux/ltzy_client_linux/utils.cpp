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


// 加密 ecb模式    
string des_encrypt(const string &clearText, const string &key)
{
	string cipherText; // 密文    

	DES_cblock keyEncrypt;
	memset(keyEncrypt, 0, 8);

	// 构造补齐后的密钥    
	if (key.length() <= 8)
		memcpy(keyEncrypt, key.c_str(), key.length());
	else
		memcpy(keyEncrypt, key.c_str(), 8);

	// 密钥置换    
	DES_key_schedule keySchedule;
	DES_set_key_unchecked(&keyEncrypt, &keySchedule);

	// 循环加密，每8字节一次    
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
		// 加密函数    
		DES_ecb_encrypt(&inputText, &outputText, &keySchedule, DES_ENCRYPT);
		memcpy(tmp, outputText, 8);

		for (int j = 0; j < 8; j++)
			vecCiphertext.push_back(tmp[j]);
	}

	cipherText.clear();
	cipherText.assign(vecCiphertext.begin(), vecCiphertext.end());

	return cipherText;
}

// 解密 ecb模式    
string des_decrypt(const string &cipherText, const string &key)
{
	string clearText; // 明文    

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
		// 解密函数    
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
	//char *keystring = new char[100];  // 分配空间
	DES_cblock key;
	DES_key_schedule key_schedule;
	//生成一个key
	DES_string_to_key(keystring, &key);
	//判断生成key_schedule是否成功
	if (DES_set_key_checked(&key, &key_schedule) != 0) {
		printf("key_schedule failed.\n");
		//return -1;
	}

	size_t len = (strlen(src) + 7) / 8 * 8;
	unsigned char *output = new unsigned char[len + 1];
	//配置函数参数
	DES_cblock ivec;

	//IV设置为0x0000000000000000
	memset((char*)&ivec, 0, sizeof(ivec));

	//加密
	DES_ncbc_encrypt((unsigned char *)src, output, strlen(src), &key_schedule, &ivec, DES_ENCRYPT);//DES_ENCRYPT代表1，表示加密

	//输出加密以后的内容
	/*printf("加密后的结果是:");
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
	//进行解密
	DES_cblock key;
	DES_key_schedule key_schedule;
	DES_cblock ivec;
	size_t len = (strlen(src) + 7) / 8 * 8;
	unsigned char *output = new unsigned char[len + 1];

	//生成一个key
	DES_string_to_key(keystring, &key);
	//判断生成key_schedule是否成功
	if (DES_set_key_checked(&key, &key_schedule) != 0) {
		printf("key_schedule failed.\n");
		//return -1;
	}

	//IV设置为0x0000000000000000
	memset((char*)&ivec, 0, sizeof(ivec));
	//解密
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
