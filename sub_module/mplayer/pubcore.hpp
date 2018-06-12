
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
#ifdef  WIN32
#include <windows.h>
#include <corecrt_io.h>
#endif
#ifdef  __LINUX__
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
#endif

#ifdef __cplusplus
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

// 1.位操作宏函数
#define BCUT_04(x,n)                ( ( (x) >> (n) ) & 0x0F )           // 获取x的(n~n+03)位
#define BCUT_08(x,n)                ( ( (x) >> (n) ) & 0xFF )           // 获取x的(n~n+07)位
#define BCUT_16(x,n)                ( ( (x) >> (n) ) & 0xFFFF )         // 获取x的(n~n+15)位

#define BSET(x,n)                   ( (x) |=  ( 1 << (n) ) )            // 设置x的第n位为"1"
#define BCLR(x,n)                   ( (x) &= ~( 1 << (n) ) )            // 清除x的第n位为"0"
#define BCHK(x,n)                   ( ( (x) >> (n) ) & 1 )              // 检测某位是否是"1"

#define BYTE_ORDR                   ( __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ )
#define SWAP_16(x)                  ((x>>08&0xff)|(x<<08&0xff00))         // 大小端字节序转换
#define SWAP_24(x)                  ((x>>16&0xff)|(x<<16&0xff0000)|x&0xff00)  
#define SWAP_32(x)                  ((x>>24&0xff)|(x>>8&0xff00)|(x << 8 & 0xff0000) | (x << 24 & 0xff000000))  

// 2.字符数组操作
#define ELEMENTS(s)					( sizeof(s)/sizeof(s[0]) )
#define FREESIZE(s)					( sizeof(s)- strlen(s)-1 )

// 3.通用函数封装
static inline bool
effective(const char* dev_name)
{
	return (nullptr != dev_name  && '\0' != dev_name[0] && ' ' != dev_name[0]
		&& '\t' != dev_name[0] && '\n' != dev_name[0]);
}


static inline void
replace_c(std::string& str, char* old, char* mew)
{
	std::string::size_type pos(0);
	while (true)
	{
		pos = str.find(old, pos);
		if (pos != (std::string::npos))
		{
			str.replace(pos, strlen(old), mew);
			pos += 2;//注意是加2，为了跳到下一个反斜杠  
		}else {
			break;
		}
	}
}

/***************************3.平台相关转换函数***************************/
#ifdef WIN32
static inline int32_t
Utf82Uni(const char* utf8, wchar_t* unicode, uint32_t size)
{
	if (!utf8 || !strlen(utf8)) {
		return 0;
	}
	wmemset(unicode, 0, size);
	int32_t dwUnicodeLen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	size_t num = dwUnicodeLen * sizeof(wchar_t);
	if (num > size) {
		return 0;
	}
	MultiByteToWideChar(CP_UTF8, 0, utf8, -1, unicode, dwUnicodeLen);
	return dwUnicodeLen;
}

static inline int32_t
Uni2Utf8(const wchar_t* unicode, char* utf8, int32_t size)
{
	if (!unicode || !wcslen(unicode)) {
		return 0;
	}
	memset(utf8, 0, size);
	int32_t len;
	len = WideCharToMultiByte(CP_UTF8, 0, unicode, -1, NULL, 0, NULL, NULL);
	if (len > size) {
		return 0;
	}
	WideCharToMultiByte(CP_UTF8, 0, unicode, -1, utf8, len, NULL, NULL);
	return len;
}

