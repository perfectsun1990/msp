
#if (!defined(_WIN32)) && (!defined(WIN32)) && (!defined(__APPLE__))
        #ifndef __USE_FILE_OFFSET64
                #define __USE_FILE_OFFSET64
        #endif
        #ifndef __USE_LARGEFILE64
                #define __USE_LARGEFILE64
        #endif
        #ifndef _LARGEFILE64_SOURCE
                #define _LARGEFILE64_SOURCE
        #endif
        #ifndef _FILE_OFFSET_BIT
                #define _FILE_OFFSET_BIT 64
        #endif
#endif

#ifdef __APPLE__
// In darwin and perhaps other BSD variants off_t is a 64 bit value, hence no need for specific 64 bit functions
#define FOPEN_FUNC(filename, mode) fopen(filename, mode)
#define FTELLO_FUNC(stream) ftello(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko(stream, offset, origin)
#else
#define FOPEN_FUNC(filename, mode) fopen64(filename, mode)
#define FTELLO_FUNC(stream) ftello64(stream)
#define FSEEKO_FUNC(stream, offset, origin) fseeko64(stream, offset, origin)
#endif


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>

#ifdef _WIN32
# include <direct.h>
# include <io.h>
#else
# include <unistd.h>
# include <utime.h>
# include <sys/types.h>
# include <sys/stat.h>
#endif

#include "zip.h"
#include "unzip.h"

#ifdef _WIN32
        #define USEWIN32IOAPI
        #include "iowin32.h"
#endif

#include "supzip.h"

#define WRITEBUFFERSIZE (16384)
#define MAXFILENAME 	(256)
#define PASSWD 			"41b1495a0615dc17dbc5f4c74941fc63"

#ifdef WIN32
		#define SEP  "\\"
#else
		#define SEP  "/"
#endif

#ifdef _WIN32
uLong filetime(f, tmzip, dt)
    char *f;                /* name of file to get info on */
    tm_zip *tmzip;             /* return value: access, modific. and creation times */
    uLong *dt;             /* dostime */
{
  int ret = 0;
  {
      FILETIME ftLocal;
      HANDLE hFind;
      WIN32_FIND_DATAA ff32;

      hFind = FindFirstFileA(f,&ff32);
      if (hFind != INVALID_HANDLE_VALUE)
      {
        FileTimeToLocalFileTime(&(ff32.ftLastWriteTime),&ftLocal);
        FileTimeToDosDateTime(&ftLocal,((LPWORD)dt)+1,((LPWORD)dt)+0);
        FindClose(hFind);
        ret = 1;
      }
  }
  return ret;
}
#else
#ifdef unix || __APPLE__
uLong filetime(f, tmzip, dt)
    char *f;               /* name of file to get info on */
    tm_zip *tmzip;         /* return value: access, modific. and creation times */
    uLong *dt;             /* dostime */
{
  int ret=0;
  struct stat s;        /* results of stat() */
  struct tm* filedate;
  time_t tm_t=0;

  if (strcmp(f,"-")!=0)
  {
    char name[MAXFILENAME+1];
    int len = strlen(f);
    if (len > MAXFILENAME)
      len = MAXFILENAME;

    strncpy(name, f,MAXFILENAME-1);
    /* strncpy doesnt append the trailing NULL, of the string is too long. */
    name[ MAXFILENAME ] = '\0';

    if (name[len - 1] == '/')
      name[len - 1] = '\0';
    /* not all systems allow stat'ing a file with / appended */
    if (stat(name,&s)==0)
    {
      tm_t = s.st_mtime;
      ret = 1;
    }
  }
  filedate = localtime(&tm_t);

  tmzip->tm_sec  = filedate->tm_sec;
  tmzip->tm_min  = filedate->tm_min;
  tmzip->tm_hour = filedate->tm_hour;
  tmzip->tm_mday = filedate->tm_mday;
  tmzip->tm_mon  = filedate->tm_mon ;
  tmzip->tm_year = filedate->tm_year;

  return ret;
}
#else
uLong filetime( char *f, tm_zip *tmzip, uLong *dt){
    return 0;
}
#endif
#endif

