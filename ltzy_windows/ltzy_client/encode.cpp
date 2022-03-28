#include <iostream>

using namespace std;


/*
#include "encode.h"
int main1()
{
	string s = "ÄãºÃ";
	wstring un_str = ANSIToUnicode(s);
	//printf("%ws\n", un_str);
	wcout.imbue(locale("chs"));
	wcout << un_str.c_str() << endl;

	string utf_str = UnicodeToUTF8(un_str);

	cout << utf_str << endl;

	string ans_str = UTF8ToANSI(utf_str);
	cout << ans_str << endl;

	char s2[] = "ÖÐÎÄu";
	string utf_str2 = ANSIToUTF8(s2);
	char s3[100];
	for (int i = 0; i < utf_str2.length(); i++) {
		cout << "s:" << utf_str2[i] << endl;
		s3[i] = utf_str2[i];
	}
	s3[utf_str2.length()] = '\0';
	cout << utf_str2 << endl;
	string ans_str2 = UTF8ToANSI(s3);
	cout << ans_str2 << endl;

	return 0;
}
*/
