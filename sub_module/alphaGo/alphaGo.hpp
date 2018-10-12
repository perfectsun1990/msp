/*************************************************************************
* @file   : EncodeDecode.c
* @author : sun
* @mail   : perfectsun1990@163.com
* @version: v1.0.0
* @date   : 2018年08月02日 星期四 10时26分12秒
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

	// 检测源.
	extern ENCDEC_API bool IsEncodeSource(const char* src);
	// 加解密. src是未加密文件直接加密,是加密文件则解密<dst=nullptr则覆盖>。
	extern ENCDEC_API bool DoEncodeDecode(const char* src, const char* dst);
	// 仅加密. src是未加密文件直接加密,已加密则直接返回<dst=nullptr则覆盖>。
	extern ENCDEC_API bool DoEncode(const char* src, const char* dst);
	// 仅解密. src是未加密文件返回失败,是加密文件则解密<dst=nullptr则覆盖>。
	extern ENCDEC_API bool DoDecode(const char* src, const char* dst);

#ifdef __cplusplus
}
#endif

#endif // _ENCRYPT_H_
