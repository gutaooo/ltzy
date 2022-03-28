#ifndef _UTILS_H_
#define _UTILS_H_
#include <string>

#pragma once


std::string des_encrypt(const std::string &clearText, const std::string &key);

std::string des_decrypt(const std::string &cipherText, const std::string &key);


unsigned char* des_encrypt_chs(char *clearText, char *key);

char* des_decrypt_chs(unsigned char *cipherText, char *key);


string enc_des(char *src, char *keystring);

string dec_des(char *src, char *keystring);

#endif