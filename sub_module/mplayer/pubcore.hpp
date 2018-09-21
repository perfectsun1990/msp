
/*************************************************************************
 * Copyright (C), 1990-2020, Tech.Co., Ltd. All rights reserved.
 * @file   : pubcore.hpp
 * @author : sun
 * @mail   : perfectsun1990@163.com 
 * @version: v1.0.0
 * @date   : 2016年05月20日 星期二 11时57分04秒 
 *-----------------------------------------------------------------------
 * @detail : 公共模块
 * @         功能:加载系统依赖接口、全局变量、宏及控制/通信协议.
 ************************************************************************/
 
#pragma  once

/***************************1.公用系统文件集合***************************/
#ifdef  unix
#include <unistd.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/resource.h>
#include <grp.h>
#include <pwd.h>
#include <dirent.h>
#endif
#ifdef  WIN32
#include <windows.h>
#include <corecrt_io.h>
#include <io.h>
#include <direct.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <corecrt_io.h>
#include <wchar.h>
#endif
#ifdef __cplusplus
#include <iostream>
#include <future>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>
#include <queue>
#include <stack>
#include <list>
#include <tuple>
#include <map>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <cstdio>
#include <cassert>
extern "C"
{
#endif
#include "libavutil/avstring.h"
#include "libavutil/imgutils.h"
#include "libavutil/opt.h"
#include "libavutil/parseutils.h"
#include "libavutil/pixdesc.h"
#include "libavutil/pixfmt.h"
#include "libavutil/time.h"
#include "libavutil/mem.h"
#include "libavutil/timestamp.h"
#include "libavdevice/avdevice.h"
#include "libavfilter/avfilter.h"
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswresample/swresample.h"
#include "libswscale/swscale.h"
#include <SDL.h>
#include <SDL_thread.h>
#undef main //NOTE:SDL2 define main micro.
#ifdef __cplusplus
}
#endif

/***************************2.公用函数或宏封装***************************/
typedef enum log_rank{	LOG_ERR, LOG_WAR, LOG_MSG, LOG_DBG,}log_rank_t;
//#define USE_DEBUG_LOG

/**
*@公共的宏函数操作
*/
const static log_rank_t	_log_rank =	  LOG_MSG;