/* mymkdir and change_file_date are not 100 % portable
   As I don't know well Unix, I wait feedback for the unix portion */
int mymkdir(const char* dirname)
{
    int ret=0;
#ifdef _WIN32
    ret = _mkdir(dirname);
#elif unix
    ret = mkdir (dirname,0775);
#elif __APPLE__
    ret = mkdir (dirname,0775);
#endif
    return ret;
}

int getfile( const char *str, char* buf, int len)
{
	char* p_str = (char*)str;
    int32_t i = 0, str_len =strlen(str);
	if (  str_len > len )
		return -2;
	
    char *pos = strrchr( str, '/' );
    if ( NULL != pos ) {   
        memset( buf, 0, len );
        memcpy( buf, p_str+(pos-p_str+1),str_len-(pos-str) );
        return 0;
    }
    return -1;
}

int getdirs( const char *str, char* buf, int len)
{
	char* p_str = (char*)str;
    int32_t i = 0;
	if ( strlen(str) > (size_t)len )
		return -2;
	
    char *pos = strrchr( str, '/' );
    if ( NULL != pos ) {   
        memset( buf, 0, len );
        memcpy( buf, p_str, pos-p_str+1 );
        return 0;
    }
    return -1;
}

int makedir (char *newdir)
{
	char *buffer ;
	char *p;
	int  len = (int)strlen(newdir);

	if (len <= 0)
		return 0;

	buffer = (char*)malloc(len+1);
	if (buffer==NULL)
	{
        printf("Error allocating memory\n");
        return UNZ_INTERNALERROR;
	}
	strcpy(buffer,newdir);

	if (buffer[len-1] == '/') {
	buffer[len-1] = '\0';
	}
	if (mymkdir(buffer) == 0)
	{
	  free(buffer);
	  return 1;
	}

	p = buffer+1;
	while (1)
	{
	  char hold;

	  while(*p && *p != '\\' && *p != '/')
	    p++;
	  hold = *p;
	  *p = 0;
	  if ((mymkdir(buffer) == -1) && (errno == ENOENT))
	    {
	      printf("couldn't create directory %s\n",buffer);
	      free(buffer);
	      return 0;
	    }
	  if (hold == 0)
	    break;
	  *p++ = hold;
	}
	free(buffer);
	return 1;
}


int check_exist_file(filename)
    const char* filename;
{
    FILE* ftestexist;
    int ret = 1;
    ftestexist = FOPEN_FUNC(filename,"rb");
    if (ftestexist==NULL)
        ret = 0;
    else
        fclose(ftestexist);
    return ret;
}

/* calculate the CRC32 of a file,
   because to encrypt a file, we need known the CRC32 of the file before */
int getFileCrc(const char* filenameinzip,void*buf,unsigned long size_buf,unsigned long* result_crc)
{
   unsigned long calculate_crc=0;
   int err=ZIP_OK;
   FILE * fin = FOPEN_FUNC(filenameinzip,"rb");

   unsigned long size_read = 0;
   unsigned long total_read = 0;
   if (fin==NULL)
   {
       err = ZIP_ERRNO;
   }

    if (err == ZIP_OK)
        do
        {
            err = ZIP_OK;
            size_read = (int)fread(buf,1,size_buf,fin);
            if (size_read < size_buf)
                if (feof(fin)==0)
	            {
	                printf("error in reading %s\n",filenameinzip);
	                err = ZIP_ERRNO;
	            }
            if (size_read>0)
                calculate_crc = crc32(calculate_crc,buf,size_read);
            total_read += size_read;

        } while ((err == ZIP_OK) && (size_read>0));

    if (fin)
        fclose(fin);

    *result_crc=calculate_crc;
    //printf("file %s crc %lx\n", filenameinzip, calculate_crc);
    return err;
}

int isLargeFile(const char* filename)
{
	int largeFile = 0;
	ZPOS64_T pos = 0;
	FILE* pFile = FOPEN_FUNC(filename, "rb");
	if(pFile != NULL)
	{
		int n = FSEEKO_FUNC(pFile, 0, SEEK_END);
		pos = FTELLO_FUNC(pFile);
		printf("@Details: %s is %lld bytes\n", filename, pos);
		if(pos >= 0xffffffff)
			largeFile = 1;
		fclose(pFile);
	}

 return largeFile;
}

