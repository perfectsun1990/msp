/*************************************************************************
* @file   : EncodeDecode.c
* @author : sun
* @mail   : perfectsun1990@163.com
* @version: v1.0.0
* @date   : 2018年08月02日 星期四 10时26分12秒
* Copyright (C), 1990-2020, Tech.Co., Ltd. All rights reserved.
************************************************************************/

#ifdef WIN32
#include <windows.h>
#include <sys/stat.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>

#include "encrypt.hpp"

static const char header[10] = { 'E','N','C','O','D','E','D','C','C','R' };

int32_t
GetSize(FILE* fp, const char* ft)
{
	if (NULL != fp) {
		struct stat s;
		if (0 == fstat(fileno(fp), &s))
			return s.st_size;
		return -1;
	}
	if (NULL != ft) {
		struct stat s;
		if (0 == stat(ft, &s))
			return s.st_size;
		return -2;
	}
	return -3;
}

bool
IsEncodeSource(const char* src)
{
	char buffer[sizeof(header)] = { 0 };
	FILE *fp = NULL;
	if (NULL != (fp = fopen(src, "rb"))) {
		fread(buffer, sizeof(char), sizeof(header), fp);
		fclose(fp);
		if (!strncmp(buffer, header, sizeof(header)))
			return true;
	}
	return false;
}

bool
DoEncodeDecode(const char* src, const char* dst)
{
	if (NULL == src || NULL == dst)
		return false;

	bool ret = false;
	static const char *pwdstr = "5d597e14144b336d94cf3d0e05158887e6d1fc48e957deff370768073cf40177";
	int32_t pwdlen = strlen(pwdstr);
	char buffer[1024] = { 0 };

	FILE *fp1 = fopen(src, "rb");
	FILE *fp2 = fopen(dst, "wb");
	if (NULL == fp1 || NULL == fp2)
		return false;

	// prev purse
	if (IsEncodeSource(src)) {//解密
		fseek(fp1, sizeof(header), SEEK_SET);
	}
	else {//加密
		fwrite(header, sizeof(char), sizeof(header), fp2);
	}
	// data codec
	int32_t pwdidx = 0;
	char ch = 0;
	do {	//算法处理
		fread(&ch, sizeof(char), 1, fp1);
		ch = ch^pwdstr[pwdidx++%pwdlen];
		fwrite(&ch, sizeof(char), 1, fp2);/*异或后写入fp2文件*/
	} while (pwdidx < 2*pwdlen);
	while (!feof(fp1))
	{	//快速拷贝
		int32_t rsize = fread(buffer, sizeof(char), sizeof(buffer), fp1);
		if (rsize > 0)
			fwrite(buffer, sizeof(char), rsize, fp2);
	}
	if (NULL != fp1) fclose(fp1);
	if (NULL != fp2) fclose(fp2);

	return true;
}

// 仅加密.
bool DoEncode(const char* src, const char* dst)
{
	if (IsEncodeSource(src))
		return true;
	return DoEncodeDecode(src, dst);
}

// 仅解密. src是未加密文件返回失败。
bool DoDecode(const char* src, const char* dst)
{
	if (IsEncodeSource(src))
		return DoEncodeDecode(src, dst);
	return false;
}

#ifdef ENCRYPT_TEST
int32_t main(int32_t argc, char** argv)
{
#ifndef WIN32
	DoEncodeDecode(argv[1], argv[2]);
	return 1;
#else
	DoEncodeDecode("E:\\av-test\\8.mp4", "E:\\av-test\\8-encode.mp4");
	DoEncodeDecode("E:\\av-test\\8-encode.mp4", "E:\\av-test\\8-decode.mp4");
	system("pause");
#endif
}
#endif