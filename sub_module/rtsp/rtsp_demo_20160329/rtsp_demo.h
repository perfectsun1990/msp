/*************************************************************************
	> File Name: rtsp_demo.h
	> Author: bxq
	> Mail: 544177215@qq.com 
	> Created Time: Monday, November 23, 2015 AM12:22:43 CST
 ************************************************************************/

#ifndef __RTSP_DEMO_H__
#define __RTSP_DEMO_H__
/*
 * a simple RTSP server demo
 * RTP over UDP/TCP H264/G711a 
 * */

#include <stdint.h>

typedef void * rtsp_demo_handle;
typedef void * rtsp_session_handle;

rtsp_demo_handle rtsp_new_demo (int port);
rtsp_session_handle rtsp_new_session (rtsp_demo_handle demo, const char *path, int has_video, int has_audio);
int rtsp_do_event (rtsp_demo_handle demo);
int rtsp_tx_video (rtsp_session_handle session, const uint8_t *frame, int len, uint64_t ts);
int rtsp_tx_audio (rtsp_session_handle session, const uint8_t *frame, int len, uint64_t ts);
void rtsp_del_session (rtsp_session_handle session);
void rtsp_del_demo (rtsp_demo_handle demo);

#endif