void swap_char(char *e1, char *e2){
	char tmp = *e1;
	*e1 = *e2;
	*e2 = tmp;
}

void EncryptBuffer(char* buff, int size)
{
	if (size>0 && buff != NULL) {
		int i = 0, j = size - 1;
		do {swap_char(&buff[i++], &buff[j--]);} while (++i <= --j);
	}
}

int CompressToZip2(const char* dst, const char* src,  int compress_level, int is_append, const char* password, int confuse)
{
    int err = ZIP_OK, i = 0;
	char  buf[WRITEBUFFERSIZE] = {0};	
    const char* src_rawfile = src;
	const char* dst_zipfile = dst;
	FILE * fin = NULL;
    unsigned long crcFile=0;
    int zip64 = 0;
	zipFile 		zf;
    zip_fileinfo 	zi;
    zi.tmz_date.tm_sec = zi.tmz_date.tm_min = zi.tmz_date.tm_hour =
    zi.tmz_date.tm_mday = zi.tmz_date.tm_mon = zi.tmz_date.tm_year = 0;
    zi.dosDate = 0;
    zi.internal_fa = 0;
    zi.external_fa = 0;
    filetime(src_rawfile, &zi.tmz_date, &zi.dosDate);
	

	is_append = (0 == is_append) ? is_append : 2;
	is_append = check_exist_file(dst_zipfile) ? is_append: 0;
	

	/* Open the zip file for write */
#ifdef USEWIN32IOAPI
    zlib_filefunc64_def ffunc;
    fill_win32_filefunc64A(&ffunc);
    zf = zipOpen2_64(dst_zipfile, is_append, NULL,&ffunc);
#else
    zf = zipOpen64(dst_zipfile, is_append);
#endif
    if (zf == NULL) {
        printf("zipOpen64: error opening %s\n", dst_zipfile);
		return -1;
    }
	
    printf("@General %s, src=%s is_append=%d\n", dst_zipfile, src, is_append);
	
	/* caclulate the crc for passwd */
    if ((password != NULL) && (err==ZIP_OK)){
        err = getFileCrc(src_rawfile, buf, sizeof(buf), &crcFile);
		if (err == ZIP_ERRNO) {
	        printf("error getFileCrc %s\n", src_rawfile);
			return -3;
	    }
    }
    zip64 = isLargeFile(src_rawfile);

	if (1)
	{// There's some problem of path with "\ // :\\ . ..",we must handle it.
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf), "%s", src_rawfile);
		char* p_src_rawfile = strstr(buf, ":\\");//is windows? E:\\c.mp4
		p_src_rawfile = (NULL == p_src_rawfile) ? buf : p_src_rawfile + 2;
		while (p_src_rawfile[0] == '\\' || p_src_rawfile[0] == '/' || p_src_rawfile[0] == '.')
			p_src_rawfile++; // pure path extract ":// \ . .."
		for (i = 0; i < strlen(p_src_rawfile); ++i) { // replace '\\' by '/'
			if (p_src_rawfile[i] == '\\')
				p_src_rawfile[i] = '/';
		}
		err = zipOpenNewFileInZip3_64(zf, p_src_rawfile, &zi,
						 NULL,0,NULL,0,NULL /* comment*/,
						 (compress_level != 0) ? Z_DEFLATED : 0,
						 compress_level,0,
						 /* -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY, */
						 -MAX_WBITS, DEF_MEM_LEVEL, Z_DEFAULT_STRATEGY,
						 password, crcFile, zip64);
		if (err != ZIP_OK){
			printf("error in opening %s in zipfile\n", src_rawfile);
			return -5;
		}
		// Open the input source file.
		fin = FOPEN_FUNC(src_rawfile, "rb");
		if (fin==NULL){
			printf("error in opening %s for reading\n",src_rawfile);
			return -2;
		}
		
		// Read buffer and wrtie to zip file.
		while (!feof(fin))
		{
			int rsize = fread(buf, sizeof(char), sizeof(buf), fin);
			if (rsize > 0) {
				if (confuse)
					EncryptBuffer(buf, rsize);  
				err = zipWriteInFileInZip (zf, buf, rsize);
				if (err < 0) {
					printf("error in writing %s in the zipfile!err=%d\n", src_rawfile, err);
					return -5;
			   	}
			}			
		}
		
		// Close the relative input source in zip.
		if (fin) fclose(fin);
		err = zipCloseFileInZip(zf);
		if (err!=ZIP_OK) 
			printf("error in closing %s in the zipfile\n", src_rawfile);
	}
	
	/* Close the zip file */
    err = zipClose(zf, NULL);
    if (err != ZIP_OK)
        printf("error in closing %s\n",dst_zipfile);
	
    printf("@Success add %s to %s!\n\n", src, dst_zipfile);
    return 0;
}

