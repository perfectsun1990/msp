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
GetSize(FILE* fp, const char* path)
{
	if (NULL != fp) {
		struct stat s;
		if (0 == fstat(fileno(fp), &s))
			return s.st_size;
		return -1;
	}
	if (NULL != path) {
		struct stat s;
		if (0 == stat(path, &s))
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

static inline void swap_char(char *e1, char *e2){
	char tmp = *e1;
	*e1 = *e2;
	*e2 = tmp;
}

static inline bool
DoEncodeDecodeInner(const char* src, const char* dst)
{
#define PWD_SIZE					 128
	static const char pwdstr[PWD_SIZE+1] = \
		"5d597e14144b336d94cf3d0e05158887e6d1fc48e957deff3707680746d1b3f6542d82638ea6f5c822f435823cf4017746d1b3f6542d82638ea6f5c822f43582";
	int32_t pwdlen = sizeof(pwdstr)-1, pwdidx = 0;
	char buffer[PWD_SIZE * 1024] = { 0 };//粒度越大加密效果越差.

	// 打开文件
	FILE *fp1 = fopen(src, "rb");
	if (NULL == fp1) return false;
	FILE *fp2 = fopen(dst, "wb");
	if (NULL == fp2) return false;
	int32_t src_size = GetSize(fp1, NULL);
	if (src_size < 0)return false;

	// 前期处理
	if (IsEncodeSource(src)) {//解密
		fseek(fp1, sizeof(header), SEEK_SET);
	}else {//加密
		fwrite(header, sizeof(char), sizeof(header), fp2);
	}

	// 算法处理
	while ((pwdidx < pwdlen) && !feof(fp1)){
		int32_t rsize = fread(&buffer, sizeof(char), sizeof(buffer), fp1);
		if (rsize > 0) {
			for (int32_t i=0; i<rsize; ++i)
				buffer[i] = buffer[i]^pwdstr[pwdidx++%pwdlen];
			fwrite(buffer, sizeof(char), rsize, fp2);/*异或后写入fp2文件*/
		}
	}
	bool switch_mark = true;
	while (!feof(fp1)){
		memset(buffer, 0, sizeof(buffer));
		int32_t rsize = fread(buffer, sizeof(char), sizeof(buffer), fp1);
		if (rsize > 0) {
			if (switch_mark ^= true) {
				int32_t i = 0, j = rsize - 1;
				while (i++ <= j--)
					swap_char(&buffer[i], &buffer[j]);
			}
			fwrite(buffer, sizeof(char), rsize, fp2);
		}			
	}
	// 关闭文件
	if (NULL != fp1) fclose(fp1);
	if (NULL != fp2) fclose(fp2);
	return true;
}

static inline bool
DoEncodeDecodeRepls(const char* src)
{
	char dst_name[512] = {0};
	snprintf(dst_name, sizeof(dst_name), "%s_dst", src);
	if (!DoEncodeDecode(src, dst_name))
		return false;
	if ( -1 == remove(src))
		return false;
	if ( -1 == rename(dst_name, src))
		return false;
	return true;
}

bool 
DoEncodeDecode(const char* src, const char* dst)
{
	if ( nullptr == dst )
		return DoEncodeDecodeRepls(src);
	return DoEncodeDecodeInner(src, dst);
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
	if (argc < 1 ) return 0;
	printf("src:%s dst:%s\n",argv[1], argv[2]);
	DoEncodeDecode(argv[1], argv[2]);
	//DoEncodeDecode(argv[1], nullptr);
	return 1;
#else
#if 1
	double start = GetTickCount();
	DoEncodeDecode("E:\\av-test\\test.zip", "E:\\av-test\\test-encode.zip");
	double  end = GetTickCount();
	printf("--->>>time:%f\n", end - start);
	DoEncodeDecode("E:\\av-test\\test-encode.zip", "E:\\av-test\\test-decode.zip");
#else
	double start = GetTickCount();
	DoEncodeDecode("F:\\sundir.tar.gz", "F:\\sundir-enc.tar.gz");
	double  end = GetTickCount();
	printf("--->>>time:%f\n", end - start);
	DoEncodeDecode("F:\\sundir-enc.tar.gz", "F:\\sundir-dec.tar.gz");
#endif
	system("pause");
#endif
}
#endif