#ifndef USE_DEBUG_LOG
#define err( format, ... )do{ if( LOG_ERR <= _log_rank )\
	av_log(nullptr, AV_LOG_ERROR, "[<%s>:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#define war( format, ... )do{ if( LOG_WAR <= _log_rank )\
	av_log(nullptr, AV_LOG_WARNING, "[<%s>:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#define msg( format, ... )do{ if( LOG_MSG <= _log_rank )\
	av_log(nullptr, AV_LOG_INFO, "[<%s>:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#define dbg( format, ... )do{ if( LOG_DBG <= _log_rank )\
	av_log(nullptr, AV_LOG_DEBUG,"[<%s>:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#define out( format, ... )do{ av_log(nullptr, AV_LOG_WARNING, format, ##__VA_ARGS__); }while(0)
#else
//Note: Just use for debug, it must be replaced by log system if used in project.
#define err( format, ... )do{ if( LOG_ERR <= _log_rank )\
	fprintf(stderr, "[<%s>:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#define war( format, ... )do{ if( LOG_WAR <= _log_rank )\
	fprintf(stderr, "[<%s>:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#define msg( format, ... )do{ if( LOG_MSG <= _log_rank )\
	fprintf(stderr, "[<%s>:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#define dbg( format, ... )do{ if( LOG_DBG <= _log_rank )\
	fprintf(stderr, "[<%s>:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#define out( format, ... )do{ fprintf(stderr, format, ##__VA_ARGS__); }while(0)
#endif

#define BCUT_04(x,n)                ( ( (x) >> (n) ) & 0x0F )           // 获取x的(n~n+03)位
#define BCUT_08(x,n)                ( ( (x) >> (n) ) & 0xFF )           // 获取x的(n~n+07)位
#define BCUT_16(x,n)                ( ( (x) >> (n) ) & 0xFFFF )         // 获取x的(n~n+15)位

#define BSET(x,n)                   ( (x) |=  ( 1 << (n) ) )            // 设置x的第n位为"1"
#define BCLR(x,n)                   ( (x) &= ~( 1 << (n) ) )            // 清除x的第n位为"0"
#define BCHK(x,n)                   ( ( (x) >> (n) ) & 1 )              // 检测某位是否是"1"

#define BYTE_ORDR                   ( __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ )
#define SWAP_16(x)                  ((x>>08&0xff)|(x<<08&0xff00))       // 大小端字节序转换
#define SWAP_24(x)                  ((x>>16&0xff)|(x<<16&0xff0000)|x&0xff00)  
#define SWAP_32(x)                  ((x>>24&0xff)|(x>>8&0xff00)|(x << 8 & 0xff0000) | (x << 24 & 0xff000000))  

#define ELEMENTS(s)					( sizeof(s)/sizeof(s[0]) )
#define FREESIZE(s)					( sizeof(s)- strlen(s)-1 )

/**
 *@字符串集常用操作
 */

static inline bool
IsStrEffect(const char* url)
{
	return (nullptr != url  && '\0' != url[0] && ' ' != url[0]
		&& '\t' != url[0] && '\r' != url[0] && '\n' != url[0]);
}

static inline bool
IsNetStream(const char* url) {
	return (strstr(url, "rtsp:") || strstr(url, "rtmp:")
		|| strstr(url, "mms:") || strstr(url, "http:"));
}

static inline int
__mkdir(const char* dirname)
{
	int ret = 0;
#ifdef _WIN32
	ret = _mkdir(dirname);
#elif unix
	ret = mkdir(dirname, 0775);
#elif __APPLE__
	ret = mkdir(dirname, 0775);
#endif
	return ret;
}

static inline int
FiltFile(const char *str, char* buf, int len)
{
	char* p_str = (char*)str;
	int32_t i = 0, str_len = strlen(str);
	if (str_len > len)
		return -2;
	const char *pos = strrchr(str, '/');
	if (NULL != pos) {
		memset(buf, 0, len);
		memcpy(buf, p_str + (pos - p_str + 1), str_len - (pos - str));
		return 0;
	}
	return -1;
}

static inline int
FiltDirc(const char *str, char* buf, int len)
{
	char* p_str = (char*)str;
	int32_t i = 0;
	if (strlen(str) > (size_t)len)
		return -2;
	const char *pos = NULL;
	pos = strrchr(str, '/');
#ifdef WIN32
	pos = (NULL == pos) ? strrchr(str, '\\') : pos;
#endif	
	if (NULL != pos) {
		memset(buf, 0, len);
		memcpy(buf, p_str, pos - p_str + 1);
		return 0;
	}
	return -1;
}

static inline int
MakeDir(char *newdir)
{
	char *buffer = NULL;
	char *p = NULL;
	int  len = (int)strlen(newdir);
	if (len <= 0)	return 0;
	buffer = (char*)malloc(len + 1);
	if (buffer == NULL){
		printf("Error allocating memory\n");
		return -1;
	}
	strcpy(buffer, newdir);
	if (buffer[len - 1] == '/') {
		buffer[len - 1] = '\0';
	}
	if (__mkdir(buffer) == 0){
		free(buffer);
		return 1;
	}
	p = buffer + 1;
	while (1) {
		char hold;
		while (*p && *p != '\\' && *p != '/')
			p++;
		hold = *p;
		*p = 0;
		if ((__mkdir(buffer) == -1) && (errno == ENOENT)){
			printf("couldn't create directory %s\n", buffer);
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

// cpp.
static inline std::vector<std::string>
StrSplits(std::string str, std::string pattern)
{
	std::string::size_type pos;
	std::vector<std::string> result;
	str += pattern;//扩展字符串以方便操作
	size_t size = str.size();

	for (size_t i = 0; i < size; i++)
	{
		pos = str.find(pattern, i);
		if (pos < size)
		{
			std::string s = str.substr(i, pos - i);
			result.push_back(s);
			i = pos + pattern.size() - 1;
		}
	}
	return result;
}

static inline std::string
StrReplace(std::string strSrc,
	const std::string &oldStr, const std::string &newStr, int count = -1)
{
	std::string strRet = strSrc;
	size_t pos = 0;
	int l_count = 0;
	if (-1 == count) // replace all
		count = strRet.size();
	while ((pos = strRet.find(oldStr, pos)) != std::string::npos)
	{
		strRet.replace(pos, oldStr.size(), newStr);
		if (++l_count >= count) break;
		pos += newStr.size();
	}
	return strRet;
}

/***************************3.平台相关通用函数***************************/
/**
*@字符编码常用函数
*/
#ifdef WIN32
/**
 *@Note:
 *@iCharNums 标识字符或宽字符个数,而非字节个数.
 *@所以确保size和目标datas的类型一致且大小足够.
 */
static inline int32_t
Utf82Unic(const char* utf8, wchar_t* unic, int32_t size)
{
	if (!utf8 || !strlen(utf8) || !unic || size <= 0)
		return 0;
	wmemset(unic, 0, size);			//Note: F1  details.
	int32_t iCharNums = MultiByteToWideChar(CP_UTF8, 0,
		utf8, -1, NULL, 0);
	if (iCharNums > size)	return 0;
	MultiByteToWideChar(CP_UTF8, 0,	//Covt: utf8->utf16
		utf8, -1, unic, iCharNums);
	return iCharNums;
}

static inline int32_t
Unic2Utf8(const wchar_t* unic, char* utf8, int32_t size)
{
	if (!unic || !wcslen(unic) || !utf8 || size <= 0)
		return 0;
	memset(utf8, 0, size);			//Note: F1  details.
	int32_t iCharNums = WideCharToMultiByte(CP_UTF8, 0,
		unic, -1, NULL, 0, NULL, NULL);
	if (iCharNums > size)	return 0;
	WideCharToMultiByte(CP_UTF8, 0, //Covt: utf16->utf8
		unic, -1, utf8, iCharNums, NULL, NULL);
	return iCharNums;
}

static inline int32_t
Ansi2Unic(const char* ansi, wchar_t* unic, int32_t size)
{
	if (!ansi || !strlen(ansi) || !unic || size <= 0)
		return 0;
	wmemset(unic, 0, size);			//Note: F1  details.
	int32_t iCharNums = MultiByteToWideChar(CP_ACP, 0,
		ansi, -1, NULL, 0);
	if (iCharNums > size)	return 0;
	MultiByteToWideChar(CP_ACP, 0, 	//Covt: ansi->utf16
		ansi, -1, unic, iCharNums);
	return iCharNums;
}

static inline int32_t
Unic2Ansi(const wchar_t* unic, char* ansi, int32_t size)
{
	if (!unic || !wcslen(unic) || !ansi || size <= 0)
		return 0;
	memset(ansi, 0, size);			//Note: F1  details.
	int32_t iCharNums = WideCharToMultiByte(CP_ACP, 0,
		unic, -1, NULL, 0, NULL, NULL);
	if (iCharNums > size)	return 0;
	WideCharToMultiByte(CP_ACP,		//Covt: utf16->ansi
		0, unic, -1, ansi, iCharNums, NULL, NULL);
	return iCharNums;
}

static inline int32_t
Utf82Ansi(const char *utf8, char* ansi, int32_t size)
{
	memset(ansi, 0, size);			//Note: F1  details.
	int32_t  iSize = strlen(utf8) + 1;
	wchar_t* pUnic = (wchar_t*)malloc(sizeof(wchar_t)*iSize);// wchar_t[iSize];
	if (NULL == pUnic)		goto handle_error;
	int32_t iCharNums = Utf82Unic(utf8, pUnic, iSize);
	if (iCharNums <= 0)		goto handle_error;
	iCharNums = Unic2Ansi(pUnic, ansi, size);
	if (iCharNums <= 0)		goto handle_error;
	free(pUnic);
	return iCharNums;
handle_error:
	if (NULL != pUnic)		free(pUnic);
	return 0;
}

static inline int32_t
Ansi2Utf8(const char *ansi, char* utf8, int32_t size)
{
	memset(utf8, 0, size);//Note: F1  details.
	int32_t  iSize = strlen(ansi) + 1;
	wchar_t* pUnic = (wchar_t*)malloc(sizeof(wchar_t)*iSize);// wchar_t[iSize];
	if (NULL == pUnic)		goto handle_error;
	int32_t iCharNums = Ansi2Unic(ansi, pUnic, iSize);
	if (iCharNums <= 0)		goto handle_error;
	iCharNums = Unic2Utf8(pUnic, utf8, size);
	if (iCharNums <= 0)		goto handle_error;
	free(pUnic);
	return iCharNums;
handle_error:
	if (NULL != pUnic)	free(pUnic);
	return 0;
}

// cpp.
static std::wstring
Astr2Wstr(const std::string& ansi)
{
	int32_t  iSize = (strlen(ansi.c_str()) + 1);
	wchar_t* pUnic = new wchar_t[iSize];
	if (nullptr == pUnic)	goto handle_error;
	int32_t iCharNums = Ansi2Unic(ansi.c_str(), pUnic, iSize);
	if (iCharNums <= 0)		goto handle_error;
	delete[] pUnic;
	return std::wstring(pUnic);
handle_error:
	if (nullptr != pUnic)	delete[] pUnic;
	return std::wstring(L"");
}

static std::string
Wstr2Astr(const std::wstring& wstr)
{
	int32_t  iSize = (wcslen(wstr.c_str()) + 1);
	char* pAnsi = new char[iSize];
	if (nullptr == pAnsi)	goto handle_error;
	int32_t iCharNums = Unic2Ansi(wstr.c_str(), pAnsi, iSize);
	if (iCharNums <= 0)		goto handle_error;
	delete[] pAnsi;
	return std::string(pAnsi);
handle_error:
	if (nullptr != pAnsi)	delete[] pAnsi;
	return std::string("");
}

/**
*@外部调用常用函数
*/

static inline bool
ShellExec(std::string cmd, std::string arg, bool is_show)
{
	std::wstring w_cmd = Astr2Wstr(cmd);
	std::wstring w_arg = Astr2Wstr(arg);

	SHELLEXECUTEINFO ShExecInfo;
	ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = nullptr;
	ShExecInfo.lpVerb = nullptr;
	ShExecInfo.lpFile = w_cmd.c_str(); //can be a file as well  
	ShExecInfo.lpParameters = w_arg.c_str();
	ShExecInfo.lpDirectory = nullptr;
	ShExecInfo.nShow = 0;
	ShExecInfo.hInstApp = nullptr;
	BOOL ret = ShellExecuteEx(&ShExecInfo);
	WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
	DWORD dwCode = 0;
	GetExitCodeProcess(ShExecInfo.hProcess, &dwCode);
	if (dwCode)
		printf("shellExecuteCommand: [%s %s <ErrorCode=%d>] error!\n", cmd.c_str(), arg.c_str(), dwCode);
	CloseHandle(ShExecInfo.hProcess);
	return !dwCode;
}
#endif

/***************************4.功能类的安全封装***************************/
namespace AT
{
	using second_t = std::chrono::duration<long long>;
	using millis_t = std::chrono::duration<long long, std::milli>;
	using micros_t = std::chrono::duration<long long, std::micro>;
	using nanosd_t = std::chrono::duration<long long, std::nano >;

	class Timer
	{
	public:
		Timer() :	   m_begin(  std::chrono::high_resolution_clock::now()) { }
		void reset() { m_begin = std::chrono::high_resolution_clock::now(); }
		int64_t elapsed(bool s = false) const{ // default milliseconds.
			if (s) printf("elapsed: %lld ms\n", elapsed_milliseconds());
			return elapsed_milliseconds();
		}
		int64_t elapsed_nanoseconds()	const{
			return std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::high_resolution_clock::now()  - m_begin).count();
		}
		int64_t elapsed_microseconds()	const{
			return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::high_resolution_clock::now() - m_begin).count();
		}
		int64_t elapsed_milliseconds()	const{
			return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - m_begin).count();
		}
	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> m_begin;
	};
	
	template<typename T> class SafeStack
	{
	public:
		// If use right ref,bellow must be defined!
		SafeStack() {}
		SafeStack(SafeStack const& other) {
			std::lock_guard<std::mutex> locker(other.m_lock);
			m_data(other.m_data);
		}
		SafeStack& operator=(SafeStack const& other) {
			std::lock_guard<std::mutex> locker(other.m_lock);
			m_data = other.m_data;
			return *this;
		}
		SafeStack(SafeStack&& other) {// cons by moving _Right
			m_data(std::move(other));
		}
		SafeStack& operator=(SafeStack&& other) {
			m_data = std::move(other);
			return *this;
		}
		uint32_t size() const {
			std::lock_guard<std::mutex> locker(m_lock);
			return m_data.size();
		}
		void push(T&& value) {
			std::lock_guard<std::mutex> locker(m_lock);
			m_data.emplace(std::move(value));
			m_cond.notify_one();
		}
		void push(T&  value) {
			std::lock_guard<std::mutex> locker(m_lock);
			m_data.emplace(value);
			m_cond.notify_one();
		}
		void peek(T&  value) {
			std::unique_lock<std::mutex> locker(m_lock);
			m_cond.wait(locker, [this] {
				return !(m_data.empty() && !m_wake); }
			);
			if (!m_data.empty()) {
				value = m_data.top();
			}
		}
		void popd(T&  value) {
			std::unique_lock<std::mutex> locker(m_lock);
			m_cond.wait(locker, [this] {
				return !(m_data.empty() && !m_wake); }
			);
			if (!m_data.empty()) {
				value = m_data.top(); m_data.pop();
			}
		}
		bool try_peek(T&  value) {
			std::lock_guard<std::mutex> locker(m_lock);
			if (m_data.empty())
				return false;
			value = m_data.top();
			return true;
		}
		bool try_popd(T&  value) {
			std::lock_guard<std::mutex> locker(m_lock);
			if (m_data.empty())
				return false;
			value = m_data.top();
			m_data.pop();
			return true;
		}
		bool empty() const {
			std::lock_guard<std::mutex> locker(m_lock);
			return m_data.empty();
		}
		void clear() {
			std::lock_guard<std::mutex> locker(m_lock);
			while (!m_data.empty())
				m_data.pop();
		}
		void awake(bool iswakeup = false) {
			m_wake = iswakeup;
			m_cond.notify_all();
		}
	private:
		mutable std::mutex		m_lock;
		std::stack<T>			m_data;
		std::condition_variable m_cond;
		std::atomic<bool>		m_wake{ false };
	};

	template<typename T> class SafeQueue
	{
	public:
		// If use right ref,bellow must be defined!
		SafeQueue() {}
		SafeQueue(SafeQueue const& other) {
			std::lock_guard<std::mutex> locker(other.m_lock);
			m_data(other.m_data);
		}
		SafeQueue& operator=(SafeQueue const& other) {
			std::lock_guard<std::mutex> locker(other.m_lock);
			m_data = other.m_data;
			return *this;
		}
		SafeQueue(SafeQueue&& other) {// cons by moving _Right
			m_data(std::move(other));
		}
		SafeQueue& operator=(SafeQueue&& other) {
			m_data = std::move(other);
			return *this;
		}
		uint32_t size() const {
			std::lock_guard<std::mutex> locker(m_lock);
			return m_data.size();
		}
		void push(T&& value) {
			std::lock_guard<std::mutex> locker(m_lock);
			m_data.emplace(std::move(value));
			m_cond.notify_one();
		}
		void push(T&  value) {
			std::lock_guard<std::mutex> locker(m_lock);
			m_data.emplace(value);
			m_cond.notify_one();
		}
		void peek(T&  value) {
			std::unique_lock<std::mutex> locker(m_lock);
			m_cond.wait(locker, [this] {
				return !(m_data.empty() && !m_wake); }
			);
			if (!m_data.empty()) {
				value = m_data.front();
			}
		}
		void popd(T&  value) {
			std::unique_lock<std::mutex> locker(m_lock);
			m_cond.wait(locker, [this] {
				return !(m_data.empty() && !m_wake); }
			);
			if (!m_data.empty()) {
				value = m_data.front(); m_data.pop();
			}
		}
		bool try_peek(T&  value) {
			std::lock_guard<std::mutex> locker(m_lock);
			if (m_data.empty())
				return false;
			value = m_data.front();
			return true;
		}
		bool try_popd(T&  value) {
			std::lock_guard<std::mutex> locker(m_lock);
			if (m_data.empty())
				return false;
			value = m_data.front();
			m_data.pop();
			return true;
		}
		bool empty() const {
			std::lock_guard<std::mutex> locker(m_lock);
			return m_data.empty();
		}
		void clear() {
			std::lock_guard<std::mutex> locker(m_lock);
			while (!m_data.empty())
				m_data.pop();
		}
		void awake(bool iswakeup = false) {
			m_wake = iswakeup;
			m_cond.notify_all();
		}
	private:
		mutable std::mutex		m_lock;
		std::queue<T>			m_data;
		std::condition_variable m_cond;
		std::atomic<bool>		m_wake{ false };
	};
}

/***************************4.实体项目公共资源***************************/
typedef enum STATUS
{
	E_INVALID = -1,
	E_INITRES,

	E_STRTING,
	E_RUNNING,
	E_PAUSING,
	E_STARTED,
	E_STOPING,
	E_STOPPED,

	E_STRTERR,
	E_RUNNERR,
	E_STOPERR,
}STATUS;//Ext...

#define STANDARDTK								(10)
#define CHK_RETURN(x) do{ if(x) return; }		while (0)
#define SET_STATUS(x, y) do{ (x) = (y); }		while (0)

typedef enum PROPTS
{
	P_MINP = -1,
	P_BEGP, 
	P_SEEK,
	P_PAUS,
	P_ENDP,
	//...
	P_MAXP = sizeof(int64_t)*8,
}PROPTS;

#define SET_PROPERTY(x,y)						BSET(x,y)
#define PUR_PROPERTY(x)							( x = 0 )
#define CLR_PROPERTY(x,y)						BCLR(x,y)
#define CHK_PROPERTY(x,y)						BCHK(x,y)

#define MAX_AUDIO_Q_SIZE						(5)
#define MAX_VIDEO_Q_SIZE						(MAX_AUDIO_Q_SIZE)

//Demuxer->...->Enmuxer.
struct MPacket
{
	MPacket() {
		//std::cout << "MPacket().." << std::endl;
		pars = avcodec_parameters_alloc();
		ppkt = av_packet_alloc();
		assert(nullptr != ppkt && nullptr != pars);
	}
	~MPacket() {
		//std::cout << "~MPacket.." << std::endl;
		avcodec_parameters_free(&pars);
		av_packet_free(&ppkt);
	}
	int32_t				ssid{ -1 };//unique mark of the stream.
	int32_t				type{ -1 };//audio or video media type.
	int64_t				prop{  0 };//specif prop, such as seek.
	AVRational			sttb{  0 };//av_stream->timebase.
	double				upts{  0 };//user pts in second.<maybe disorder for video>
	AVRational			ufps{  0 };//user fps.eg.25.	
	AVCodecParameters*	pars{ nullptr };
	AVPacket* 			ppkt{ nullptr };
};

//Decoder->...->Encoder|Mrender.
struct MRframe
{
	MRframe() {
		pars = avcodec_parameters_alloc();
		pfrm = av_frame_alloc();
		assert(nullptr != pfrm && nullptr != pars);
	}
	~MRframe() {
		avcodec_parameters_free(&pars);
		av_frame_free(&pfrm);
	}
	int32_t				ssid{ -1 };//unique mark of the stream.
	int32_t				type{ -1 };//audio or video media type.
	int64_t				prop{  0 };//specif prop, such as seek.
	AVRational			sttb{  0 };//av_codec->timebase.
	double				upts{  0 };//user pts in second.<must be orderly>
	AVCodecParameters*	pars{ nullptr };
	AVFrame* 			pfrm{ nullptr };
};

static void
sleepMs(int64_t delay){
	return std::this_thread::sleep_for(std::chrono::milliseconds(delay));
}

static char*
averr2str(int num) 
{
	static char strerr[512] = { 0 };
	memset(strerr, 0, sizeof(strerr));
	return (av_strerror(num, strerr, sizeof(strerr))) ? NULL : strerr;
}

static void
audio_frame_freep(AVFrame** frame)
{
	av_frame_free(frame);
}

static AVFrame*
audio_frame_alloc(enum AVSampleFormat smp_fmt,
	uint64_t channel_layout, int32_t sample_rate, int32_t nb_samples)
{
	int32_t ret = 0;
	AVFrame *frame = nullptr;
	if (!(frame = av_frame_alloc())) {
		err("av_frame_alloc failed! frame=%p\n", frame);
		return nullptr;
	}
	frame->format		= smp_fmt;
	frame->sample_rate	= sample_rate;
	frame->nb_samples	= nb_samples;
	frame->channel_layout = channel_layout;
	frame->channels		= av_get_channel_layout_nb_channels(channel_layout);
	if ((ret = av_frame_get_buffer(frame, 0)) < 0){
		err("av_frame_get_buffer failed! ret=%d\n", ret);
		audio_frame_freep(&frame);
		return nullptr;
	}
	return frame;
}

static void
video_frame_freep(AVFrame** frame)
{
	av_frame_free(frame);
}

static AVFrame*
video_frame_alloc(enum AVPixelFormat pix_fmt, int32_t width, int32_t height)
{
	AVFrame *frame = nullptr;
	int32_t ret = 0;
	if (!(frame = av_frame_alloc())) {
		err("av_frame_alloc failed! frame=%p\n", frame);
		return nullptr;
	}
	frame->format = pix_fmt;
	frame->width  = width;
	frame->height = height;
	if ((ret = av_frame_get_buffer(frame, 0)) < 0)
	{/* allocate the buffers for the frame->data[] */
		err("av_frame_get_buffer failed! ret=%d\n", ret);
		video_frame_freep(&frame);
		return nullptr;
	}
	return frame;
}

/**
 *@重采样函数,返回一个指针指向采样后数据，有可能是输入的指针，不好。
 */
static bool
video_rescale(SwsContext **pswsctx,AVFrame *dst_frame, AVFrame *src_frame)
{
	if (!src_frame || !dst_frame || !pswsctx)
		return false;

	if (	src_frame->width  != dst_frame->width 
		||  src_frame->height != dst_frame->height
		||	src_frame->format != dst_frame->format )
	{	//  Auto call sws_freeContext() if reset.
		//  1.获取转换句柄. [sws_getCachedContext will check cfg and reset *pswsctx]
		if (nullptr == (*pswsctx = sws_getCachedContext( *pswsctx,
			src_frame->width, src_frame->height, (AVPixelFormat)src_frame->format,
			dst_frame->width, dst_frame->height, (AVPixelFormat)dst_frame->format,
			SWS_FAST_BILINEAR, nullptr, nullptr, nullptr)))
			return false;
		//  2.进行图像转换. [0-from begin]
		if (sws_scale(*pswsctx, (const uint8_t* const*)src_frame->data, src_frame->linesize, 0, dst_frame->height, dst_frame->data, dst_frame->linesize) <= 0)
			video_frame_freep(&dst_frame);//inner reset dst_frame = nullptr;
	} else {
		if (av_frame_copy(dst_frame, src_frame))
			return false;
		if (av_frame_copy_props(dst_frame, src_frame))
			return false;
	}
	return true;
}

static bool
audio_resmple(SwrContext **pswrctx, AVFrame *dst_frame, const AVFrame *src_frame )
{
	if (!src_frame || !dst_frame || !pswrctx)
		return false;
	int32_t dst_nb_samples = -1;
	if (	src_frame->sample_rate	!= dst_frame->sample_rate
		||	src_frame->channel_layout != dst_frame->channel_layout
		||	src_frame->format		!= dst_frame->format
		||  src_frame->nb_samples	!= dst_frame->nb_samples)
	{
		//  1.获取转换句柄.
		if (nullptr == *pswrctx)
		{// Because we can't get SwrContext' sub members,so can't cached.
			if (nullptr == (*pswrctx = swr_alloc_set_opts(*pswrctx, 
				dst_frame->channel_layout, (AVSampleFormat)dst_frame->format, dst_frame->sample_rate,
				src_frame->channel_layout, (AVSampleFormat)src_frame->format, src_frame->sample_rate,
				0, 0)))
				return false;
			if (swr_init(*pswrctx) < 0) {
				swr_free(pswrctx);
				return false;
			}
		}		
		//  2.进行音频转换.
		if ((dst_nb_samples = swr_convert(*pswrctx, dst_frame->data, dst_frame->nb_samples,
			(const uint8_t**)src_frame->data, src_frame->nb_samples)) <= 0)		
			return false;
		if (dst_nb_samples != dst_frame->nb_samples)
			war("expect nb_samples=%d, dst_nb_samples=%d\n", dst_frame->nb_samples, dst_nb_samples);
	} else {
		if (av_frame_copy(dst_frame, src_frame))
			return false;
		if (av_frame_copy_props(dst_frame, src_frame))
			return false;
	}
	return true;
}

static inline char*
av_pcmalaw_clone2buffer(AVFrame *frame, char* data, int32_t size)
{
	if (nullptr == frame || nullptr == data || size <= 0)
		return nullptr;

	memset(data, 0, size);
	int32_t bytes_per_sample = av_get_bytes_per_sample((enum AVSampleFormat)frame->format);
	int32_t frame_size = frame->channels * frame->nb_samples * bytes_per_sample;
	char*   frame_data = data, *p_cur_ptr = frame_data;

	if (frame_size > 0 && size >= frame_size)
	{// dump pcm
	 // 1.For packet sample foramt and 1 channel,we just store pcm data in byte order.
		if ((1 == frame->channels)
			|| (frame->format >= AV_SAMPLE_FMT_U8 &&  frame->format <= AV_SAMPLE_FMT_DBL))
		{//linesize[0] maybe 0 or has padding bits,so calculate the real size by ourself.
			memcpy(p_cur_ptr, frame->data[0], frame_size);
		}
		else {//2.For plane sample foramt, we must store pcm datas interleaved. [LRLRLR...LR].
			for (int32_t i = 0; i < frame->nb_samples; ++i)
			{
				memcpy(p_cur_ptr, frame->data[0] + i*bytes_per_sample, bytes_per_sample);
				p_cur_ptr += bytes_per_sample;
				memcpy(p_cur_ptr, frame->data[1] + i*bytes_per_sample, bytes_per_sample);
				p_cur_ptr += bytes_per_sample;
			}
		}
		return frame_data;
	}
	return nullptr;
}

static inline char*
av_yuv420p_clone2buffer(AVFrame *frame, char* data, int32_t size)
{
	if (nullptr == frame || nullptr == data || size <= 0)
		return nullptr;

	memset(data, 0, size);
	int32_t y_size = frame->width * frame->height;
	int32_t frame_size = y_size * 3 / 2;
	char*   frame_data = data, *p_cur_ptr = frame_data;

	if (frame_size > 0 && size >= frame_size)
	{// dump yuv420
		for (int32_t i = 0; i < frame->height; ++i)
			memcpy(p_cur_ptr + frame->width*i, frame->data[0] + frame->linesize[0] * i, frame->width);
		p_cur_ptr += y_size;
		for (int32_t i = 0; i < frame->height / 2; ++i)
			memcpy(p_cur_ptr + i*frame->width / 2, frame->data[1] + frame->linesize[1] * i, frame->width / 2);
		p_cur_ptr += y_size / 4;
		for (int32_t i = 0; i < frame->height / 2; ++i)
			memcpy(p_cur_ptr + i*frame->width / 2, frame->data[2] + frame->linesize[2] * i, frame->width / 2);
		return frame_data;
	}
	return nullptr;
}

static inline void
av_pcmalaw_freep(char* pcm_data)
{
	free((void*)pcm_data);
}

static inline char*
av_pcmalaw_clone(AVFrame *frame)
{
	assert(nullptr != frame);

	int32_t frame_size = av_samples_get_buffer_size(nullptr, frame->channels, frame->nb_samples, (enum AVSampleFormat)frame->format, 1);
	char*	frame_data = (char*)calloc(1, frame_size);
	return av_pcmalaw_clone2buffer(frame, frame_data, frame_size);
}

static inline void
av_yuv420p_freep(char* yuv420_data)
{
	free(yuv420_data);
}

static inline char*
av_yuv420p_clone(AVFrame *frame)
{
	assert(nullptr != frame);
	int32_t frame_size = frame->width * frame->height * 3 / 2;
	char*	frame_data = (char*)calloc(1, frame_size);
	return av_yuv420p_clone2buffer(frame, frame_data, frame_size);
}

static inline void
debug_write_alaw(AVFrame *frame)
{
	if (nullptr == frame)
		return;

	char name[128] = {};
	int32_t bytes_per_sample = av_get_bytes_per_sample((enum AVSampleFormat)frame->format);
	sprintf(name, "./test_%dx%dx%dx%d_%lld.pcm",
		frame->sample_rate, frame->channels, frame->nb_samples, bytes_per_sample, time(nullptr));

	static FILE *fp = nullptr;
	if (nullptr == fp)
		fp = fopen(name, "wb+");
	if (nullptr == fp)
		return;

#if 0
	int32_t frame_size = av_samples_get_buffer_size(nullptr, frame->channels, frame->nb_samples, (enum AVSampleFormat)frame->format, 1);
	char* pcm_data = av_pcm_clone(frame);
	fwrite(pcm_data, 1, frame_size, fp);
	av_pcm_freep(pcm_data);
#else
	// 1.For packet sample foramt and 1 channel,we just store pcm data in byte order.
	if ((1 == frame->channels)
		|| (frame->format >= AV_SAMPLE_FMT_U8 &&  frame->format <= AV_SAMPLE_FMT_DBL))
	{//linesize[0] maybe 0 or has padding bits,so calculate the real size by ourself.
		int32_t frame_size = frame->channels*frame->nb_samples*bytes_per_sample;
		fwrite(frame->data[0], 1, frame_size, fp);
	}
	else {//2.For plane sample foramt, we must store pcm datas interleaved. [LRLRLR...LR].
		for (int32_t i = 0; i < frame->nb_samples; ++i)
		{
			fwrite(frame->data[0] + i*bytes_per_sample, 1, bytes_per_sample, fp);
			fwrite(frame->data[1] + i*bytes_per_sample, 1, bytes_per_sample, fp);
		}
	}
#endif
	fflush(fp);
}

static inline void
debug_write_420p(AVFrame *frame)
{
	if (nullptr == frame)
		return;

	char name[128] = {};
	sprintf(name, "./test_%dx%d_%lld.yuv", frame->width, frame->height, time(nullptr));

	static FILE *fp = nullptr;
	if (nullptr == fp)
		fp = fopen(name, "wb+");
	if (nullptr == fp)
		return;

#if 0//剥离AVFrame中的裸数据。
	char* yuv420_data = av_yuv420p_clone(frame);
	fwrite(yuv420_data, 1, frame->width*frame->height * 3 / 2, fp);
	av_yuv420p_freep(yuv420_data);
#else
	//write yuv420p(I420)
	for (int32_t i = 0; i < frame->height; ++i)
		fwrite(frame->data[0] + frame->linesize[0] * i, 1, frame->width, fp);
	for (int32_t i = 0; i < frame->height / 2; ++i)
		fwrite(frame->data[1] + frame->linesize[1] * i, 1, frame->width / 2, fp);
	for (int32_t i = 0; i < frame->height / 2; ++i)
		fwrite(frame->data[2] + frame->linesize[2] * i, 1, frame->width / 2, fp);
#endif
	fflush(fp);
}

static inline void
debug_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt, const char *tag)
{
	AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;
	printf("[PACKET]%s #stream-%d# (%d/%d): iskey=%d, pts:%lld pts_time=%0.6g(s) dts:%lld dts_time=%0.6g(s) duration=%lld, duration_time=%0.6g(ms)\n",
		tag, pkt->stream_index, time_base->num, time_base->den, pkt->flags,
		pkt->pts, pkt->pts*av_q2d(*time_base), pkt->dts, pkt->dts*av_q2d(*time_base), pkt->duration, 1000 * pkt->duration*av_q2d(*time_base));
}

static inline void
debug_frames(const AVCodecContext *codec_ctx, const AVFrame *frm, const char *tag)
{
	bool is_audio = (frm->width == 0);
	const AVRational *time_base = &codec_ctx->time_base;
	printf("[FRAMES]%s #baudio-%d# (%d/%d): iskey=%d, pts:%lld pts_time=%0.6g(s) -->pkt_dts:%lld pkt_duration=%lld pkt_duration_time=%0.6g(ms)\n",
		tag, is_audio, time_base->num, time_base->den, frm->key_frame,
		frm->pts, frm->pts*av_q2d(*time_base), frm->pkt_dts, frm->pkt_duration, 1000 * frm->pkt_duration*av_q2d(*time_base));
}
