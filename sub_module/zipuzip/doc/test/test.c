/*************************************************************************
 * @file   : test.c
 * @author : sun
 * @mail   : perfectsun1990@163.com 
 * @version: v1.0.0
 * @date   : 2018年08月21日 星期二 11时11分28秒
 * Copyright (C), 1990-2020, Tech.Co., Ltd. All rights reserved.
 ************************************************************************/

#include <stdio.h>

#include "supzip.h"

int main(int argc,const char** argv)
{
#if 1
	if (argc!=4){
		printf("Usage: cmd [e|d] *.zip [src|dir]\n");
		return -1;
	}
	if (argv[1][0] == 'e')
		CompressToZipEnc(argv[2], argv[3], 1);
	if (argv[1][0] == 'd')
		DecompressZipDec(argv[2], argv[3]);
#else
	if (argc!=4){
		printf("Usage: cmd [e|d] *.zip [src|dir]\n");
		return -1;
	}
	if (argv[1][0] == 'e')
		CompressToZip2(argv[2], argv[3], Z_DEFAULT_COMPRESSION, 1, PASSWD, 1);
	if (argv[1][0] == 'd')
		DecompressZip2(argv[2], argv[3], PASSWD, 1);
#endif
	return 0;
}
