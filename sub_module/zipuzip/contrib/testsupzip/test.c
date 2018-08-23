
#include <stdio.h>
#include "supzip.h"

#define PASSWD 			"41b1495a0615dc17dbc5f4c74941fc63"

int main(int argc,const char** argv)
{
#ifndef WIN32
	if (argc!=4){
		printf("Usage: cmd [e|d] *.zip [src|dir]\n");
		return -1;
	}
	if (argv[1][0] == 'e')
		CompressToZipEnc(argv[2], argv[3], 1);
	if (argv[1][0] == 'd')
		DecompressZipDec(argv[2], argv[3]);
#if 0
	if (argc != 4) {
		printf("Usage: cmd [e|d] *.zip [src|dir]\n");
		return -1;
	}
	if (argv[1][0] == 'e')
		CompressToZip2(argv[2], argv[3], Z_DEFAULT_COMPRESSION, 1, PASSWD, 1);
	if (argv[1][0] == 'd')
		DecompressZip2(argv[2], argv[3], PASSWD, 1);
#endif
#else
	printf("begin to compress....\n");
	if (CompressToZipEnc("E:\\123.zip", "E:\\av-test\\10s.mp4", 1) < 0)
		return -1;
	printf("begin to decompress....\n");
	if (DecompressZipDec("E:\\123.zip", "E:\\123")< 0)
		return -2;
	system("pause");
#endif
	return 0;
}