int DecompressZip2(const char* dst, const char* dir, const char* passwd, int confuse)
{
    int err = 0;
	uLong i = 0;
    // Buffer to hold data read from the zip file.
    char buff[ WRITEBUFFERSIZE ];

	// Open zip file.
    unzFile *zf = unzOpen( dst );
    if ( zf == NULL ){
        printf( "%s: not found\n",dst);
        return -1;
    }
	
	// Get detail zip file info.
    unz_global_info global_info;
    if (unzGetGlobalInfo( zf, &global_info ) != UNZ_OK)
	{
        printf( "could not read file global info\n" );
        unzClose(zf);
        return -4;
    }
	
	// Extract all file and dirs in zip.
    for (i = 0; i < global_info.number_entry; ++i)
    {
        // Get info about current file.
        unz_file_info file_info;
        char filename[ 2*MAXFILENAME ] = {0}, curfilename[ MAXFILENAME ] = {0};
        if ( unzGetCurrentFileInfo( zf, &file_info, 
			curfilename, sizeof(curfilename),NULL, 0, NULL, 0 ) != UNZ_OK )
        {
            printf( "could not read file info\n" );
            unzClose( zf );
            return -3;
        }
			
		if ( NULL != dir ) {
			int size = strlen( dir );
       		if ( dir[size-1] == '/' || dir[size-1] == '\\') {
				snprintf(filename , sizeof(filename), "%s%s", dir, curfilename);
			}else {
				snprintf(filename , sizeof(filename), "%s%s%s", dir, SEP, curfilename);	
			}
		} else
			snprintf(filename , sizeof(filename), "%s", curfilename);
					
        // Check  the entry is a directory or file.
        const size_t size = strlen( filename );
        if ( filename[size-1] == '/' || filename[size-1] == '\\')
        { // Entry is a directory, so create it.
            printf( "1dir:%s\n", filename );
			if (!check_exist_file(filename))
				if ( !makedir(filename) )
					return -2;
        }else {
            // Entry is a file, so extract it.
            printf( "file:%s passwd=%s\n", filename, passwd );
			char tmp[MAXFILENAME] = {0};
			if ( 0 == getdirs(filename, tmp, sizeof(tmp))){
				printf("2dir:%s\n",tmp);
				if (!check_exist_file(tmp))
					if ( !makedir(tmp) )
						return -2;
			}
			err = unzOpenCurrentFilePassword(zf, passwd);
            if ( err != UNZ_OK ) {//other options.
                printf( "could not open file:%d\n", err );
                unzClose( zf );
                return -5;
            }
			
            // Open a file to write out the data.
            FILE *out = fopen( filename, "wb+" );
            if ( out == NULL ) {
                printf( "could not open destination file=%s\n",filename);
                unzClose( zf );
                return -2;
            }
            int size = 0;
            do {
                size = unzReadCurrentFile( zf, buff, sizeof(buff) );
                if ( size < 0 ){
                    printf( "error %d\n", size );
                    unzClose( zf );
 				  	fclose( out );
                    return -5;
                }
				if (confuse)
					EncryptBuffer(buff, size);
                if ( size > 0 )  // Write data to file.
                    fwrite( buff, size, 1, out );
            } while ( size > 0 );

            fclose( out );
        }

        unzCloseCurrentFile( zf );
        if ( ( i+1 ) < global_info.number_entry )
        {// Go the the next entry listed in the zip file.
            if ( unzGoToNextFile( zf ) != UNZ_OK ){
                printf( "cound not read next file\n" );
                unzClose( zf );
                return -3;
            }
        }
    }
	
	// Close zip file.
    unzClose( zf );
    return 0;
}

