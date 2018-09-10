
#include <stdio.h>
#include <string.h>
#include <stdlib.h> 

#include "supzip.h"

#define WRITEBUFFERSIZE (16384)
#define MAXFILENAME 	(256)
//#define PASSWD 			"41b1495a0615dc17dbc5f4c74941fc63"

#ifdef unix
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h> 
#endif

int main(int argc,const char** argv)
{
#ifndef WIN32
#ifdef unix
	int ret = 0;
	char dir[MAXFILENAME] 		= {0};
	char childpath[MAXFILENAME] = {0};
	DIR *dp;                
	struct dirent *entry;   
	struct stat statbuf;    
	char currentdir[MAXFILENAME] = {0};
	char compresdir[MAXFILENAME] = {0};
	char dstzipfile[MAXFILENAME] = {0};

	if (argc < 4 || argc > 5){
		printf("Usage: cmd [e|d] [*.zip] [src|dir] <opt>\n");
		return -1;
	}
	
	if (argv[1][0] == 'd')
		DecompressZipDec(argv[2], argv[3]);
	if (argv[1][0] == 'e') {
		if (argc == 4) {
			realpath(argv[2], dstzipfile);
			return CompressToZipEnc(dstzipfile, argv[3], 1);
		}
		if (argc == 5 && argv[4][0] == '1')
		{	
			realpath(argv[2], dstzipfile);
			strcpy(dir, argv[3]);
			if (lstat(dir, &statbuf) < 0 ){
				perror("lstat:");
				return -2;
			}
			if ( S_ISDIR(statbuf.st_mode) ) 
			{
				if((dp = opendir(dir)) == NULL) {
					printf("#can't open dir: %s", dir);
					return -2;
				}
				if (NULL == getcwd(currentdir, sizeof(currentdir)))
					return -4;
				snprintf(compresdir, sizeof(compresdir), "%s", dir);
				if (-1 == chdir(compresdir)) return -2;
				while((entry = readdir(dp)) != NULL)
				{
					if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
						continue;
					size_t size = strlen( dir );
					while(size > 0 && dir[size-1] == '/' )
						dir[--size] = '\0';
					snprintf(childpath, sizeof(childpath), "%s", entry->d_name);
					printf("Info: compresdir=%s --->zippath:%s\n", compresdir, childpath );
					ret = CompressToZipEnc(dstzipfile, childpath, 1);
					if (ret < 0) break;
				}
				if (-1 == chdir(currentdir)) return -2; 

			}	
			if ( S_ISREG(statbuf.st_mode) ) 
			{
				return CompressToZipEnc(argv[2], argv[3], 1);
			}
		}
	}
#endif

#else
	printf("begin to compress....\n");
	if (CompressToZipEnc("E:\\123.zip", "E:\\ÖÐÎÄ\\Òì³£.txt", 1) < 0)
		return -1;
	printf("begin to decompress....\n");
	if (DecompressZipDec("E:\\123.zip", "E:\\123")< 0)
		return -2;
	system("pause");
#endif
	return 0;
}