static inline int32_t
Mul2Utf8(const char *mult, char* utf8, int32_t size)
{
	if (nullptr == mult || nullptr == utf8)
		return 0;
	memset(utf8, 0, size);
	//返回接受字符串所需缓冲区的大小，已经包含字符结尾符'\0',iSize =wcslen(srcString)+1
	int32_t iSize = MultiByteToWideChar(CP_ACP, 0, mult, -1, NULL, 0);
	wchar_t* pUnicode = (wchar_t *)malloc(iSize * sizeof(wchar_t));
	MultiByteToWideChar(CP_ACP, 0, mult, -1, pUnicode, iSize);
	int32_t len = 0;
	len = WideCharToMultiByte(CP_UTF8, 0, pUnicode, -1, NULL, 0, NULL, NULL);
	if (len > size) {
		return 0;
	}
	WideCharToMultiByte(CP_UTF8, 0, pUnicode, -1, utf8, len, NULL, NULL);
	free(pUnicode);
	return len;
}

static std::wstring
str2wstr(const std::string& str)
{
	LPCSTR pszSrc = str.c_str();
	int32_t nLen = MultiByteToWideChar(CP_ACP, 0, pszSrc, -1, NULL, 0);
	if (nLen == 0)
		return std::wstring(L"");

	wchar_t* pwszDst = new wchar_t[nLen];
	if (!pwszDst)
		return std::wstring(L"");

	MultiByteToWideChar(CP_ACP, 0, pszSrc, -1, pwszDst, nLen);
	std::wstring wstr(pwszDst);
	delete[] pwszDst;
	pwszDst = NULL;

	return wstr;
}

static std::string
wstr2str(const std::wstring& wstr)
{
	LPCWSTR pwszSrc = wstr.c_str();
	int32_t nLen = WideCharToMultiByte(CP_ACP, 0, pwszSrc, -1, NULL, 0, NULL, NULL);
	if (nLen == 0)	return std::string("");
	char* pszDst = new char[nLen];
	if (!pszDst)	return std::string("");
	WideCharToMultiByte(CP_ACP, 0, pwszSrc, -1, pszDst, nLen, NULL, NULL);
	std::string str(pszDst);
	delete[] pszDst;
	return str;
}

static inline bool
shellExecuteCommand(std::string cmd, std::string arg, bool is_show)
{
	std::wstring w_cmd = str2wstr(cmd);
	std::wstring w_arg = str2wstr(arg);

	SHELLEXECUTEINFO ShExecInfo;
	ShExecInfo.cbSize = sizeof(SHELLEXECUTEINFO);
	ShExecInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	ShExecInfo.hwnd = NULL;
	ShExecInfo.lpVerb = NULL;
	ShExecInfo.lpFile = w_cmd.c_str(); //can be a file as well  
	ShExecInfo.lpParameters = w_arg.c_str();
	ShExecInfo.lpDirectory = NULL;
	ShExecInfo.nShow = 0;
	ShExecInfo.hInstApp = NULL;
	BOOL ret = ShellExecuteEx(&ShExecInfo);
	WaitForSingleObject(ShExecInfo.hProcess, INFINITE);
	DWORD dwCode = 0;
	GetExitCodeProcess(ShExecInfo.hProcess, &dwCode);
	if (dwCode)
		printf("shellExecuteCommand: [%s %s <ErrorCode=%d>] error!\n", cmd.c_str(), arg.c_str(), dwCode);
	CloseHandle(ShExecInfo.hProcess);
	return !dwCode;
}
#else

#endif

