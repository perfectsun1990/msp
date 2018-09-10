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
#include <wchar.h>
#else
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <stdint.h>
#include <assert.h>

#include "alphaGo.hpp"

int32_t
printString(const char* src)
{
	printf("Enter %s\n",__FUNCTION__);
	return -1;
}