int IsEncryptSource(const char* src)
{
	const char header[4] = {'S','C','C','R'};
	const char ziphdr[4] = {0x50, 0x4B, 0x03, 0x04};
	
	char cache[4] = { 0 };
	int is_sccr = 0, i=0;
	
	FILE* fp = fopen(src, "rb");
	if (NULL != fp ) {
		is_sccr = 1;
		int ret = fread(cache, sizeof(char), sizeof(cache), fp);
		for (i=0; i< sizeof(header); ++i){
			if (header[i] != cache[i]){
				is_sccr = 0;
				break;
			}
		}
		fclose(fp);
	}
	return is_sccr;
}

int CompressToZipEnc2(const char* dst, const char* src, bool is_append)
{
	const char header[4] = {'S','C','C','R'};
	const char ziphdr[4] = {0x50, 0x4B, 0x03, 0x04};

	char cache[4] = { 0 };
	FILE* fp = fopen(dst, "rb+");
	if (NULL != fp ) {
		if (IsEncryptSource(dst)) {
			fwrite(ziphdr, sizeof(char), sizeof(ziphdr), fp);
			fclose(fp);
		}else{
			fclose(fp);
			return -6;
		}
	}
	int ret = CompressToZip2(dst, src, Z_NO_COMPRESSION, is_append, PASSWD, 1);
	if (0 == ret)
	{
		FILE* fp = fopen(dst, "rb+");
		if (NULL != fp ) {
			fwrite(header, sizeof(char), sizeof(header), fp);
			fclose(fp);
		}
	}
	return ret;
}

#ifdef unix
#include <limits.h> 
#include <dirent.h>