/***************************3.兼容旧的调试机制***************************/
typedef enum log_rank
{
	LOG_ERR, LOG_WAR,
	LOG_MSG, LOG_DBG,
}log_rank_t;//Warning: Just debug for test,must use log system in project.
const static log_rank_t	rank				= LOG_MSG;
#define err( format, ... )do{ if( LOG_ERR <= rank )\
	fprintf(stderr, "[<%s>:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#define war( format, ... )do{ if( LOG_WAR <= rank )\
	fprintf(stderr, "[<%s>:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#define msg( format, ... )do{ if( LOG_MSG <= rank )\
	fprintf(stderr, "[<%s>:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#define dbg( format, ... )do{ if( LOG_DBG <= rank )\
	fprintf(stderr, "[<%s>:%d] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__);}while(0)
#define out( format, ... )do{ fprintf(stderr, format, ##__VA_ARGS__); }while(0)

/***************************4.公用类的安全封装***************************/
namespace AT
{
	using second_t = std::chrono::duration<int32_t>;
	using millis_t = std::chrono::duration<int32_t, std::milli>;
	using micros_t = std::chrono::duration<int32_t, std::micro>;
	using nanosd_t = std::chrono::duration<int32_t, std::nano >;

	class Timer
	{
	public:
		Timer() :	   m_begin(std::chrono::high_resolution_clock::now()) { }
		void reset() { m_begin = std::chrono::high_resolution_clock::now(); }
		int64_t elapsed() const{ // default output milliseconds.
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
		int64_t elapsed_seconds()	const{
			return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::high_resolution_clock::now() - m_begin).count();
		}
		int64_t elapsed_minutes()	const{
			return std::chrono::duration_cast<std::chrono::minutes>(std::chrono::high_resolution_clock::now() - m_begin).count();
		}
		int64_t elapsed_hours()		const{
			return std::chrono::duration_cast<std::chrono::hours>(std::chrono::high_resolution_clock::now()   - m_begin).count();
		}
	private:
		std::chrono::time_point<std::chrono::high_resolution_clock> m_begin;
	};
	
	class Event
	{//TODO:
	public:
		Event() {}
		~Event() {}
	private:
	};

	template<typename T> class safe_stack
	{
	public:
		safe_stack() {}
		safe_stack(safe_stack const& other) {
			std::lock_guard<std::mutex> locker(other.stack_lock);
			stack_data = other.stack_data;
		}
		uint32_t size()  const {
			std::lock_guard<std::mutex> locker(stack_lock);
			return stack_data.size();
		}
		void push(T  value) {
			std::lock_guard<std::mutex> locker(stack_lock);
			stack_data.push(value);
			stack_cond.notify_one();
		}
		void peek(T& value) {
			std::unique_lock<std::mutex> locker(stack_lock);
			stack_cond.wait(locker, [this] {return !stack_data.empty(); });
			value = stack_data.front();
		}
		void popd(T& value) {
			std::unique_lock<std::mutex> locker(stack_lock);
			stack_cond.wait(locker, [this] {return !stack_data.empty(); });
			value = stack_data.front();
			stack_data.pop();
		}
		bool try_peek(T& value) {
			std::lock_guard<std::mutex> locker(stack_lock);
			if (stack_data.empty())
				return false;
			value = stack_data.front();
			return true;
		}
		bool try_popd(T& value) {
			std::lock_guard<std::mutex> locker(stack_lock);
			if (stack_data.empty())
				return false;
			value = stack_data.front();
			stack_data.pop();
			return true;
		}
		bool empty() const {
			std::lock_guard<std::mutex> locker(stack_lock);
			return stack_data.empty();
		}
		void clear() {
			std::lock_guard<std::mutex> locker(stack_lock);
			while (!stack_data.empty())
				stack_data.pop();
		}
	private:
		mutable std::mutex		stack_lock;
		std::stack<T>			stack_data;
		std::condition_variable stack_cond;
	};

	template<typename T> class safe_queue
	{
	public:
		safe_queue() {}
		safe_queue(safe_queue const& other) {
			std::lock_guard<std::mutex> locker(other.queue_lock);
			queue_data = other.queue_data;
		}
		uint32_t size()  const {
			std::lock_guard<std::mutex> locker(queue_lock);
			return queue_data.size();
		}
		void push(T  value) {
			std::lock_guard<std::mutex> locker(queue_lock);
			queue_data.push(value);
			queue_cond.notify_one();
		}
		void peek(T& value) {
			std::unique_lock<std::mutex> locker(queue_lock);
			queue_cond.wait(locker, [this] {return !queue_data.empty(); });
			value = queue_data.front();
		}
		void popd(T& value) {
			std::unique_lock<std::mutex> locker(queue_lock);
			queue_cond.wait(locker, [this] {return !queue_data.empty(); });
			value = queue_data.front();
			queue_data.pop();
		}
		bool try_peek(T& value) {
			std::lock_guard<std::mutex> locker(queue_lock);
			if (queue_data.empty())
				return false;
			value = queue_data.front();
			return true;
		}
		bool try_popd(T& value) {
			std::lock_guard<std::mutex> locker(queue_lock);
			if (queue_data.empty())
				return false;
			value = queue_data.front();
			queue_data.pop();
			return true;
		}
		bool empty() const {
			std::lock_guard<std::mutex> locker(queue_lock);
			return queue_data.empty();
		}
		void clear() {
			std::lock_guard<std::mutex> locker(queue_lock);
			while (!queue_data.empty())
				queue_data.pop();
		}
	private:
		mutable std::mutex		queue_lock;
		std::queue<T>			queue_data;
		std::condition_variable queue_cond;
	};
}

/***************************4.具体项目公共资源***************************/
typedef enum STATUS
{
	E_INVALID = -1,
	E_INITRES,
	E_STRTING,
	E_STARTED,
	E_STOPING,
	E_STOPPED,
	E_PAUSING,
}STATUS;//Ext...

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

#define MAX_AUDIO_Q_SIZE						   (1*43)
#define MAX_VIDEO_Q_SIZE						   (1*25)

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
	int32_t				type{ -1 };//audio or video media type.
	int64_t				prop{  0 };//specif prop, such as seek.
	AVRational			sttb{  0 };//av_codec->timebase.
	double				upts{  0 };//user pts in second.<must be orderly>
	AVCodecParameters*	pars{ nullptr };
	AVFrame* 			pfrm{ nullptr };
};

