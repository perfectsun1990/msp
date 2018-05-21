
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

#ifdef __cplusplus
extern "C"
{
#endif
#include <SDL.h>
#include <SDL_thread.h>

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

#undef main //NOTE:SDL2 define main micro.
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include <iostream>
#include <future>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>
#include <memory>
#include <thread>
#include <mutex>
#include <queue>
#include <condition_variable>
#include <functional>
#include <cstdio>
#include <cassert>
#endif

#ifndef __WIN32__
    #ifndef __LINUX__
        #define __LINUX__
    #endif
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

#ifdef  __WIN32__
#include <windows.h>
#include <corecrt_io.h>
#endif

/***************************4.公用函数或宏封装***************************/

// 1.位操作宏函数
#define BCUT_04(x,n)                ( ( (x) >> (n) ) & 0x0F )           // 获取x的(n~n+03)位
#define BCUT_08(x,n)                ( ( (x) >> (n) ) & 0xFF )           // 获取x的(n~n+07)位
#define BCUT_16(x,n)                ( ( (x) >> (n) ) & 0xFFFF )         // 获取x的(n~n+15)位

#define BSET(x,n)                   ( (x) |=  ( 1 << (n) ) )            // 设置x的第n位为"1"
#define BCLR(x,n)                   ( (x) &= ~( 1 << (n) ) )            // 清除x的第n位为"0"
#define BCHK(x,n)                   ( ( (x) >> (n) ) & 1 )              // 检测某位是否是"1"

#define BYTE_ORDR                   ( __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__ )
#define SWAP_16(x)                  ((x>>8&0xff)|(x<<8&0xff00))         // 大小端字节序转换
#define SWAP_24(x)                  ((x>>16&0xff)|(x<<16&0xff0000)|x&0xff00)  
#define SWAP_32(x)                  ((x>>24&0xff)|(x>>8&0xff00)|(x << 8 & 0xff0000) | (x << 24 & 0xff000000))  

// 2.字符数组操作
#define ELEMENTS(s)              ( sizeof(s)/sizeof(s[0]) )
#define FREESIZE(s)              ( sizeof(s)- strlen(s)-1 )
#define MAX_AUDIO_Q_SIZE							  (1)
#define MAX_VIDEO_Q_SIZE							  (1)

using second_t = std::chrono::duration<int32_t>;
using millis_t = std::chrono::duration<int32_t, std::milli>;
using micros_t = std::chrono::duration<int32_t, std::micro>;
using nannos_t = std::chrono::duration<int32_t, std::nano >;

typedef enum STATUS
{
	E_INVALID = -1,
	E_INITRES,
	E_STRTING,
	E_STARTED,
	E_STOPING,
	E_STOPPED,
	E_DESTROY,
}STATUS;//Ext...

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
#define SET_PROPERTY(x,y)  BSET(x,y)
#define PUR_PROPERTY(x)	   ( x = 0 )
#define CLR_PROPERTY(x,y)  BCLR(x,y)
#define CHK_PROPERTY(x,y)  BCHK(x,y)

//Demuxer->...->Enmuxer.
struct MPacket
{
	MPacket() {
		//std::cout << "MPacket().." << std::endl;
		pars = avcodec_parameters_alloc();
		ppkt = av_packet_alloc();
	}
	~MPacket() {
		//std::cout << "~MPacket.." << std::endl;
		avcodec_parameters_free(&pars);
		av_packet_free(&ppkt);
	}
	int32_t				type{ -1 };//audio or video media type.
	int64_t				prop{  0 };//specif prop, such as seek.
	double				upts{  0 };//user pts in ms.
	AVRational			ufps{  0 };//user fps.eg.25.
	AVRational			sttb{  0 };//stream->tb.
	AVCodecParameters*	pars{ nullptr };
	AVPacket* 			ppkt{ nullptr };
};

//Decoder->...->Encoder|Mrender.
struct MRframe
{
	MRframe() {
		pars = avcodec_parameters_alloc();
		pfrm = av_frame_alloc();
	}
	~MRframe() {
		avcodec_parameters_free(&pars);
		av_frame_free(&pfrm);
	}
	int32_t				type{ -1 };//audio or video media type.
	int64_t				prop{  0 };//specif prop, such as seek.
	double				upts{  0 };//user pts in ms.
	AVRational			sttb{  0 };//stream->tb.
	AVCodecParameters*	pars{ nullptr };
	AVFrame* 			pfrm{ nullptr };
};

//Mrender->...->Encoder|Mrender.
struct AVCodecPars
{
	int32_t		audioformat{ 0 };
	int32_t		videoformat{ 0 };
	int32_t		width{ 0 };
	int32_t		height{ 0 };
	int32_t		sample_rate{ 0 };
	int32_t		nb_channels{ 0 };
	int32_t		nb_asamples{ 0 };
};
struct AVRenderFrm :
	public AVCodecPars
{
	int32_t		type{ -1 };
	int32_t		size{  0 };
	double		upts{  0 };//pts in ms.
	char*		data{ nullptr };
};

static inline bool
effective(const char* dev_name)
{
	return (NULL != dev_name  && '\0' != dev_name[0] && ' ' != dev_name[0]
		&& '\t' != dev_name[0] && '\n' != dev_name[0]);
}

static inline void
av_pcm_freep(char* pcm_data)
{
	free((void*)pcm_data);
}

static inline char*
av_pcm_clone(AVFrame *frame)
{
	assert(NULL != frame);

	int32_t bytes_per_sample = av_get_bytes_per_sample((enum AVSampleFormat)frame->format);
	char* p_cur_ptr = NULL, *pcm_data = NULL;
	if (bytes_per_sample <= 0)
		return NULL;


	// 1.For packet sample foramt and 1 channel,we just store pcm data in byte order.
	if ((1 == frame->channels)
		|| (frame->format >= AV_SAMPLE_FMT_U8 &&  frame->format <= AV_SAMPLE_FMT_DBL))
	{//linesize[0] maybe 0 or has padding bits,so calculate the real size by ourself.		
		int32_t frame_size = frame->channels*frame->nb_samples*bytes_per_sample;
		p_cur_ptr = pcm_data = (char*)calloc(1, frame_size);
		memcpy(p_cur_ptr, frame->data[0], frame_size);
	}else{//2.For plane sample foramt, we must store pcm datas interleaved. [LRLRLR...LR].
		int32_t frame_size = frame->channels*frame->nb_samples*bytes_per_sample;
		p_cur_ptr = pcm_data = (char*)calloc(1, frame_size);
		for (int i = 0; i < frame->nb_samples; ++i)
		{
			memcpy(p_cur_ptr, frame->data[0] + i*bytes_per_sample, bytes_per_sample);
			p_cur_ptr += bytes_per_sample;
			memcpy(p_cur_ptr, frame->data[1] + i*bytes_per_sample, bytes_per_sample);
			p_cur_ptr += bytes_per_sample;
		}
	}
	return pcm_data;
}

static inline void
debug_write_pcm(AVFrame *frame)
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
	printf("format=%d, channel=%d,nb_samples=%d,bytes_per_sample=%d,linesize[0]=%d,inesize[1]=%d\n",
		frame->format, frame->channels, frame->nb_samples, bytes_per_sample, frame->linesize[0], frame->linesize[1]);
#endif

#if 1
	int32_t frame_size = frame->channels*frame->nb_samples*bytes_per_sample;
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
	}else{//2.For plane sample foramt, we must store pcm datas interleaved. [LRLRLR...LR].
		for (int i = 0; i < frame->nb_samples; ++i)
		{
			fwrite(frame->data[0] + i*bytes_per_sample, 1, bytes_per_sample, fp);
			fwrite(frame->data[1] + i*bytes_per_sample, 1, bytes_per_sample, fp);
		}
	}
