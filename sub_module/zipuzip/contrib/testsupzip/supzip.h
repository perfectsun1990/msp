
#ifndef _SUPZIP_H_
#define _SUPZIP_H_

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
extern ENCDEC_API int IsEncryptSource(const char* src);
extern ENCDEC_API int CompressToZipEnc(const char* dst, const char* src, bool is_append);
extern ENCDEC_API int DecompressZipDec(const char* dst, const char* dir);
#ifdef __cplusplus
}
#endif

#endif // _SUPZIP_H_