int CompressToZip3(const char* dst, const char* src, int compress_level, int is_append, const char* password, int confuse, bool is_first)
{
	char dir[MAXFILENAME] 		= {0};
	char childpath[MAXFILENAME] = {0};
	DIR *dp;                
	struct dirent *entry;   
	struct stat statbuf;    

	static char currentdir[MAXFILENAME] = {0};
	static char compresdir[MAXFILENAME] = {0};
	static int  offset = 0;
	
	int ret = lstat(src, &statbuf);
	if (ret < 0 ){
		if (strlen(currentdir) > 0)
			return 0;
		return -2;
	}
	if ( S_ISDIR(statbuf.st_mode) ) 
	{
		strcpy(dir, src);
		if (is_first) 
		{// eg. /test/abc/--->/test/abc
			size_t size = strlen( dir );
			while(size > 0 && dir[size-1] == '/' )
				dir[--size] = '\0';
			char *pLastdir = strrchr(dir, '/');
			offset =  (NULL == pLastdir) ? 0 : pLastdir - dir+1;
			snprintf(compresdir, sizeof(compresdir), "%s/../", dir);
			if (NULL == getcwd(currentdir, sizeof(currentdir)))
				return -4;
			is_first = 0;
		}
		if((dp = opendir(dir)) == NULL) {
		   printf("#can't open dir: %s", dir);
		   return -2;
		}
		
		int dir_nums = 0, file_nums = 0;
		// Looping get all dirs and files in current dir.
		while((entry = readdir(dp)) != NULL)
		{
			// lstat(entry->d_name, &statbuf); 	// lstat can't check entry->d_name is dir or file,always is 1.
			if(DT_DIR == entry->d_type)
			{// Is dirs.
				dir_nums++;
				if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
					continue;
			    size_t size = strlen( dir );
				while(size > 0 && dir[size-1] == '/' )
					dir[--size] = '\0';
				snprintf(childpath, sizeof(childpath), "%s%s%s", dir, SEP, entry->d_name);
				printf("--->Add path:%s\n",childpath);
				ret = CompressToZip3(dst, childpath, compress_level, is_append, password, confuse, 0);
				if (ret < 0) break;
			} else {// is file.
				file_nums++;
				snprintf(childpath, sizeof(childpath), "%s%s%s", dir, SEP, entry->d_name); 					
					if (-1 == chdir(compresdir)) return -2;
				char *pfile = childpath + offset;
				printf("--->Add File:%s pfile=%s\n",childpath, pfile);		
				ret = CompressToZip2(dst, pfile, compress_level, is_append, password, confuse);
				if (-1 == chdir(currentdir)) return -2;				
				if (ret < 0) break;
			}
		}
		closedir(dp);
		// if have empty dirs use placeholder.
		if (2 == dir_nums && 0 == file_nums) {
			char placeholder[MAXFILENAME] = {0};
			snprintf(placeholder, sizeof(placeholder),"%s/.placeholder", dir);
			ret =fclose(fopen(placeholder,"wb+"));
			if (ret < 0) return -7;
			ret = CompressToZip2(dst, placeholder, compress_level, is_append, password, confuse);
			if (ret < 0) return -7;
			printf("### create Empty dir=%s, placeholder=%s\n", dir,placeholder);
		}
		return ret;
	}
	
	if ( S_ISREG(statbuf.st_mode) ) 
	{
		if (is_first) 
		{// eg. /test/abc--->/test/abc
			size_t size = strlen( src );
			char *pLastdir = strrchr(src, '/');
			offset =  (NULL == pLastdir) ? 0 : pLastdir - src+1;			
			char *pfile = src + offset;
			if (NULL == getcwd(currentdir, sizeof(currentdir)))
				return -4;
			ret = getdirs(src, childpath, sizeof(childpath));
			if (ret <0)
				snprintf(compresdir, sizeof(compresdir), "%s", currentdir);
			else				
				snprintf(compresdir, sizeof(compresdir), "%s", childpath);
			is_first = 0;
		}
		
		printf("Add File:%s\n",src);		
		char *pfile = src + offset;
		if (-1 == chdir(compresdir)) return -2;
		ret = CompressToZip2(dst, pfile, compress_level, is_append, password, confuse);
		if (-1 == chdir(currentdir)) return -2; 			
		return ret;
	}

	return -2;
}