static void
av_a_frame_freep(AVFrame** frame)
{
	av_frame_free(frame);
}

static AVFrame*
av_a_frame_alloc(enum AVSampleFormat smp_fmt,
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
	if ((ret = av_frame_get_buffer(frame, 0)) < 0)
	{
		err("av_frame_get_buffer failed! ret=%d\n", ret);
		av_a_frame_freep(&frame);
		return nullptr;
	}
	return frame;
}

static void
av_v_frame_freep(AVFrame** frame)
{
	av_frame_free(frame);
}

static AVFrame*
av_v_frame_alloc(enum AVPixelFormat pix_fmt, int32_t width, int32_t height)
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
		av_v_frame_freep(&frame);
		return nullptr;
	}
	return frame;
}

static AVFrame*
rescale(SwsContext **pswsctx, AVFrame*  avframe,
	int32_t dst_w, int32_t dst_h, int32_t dst_f)
{
	if (nullptr == avframe || nullptr == pswsctx)
		return nullptr;

	AVFrame	 *src_frame = avframe, *dst_frame = src_frame;

	if (src_frame->width != dst_w || src_frame->height != dst_h
		|| src_frame->format != dst_f)
	{	//  Auto call sws_freeContext() if reset.
		//  1.获取转换句柄. [sws_getCachedContext will check cfg and reset *pswsctx]
		if (nullptr == (*pswsctx = sws_getCachedContext(*pswsctx,
			src_frame->width, src_frame->height, (AVPixelFormat)src_frame->format,
			dst_w, dst_h, (AVPixelFormat)dst_f,
			SWS_FAST_BILINEAR, NULL, NULL, NULL)))
			return nullptr;
		//  2.申请缓存数据. []
		if (nullptr == (dst_frame = av_v_frame_alloc((AVPixelFormat)dst_f, dst_w, dst_h)))
			return nullptr;
		//  3.进行图像转换. [0-from begin]
		if (sws_scale(*pswsctx, (const uint8_t* const*)src_frame->data, src_frame->linesize, 0, dst_frame->height, dst_frame->data, dst_frame->linesize) <=0 )
			av_v_frame_freep(&dst_frame);//inner reset dst_frame = nullptr;
	}
	return dst_frame;
}

