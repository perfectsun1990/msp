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

	extern ENCDEC_API int32_t printString(const char* src);

#ifdef __cplusplus
}
#endif

#endif // _ENCRYPT_H_