int CompressToZipEnc3(const char* dst, const char* src, bool is_append, bool is_first)
{
	char dir[MAXFILENAME] 		= {0};
	char childpath[MAXFILENAME] = {0};
	DIR *dp;                
	struct dirent *entry;   
	struct stat statbuf;    

	static char currentdir[MAXFILENAME] = {0};
	static char compresdir[MAXFILENAME] = {0};
	static int  offset = 0;
	
	int ret = lstat(src, &statbuf);
	if (ret < 0 ){
		if (strlen(currentdir) > 0)
			return 0;
		return -2;
	}
	if ( S_ISDIR(statbuf.st_mode) ) 
	{
		strcpy(dir, src);
		if (is_first) 
		{// eg. /test/abc/--->/test/abc
			size_t size = strlen( dir );
			while(size > 0 && dir[size-1] == '/' )
				dir[--size] = '\0';
			char *pLastdir = strrchr(dir, '/');
			offset =  (NULL == pLastdir) ? 0 : pLastdir - dir+1;
			snprintf(compresdir, sizeof(compresdir), "%s/../", dir);
			if (NULL == getcwd(currentdir, sizeof(currentdir)))
				return -4;
			is_first = 0;
		}
		if((dp = opendir(dir)) == NULL) {
		   printf("#can't open dir: %s", dir);
		   return -2;
		}
		
		int dir_nums = 0, file_nums = 0;
		// Looping get all dirs and files in current dir.
		while((entry = readdir(dp)) != NULL)
		{
			// lstat(entry->d_name, &statbuf); 	// lstat can't check entry->d_name is dir or file,always is 1.
			if(DT_DIR == entry->d_type)
			{// Is dirs.
				dir_nums++;
				if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0)
					continue;
			    size_t size = strlen( dir );
				while(size > 0 && dir[size-1] == '/' )
					dir[--size] = '\0';
				snprintf(childpath, sizeof(childpath), "%s%s%s", dir, SEP, entry->d_name);
				printf("--->Add path:%s\n",childpath);
				ret = CompressToZipEnc3(dst, childpath, is_append, 0);
				if (ret < 0) break;
			} else {// is file.
				file_nums++;
				snprintf(childpath, sizeof(childpath), "%s%s%s", dir, SEP, entry->d_name); 					
					if (-1 == chdir(compresdir)) return -2;
				char *pfile = childpath + offset;
				printf("--->Add File:%s pfile=%s\n",childpath, pfile);		
				ret = CompressToZipEnc2(dst, pfile, is_append);
				if (-1 == chdir(currentdir)) return -2;				
				if (ret < 0) break;
			}
		}
		closedir(dp);
		// if have empty dirs use placeholder.
		if (2 == dir_nums && 0 == file_nums) {
			char placeholder[MAXFILENAME] = {0};
			snprintf(placeholder, sizeof(placeholder),"%s/.placeholder", dir);
			ret =fclose(fopen(placeholder,"wb+"));
			if (ret < 0) return -7;
			ret = CompressToZipEnc2(dst, placeholder, is_append);
			if (ret < 0) return -7;
			printf("### create Empty dir=%s, placeholder=%s\n", dir,placeholder);
		}
		return ret;
	}
	
	if ( S_ISREG(statbuf.st_mode) ) 
	{
		if (is_first) 
		{// eg. /test/abc--->/test/abc
			size_t size = strlen( src );
			char *pLastdir = strrchr(src, '/');
			offset =  (NULL == pLastdir) ? 0 : pLastdir - src+1;			
			char *pfile = src + offset;
			if (NULL == getcwd(currentdir, sizeof(currentdir)))
				return -4;
			ret = getdirs(src, childpath, sizeof(childpath));
			if (ret <0)
				snprintf(compresdir, sizeof(compresdir), "%s", currentdir);
			else				
				snprintf(compresdir, sizeof(compresdir), "%s", childpath);
			is_first = 0;
		}
		
		printf("Add File:%s\n",src);		
		char *pfile = src + offset;
		if (-1 == chdir(compresdir)) return -2;
		ret = CompressToZipEnc2(dst, pfile, is_append);		
		if (-1 == chdir(currentdir)) return -2; 			
		return ret;
	}

	return -2;
}
#endif

// Normal compress apis.
int CompressToZip(const char* dst, const char* src, bool is_append)
{
#ifdef unix
	return CompressToZip3(dst, src, Z_NO_COMPRESSION, is_append, NULL, 0, 1);
#else
	return CompressToZip2(dst, src, Z_NO_COMPRESSION, is_append, NULL, NULL);
#endif
}

int DecompressZip(const char* dst, const char* dir)
{
	return DecompressZip2(dst, dir, NULL, 0);
}

// Encrypt compress apis.
int CompressToZipEnc(const char* dst, const char* src, bool is_append)
{
#ifdef unix
	return CompressToZipEnc3(dst, src, is_append, 1);
#else
	return CompressToZipEnc2(dst, src, is_append);
#endif
}

int DecompressZipDec(const char* dst, const char* dir)
{
	const char header[4] = {'S','C','C','R'};
	const char ziphdr[4] = {0x50, 0x4B, 0x03, 0x04};

	int need_reset = 0;
	if (!IsEncryptSource(dst)) {
		int ret = DecompressZip2(dst, dir, NULL, 0);
		if (ret<0)
			printf("@@@ invalid ccr encrypt source! %s\n", dst);
		return ret;
	}else{
		FILE* fp = fopen(dst, "rb+");
		if (NULL != fp ) {
			fwrite(ziphdr, sizeof(char), sizeof(ziphdr), fp);		
			fclose(fp);
			need_reset = 1;
		}else{
			return -1;
		}
	}
	int ret = DecompressZip2(dst, dir, PASSWD, 1);
	if (0 == ret || 1 == need_reset )
	{
		FILE* fp = fopen(dst, "rb+");
		if (NULL != fp ) {
			fwrite(header, sizeof(char), sizeof(header), fp);
			fclose(fp);
		}
	}
	return ret;
}


#ifdef DEBUG_TEST
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
#endif