#endif
	fflush(fp);
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

	int32_t y_size = frame->width*frame->height;
	char* yuv420_data = (char*)calloc(1, y_size * 3 / 2);
	char* p_cur_ptr = yuv420_data;

	if (NULL == yuv420_data || y_size <= 0)
		return NULL;

	// dump yuv420
	for (int32_t i = 0; i < frame->height; ++i)
		memcpy(p_cur_ptr + frame->width*i, frame->data[0] + frame->linesize[0] * i, frame->width);
	p_cur_ptr += y_size;
	for (int32_t i = 0; i < frame->height / 2; ++i)
		memcpy(p_cur_ptr + i*frame->width / 2, frame->data[1] + frame->linesize[1] * i, frame->width / 2);
	p_cur_ptr += y_size / 4;
	for (int32_t i = 0; i < frame->height / 2; ++i)
		memcpy(p_cur_ptr + i*frame->width / 2, frame->data[2] + frame->linesize[2] * i, frame->width / 2);

	return yuv420_data;
}

static inline void
debug_write_yuv420p(AVFrame *frame)
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

#if 1//剥离AVFrame中的裸数据。
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

/***************************3.兼容旧的调试机制***************************/

#ifndef msg
#define msg( format, ... )\
	fprintf( stderr,"[<%s>:%d] " format , __FUNCTION__,__LINE__, ##__VA_ARGS__);
#endif