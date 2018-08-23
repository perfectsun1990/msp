/*************************************************************************
* @file   : supzip.h
* @author : sun
* @mail   : perfectsun1990@163.com
* @version: v1.0.0
* @date   : 2018年08月18日 星期四 10时26分12秒
* Copyright (C), 1990-2020, Tech.Co., Ltd. All rights reserved.
************************************************************************/

#ifndef _ENCRYPT_H_
#define _ENCRYPT_H_

#if defined (WIN32) && defined (DLL_EXPORT)
# define ENCDEC_API __declspec(dllexport)
#elif defined (__GNUC__) && (__GNUC__ >= 4)
# define ENCDEC_API __attribute__((visibility("default")))
#else
# define ENCDEC_API
#endif

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 检测src是否是加密源.
 * @param src 待检测源文件.(一般是*.ccr)
 * @return 1-是，0-否.
 */
extern ENCDEC_API int IsEncryptSource(const char* src);

/**
 * @brief 压缩并加密
 * @param dst 最终压缩生成的zip文件.
 * @param src 需要进行压缩的文件.
 * @param compress_level 需要进行压缩的文件.
 * @param is_append =1,追加到dst原先的文件; =0，将覆盖dst原先的文件.
 * @return 0-成功
 * errcode: 
  		-1 -打开压缩文件(dst)失败；
  		-2 -打开输入文件(src)失败；
  		-3 -获取文件crc校验码失败;
  		-4 -保留.
		-5 -打开或写入压缩文件的内部文件失败;
		-6 -dst存在但并不是加密文件格式;
 */
extern ENCDEC_API int CompressToZipEnc(const char* dst, const char* src, bool is_append);

/**
 * @brief 解压并解密
 * @param dst 需要进行解压解密的文件.
 * @param dir 解压后输出目录, =NULL则解压到当前目录. 
 * @return 0-成功
 * errcode: 
		-1 -打开输入文件(dst)失败；
		-2 -本地生成(dst)内部目录或文件失败,fopen或mkdir失败，检测是否有权限或者目录名是否正确;
		-3 -获取压缩文件中具体文件信息(dst->file)失败;
		-4 -获取全局信息(dst)失败;
		-5 -打开或读取压缩文件的内部文件失败;
		-6 -dst存在但并不是加密文件格式;
 */
extern ENCDEC_API int DecompressZipDec(const char* dst, const char* dir);


#ifdef __cplusplus
}
#endif

#endif // _ENCRYPT_H_