static AVFrame*
resmple(SwrContext **pswrctx, AVFrame*  avframe,
	int32_t dst_s, int32_t dst_n, uint64_t dst_l, int32_t dst_f)
{
	if (nullptr == avframe || nullptr == pswrctx)
		return nullptr;

	AVFrame	 *src_frame = avframe, *dst_frame = src_frame;
	int32_t dst_nb_samples = -1;

	if (src_frame->sample_rate != dst_s || src_frame->nb_samples != dst_n ||
		src_frame->channel_layout != dst_l || src_frame->format != dst_f)
	{
		//  1.获取转换句柄.
		if (nullptr == *pswrctx)
		{
			if (nullptr == (*pswrctx = swr_alloc_set_opts(*pswrctx, dst_l, (AVSampleFormat)dst_f, dst_s,
				src_frame->channel_layout, (AVSampleFormat)src_frame->format, src_frame->sample_rate,
				0, 0)))
				return nullptr;
			if (swr_init(*pswrctx) < 0) {
				swr_free(pswrctx);
				return nullptr;
			}
		}
		//  2.申请缓存数据.
		if (nullptr == (dst_frame = av_a_frame_alloc((AVSampleFormat)dst_f, dst_l, dst_s, dst_n)))
			return nullptr;
		//  3.进行音频转换.
		if ((dst_nb_samples = swr_convert(*pswrctx, dst_frame->data, dst_frame->nb_samples,
			(const uint8_t**)src_frame->data, src_frame->nb_samples)) <= 0)
		{
			av_a_frame_freep(&dst_frame);
			return nullptr;
		}
		if (dst_nb_samples != dst_frame->nb_samples)
			war("expect nb_samples=%d, dst_nb_samples=%d\n", dst_frame->nb_samples, dst_nb_samples);
	}
	return dst_frame;
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
	assert(NULL != frame);
	int32_t frame_size = frame->width * frame->height * 3 / 2;
	char*	frame_data = (char*)calloc(1, frame_size);
	return av_yuv420p_clone2buffer(frame, frame_data, frame_size);
}

static inline void
debug_write_alaw(AVFrame *frame)
{
	if (NULL == frame)
		return;

	char name[128] = {};
	int32_t bytes_per_sample = av_get_bytes_per_sample((enum AVSampleFormat)frame->format);
	sprintf(name, "./test_%dx%dx%dx%d_%lld.pcm",
		frame->sample_rate, frame->channels, frame->nb_samples, bytes_per_sample, time(NULL));

	static FILE *fp = NULL;
	if (NULL == fp)
		fp = fopen(name, "wb+");
	if (NULL == fp)
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
	if (NULL == frame)
		return;

	char name[128] = {};
	sprintf(name, "./test_%dx%d_%lld.yuv", frame->width, frame->height, time(NULL));

	static FILE *fp = NULL;
	if (NULL == fp)
		fp = fopen(name, "wb+");
	if (NULL == fp)
		return;

#if 0//剥离AVFrame中的裸数据。
	char* yuv420_data = av_yuv420p_clone(frame);
	fwrite(yuv420_data, 1, frame->width*frame->height * 3 / 2, fp);
	av_yuv420p_freep(yuv420_data);
#else
	//write yuv420p
	for (int32_t i = 0; i < frame->height; ++i)
		fwrite(frame->data[0] + frame->linesize[0] * i, 1, frame->width, fp);
	for (int32_t i = 0; i < frame->height / 2; ++i)
		fwrite(frame->data[1] + frame->linesize[1] * i, 1, frame->width / 2, fp);
	for (int32_t i = 0; i < frame->height / 2; ++i)
		fwrite(frame->data[2] + frame->linesize[2] * i, 1, frame->width / 2, fp);
#endif
	fflush(fp);
}

