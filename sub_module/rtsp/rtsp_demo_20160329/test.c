/*************************************************************************
	> File Name: test.c
	> Author: bxq
	> Mail: 544177215@qq.com 
	> Created Time: Saturday, December 12, 2015 PM03:19:12 CST
 ************************************************************************/

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <time.h>

#include "comm.h"
#include "rtsp_demo.h"


#include <signal.h>
static int flag_run = 1;
static void sig_proc(int signo)
{
	flag_run = 0;
}

static int get_next_video_frame (FILE *fp, uint8_t **buff, int *size)
{
	uint8_t szbuf[1024];
	int szlen = 0;
	int ret;
	if (!(*buff)) {
		*buff = (uint8_t*)malloc(2*1024*1024);
		if (!(*buff))
			return -1;
	}

	*size = 0;

	while ((ret = fread(szbuf + szlen, 1, sizeof(szbuf) - szlen, fp)) > 0) {
		int i = 3;
		szlen += ret;
		while (i < szlen - 3 && !(szbuf[i] == 0 &&  szbuf[i+1] == 0 && (szbuf[i+2] == 1 || (szbuf[i+2] == 0 && szbuf[i+3] == 1)))) i++;
		memcpy(*buff + *size, szbuf, i);
		*size += i;
		memmove(szbuf, szbuf + i, szlen - i);
		szlen -= i;
		if (szlen > 3) {
			//printf("szlen %d\n", szlen);
			fseek(fp, -szlen, SEEK_CUR);
			break;
		}
	}
	if (ret > 0)
		return *size;
	return 0;
}

static int get_next_audio_frame (FILE *fp, uint8_t **buff, int *size)
{
	int ret;
#define AUDIO_FRAME_SIZE 320
	if (!(*buff)) {
		*buff = (uint8_t*)malloc(AUDIO_FRAME_SIZE);
		if (!(*buff))
			return -1;
	}

	ret = fread(*buff, 1, AUDIO_FRAME_SIZE, fp);
	if (ret > 0) {
		*size = ret;
		return ret;
	}
	return 0;
}

static uint64_t os_gettime (void)
{
#ifdef __WIN32__
	return (timeGetTime() * 1000ll);//us
#endif
#ifdef __LINUX__
	struct timespec tp;
	clock_gettime(CLOCK_MONOTONIC, &tp);
	return (tp.tv_sec * 1000000llu + tp.tv_nsec / 1000llu);
#endif
}


#define CHN0_HAS_VIDEO 1
#define CHN0_HAS_AUDIO 1
// #define CHN1_HAS_VIDEO 1
// #define CHN1_HAS_AUDIO 1

int main(int argc, const char *argv[])
{
	int ret;
	FILE *vfp0, *afp0;	
	FILE *vfp1, *afp1;
	uint8_t *vbuf = NULL;
	uint8_t *abuf = NULL;
	uint64_t ts = 0;
	uint64_t ts_last = 0;
	int vsize = 0, asize = 0;
	rtsp_demo_handle demo;
	rtsp_session_handle chn0;

#if CHN0_HAS_VIDEO
	const char* test_video = "E:\\work\\rtsp_demo_20160329\\BarbieGirl.h264";
	vfp0 = fopen(test_video, "rb");
	if (!vfp0) {
		fprintf(stderr, "open %s failed\n", argv[1]);
		fprintf(stderr, "Usage: %s <H264FILE> <G711AFILE> <H264FILE> <G711AFILE>\n", argv[0]);
		return 0;
	}
#endif

#if CHN0_HAS_AUDIO
	const char* test_audio = "E:\\work\\rtsp_demo_20160329\\BarbieGirl.alaw";
	afp0 = fopen(test_audio, "rb");
	if (!afp0 < 0) {
		fprintf(stderr, "open %s failed\n", argv[2]);
		fprintf(stderr, "Usage: %s <H264FILE> <G711AFILE> <H264FILE> <G711AFILE>\n", argv[0]);
		return 0;
	}
#endif

	demo = rtsp_new_demo(0);
	if (NULL == demo) {
		printf("rtsp_start failed\n");
		return -1;
	}

	chn0 = rtsp_new_session(demo, "/live/chn0", CHN0_HAS_VIDEO, CHN0_HAS_AUDIO);
	if (NULL == chn0) {
		printf("rtsp_new_session failed\n");
		return 0;
	}

	ts_last = os_gettime();
	signal(SIGINT, sig_proc);
	while (flag_run) 
	{
		uint8_t type = 0;

		do {
			ret = rtsp_do_event(demo);
		} while (ret > 0);
		if (ret < 0)
			break;

#if CHN0_HAS_VIDEO
chn0_video_again:
		ret = get_next_video_frame(vfp0, &vbuf, &vsize);
		if (ret < 0) {
			fprintf(stderr, "get_next_video_frame failed\n");
			break;
		}
		if (ret == 0) {
			fseek(vfp0, 0, SEEK_SET);
#if CHN0_HAS_AUDIO
			fseek(afp0, 0, SEEK_SET);
#endif
			goto chn0_video_again;
		}

		rtsp_tx_video(chn0, vbuf, vsize, ts);

		type = 0;
		if (vbuf[0] == 0 && vbuf[1] == 0 && vbuf[2] == 1) {
			type = vbuf[3] & 0x1f;
		}
		if (vbuf[0] == 0 && vbuf[1] == 0 && vbuf[2] == 0 && vbuf[3] == 1) {
			type = vbuf[4] & 0x1f;
		}
		if (type != 5 && type != 1)
			goto chn0_video_again;
#endif
#if CHN0_HAS_AUDIO
		ret = get_next_audio_frame(afp0, &abuf, &asize);
		if (ret < 0) {
			fprintf(stderr, "get_next_audio_frame failed\n");
			break;
		}
		if (ret == 0) {
#if CHN0_HAS_VIDEO
			fseek(vfp0, 0, SEEK_SET);
#endif
			fseek(afp0, 0, SEEK_SET);
			continue;
		}
		rtsp_tx_audio(chn0, abuf, asize, ts);
#endif
#if 0
#define DELAY_TIME (1000000 / 25)
		while (os_gettime() - ts < DELAY_TIME)
			usleep(DELAY_TIME);//每隔40ms发送一帧
		ts += DELAY_TIME;
		printf("ts=%llu (ms)\r\b", ts/1000);
		fflush(stdout);
#else
#define DELAY_TIME (1000000 / 25)
		while (os_gettime() - ts_last < DELAY_TIME)
			usleep(DELAY_TIME);//每隔40ms发送一帧
		ts_last += DELAY_TIME;
		ts += DELAY_TIME;
		printf("ts=%llu (ms)\r\b", ts / 1000);
		fflush(stdout);
#endif
	}

	free(vbuf);
	free(abuf);
#if CHN0_HAS_VIDEO
	fclose(vfp0);
#endif
#if CHN0_HAS_AUDIO
	fclose(afp0);
#endif
	rtsp_del_session(chn0);
	rtsp_del_demo(demo);
	printf("Exit.\n");
	getchar();
	return 0;
}
#if 0
#define CHN0_HAS_VIDEO 1
#define CHN0_HAS_AUDIO 1
#define CHN1_HAS_VIDEO 1
#define CHN1_HAS_AUDIO 1

int main(int argc, char *argv[])
{
	int ret;
	FILE *vfp0, *afp0;	
	FILE *vfp1, *afp1;
	uint8_t *vbuf = NULL;
	uint8_t *abuf = NULL;
	uint64_t ts = 0;
	int vsize = 0, asize = 0;
	rtsp_demo_handle demo;
	rtsp_session_handle chn0;
	rtsp_session_handle chn1;

	if (argc < 5) {
		fprintf(stderr, "Usage: %s <H264FILE> <G711AFILE> <H264FILE> <G711AFILE>\n", argv[0]);
		return 0;
	}

#if CHN0_HAS_VIDEO
	vfp0 = fopen(argv[1], "rb");
	if (!vfp0) {
		fprintf(stderr, "open %s failed\n", argv[1]);
		fprintf(stderr, "Usage: %s <H264FILE> <G711AFILE> <H264FILE> <G711AFILE>\n", argv[0]);
		return 0;
	}
#endif

#if CHN0_HAS_AUDIO
	afp0 = fopen(argv[2], "rb");
	if (!afp0 < 0) {
		fprintf(stderr, "open %s failed\n", argv[2]);
		fprintf(stderr, "Usage: %s <H264FILE> <G711AFILE> <H264FILE> <G711AFILE>\n", argv[0]);
		return 0;
	}
#endif

#if CHN1_HAS_VIDEO
	vfp1 = fopen(argv[3], "rb");
	if (!vfp1) {
		fprintf(stderr, "open %s failed\n", argv[3]);
		fprintf(stderr, "Usage: %s <H264FILE> <G711AFILE> <H264FILE> <G711AFILE>\n", argv[0]);
		return 0;
	}
#endif

#if CHN1_HAS_AUDIO
	afp1 = fopen(argv[4], "rb");
	if (!afp1 < 0) {
		fprintf(stderr, "open %s failed\n", argv[4]);
		fprintf(stderr, "Usage: %s <H264FILE> <G711AFILE> <H264FILE> <G711AFILE>\n", argv[0]);
		return 0;
	}
#endif

	demo = rtsp_new_demo(0);
	if (NULL == demo) {
		printf("rtsp_start failed\n");
		return -1;
	}

	chn0 = rtsp_new_session(demo, "/live/chn0", CHN0_HAS_VIDEO, CHN0_HAS_AUDIO);
	if (NULL == chn0) {
		printf("rtsp_new_session failed\n");
		return 0;
	}

	chn1 = rtsp_new_session(demo, "/live/chn1", CHN1_HAS_VIDEO, CHN1_HAS_AUDIO);
	if (NULL == chn1) {
		printf("rtsp_new_session failed\n");
		return 0;
	}

	ts = os_gettime();
	signal(SIGINT, sig_proc);
	while (flag_run) {
		uint8_t type = 0;

		do {
			ret = rtsp_do_event(demo);
		} while (ret > 0);
		if (ret < 0)
			break;

#if CHN0_HAS_VIDEO
chn0_video_again:
		ret = get_next_video_frame(vfp0, &vbuf, &vsize);
		if (ret < 0) {
			fprintf(stderr, "get_next_video_frame failed\n");
			break;
		}
		if (ret == 0) {
			fseek(vfp0, 0, SEEK_SET);
#if CHN0_HAS_AUDIO
			fseek(afp0, 0, SEEK_SET);
#endif
			goto chn0_video_again;
		}

		rtsp_tx_video(chn0, vbuf, vsize, ts);

		type = 0;
		if (vbuf[0] == 0 && vbuf[1] == 0 && vbuf[2] == 1) {
			type = vbuf[3] & 0x1f;
		}
		if (vbuf[0] == 0 && vbuf[1] == 0 && vbuf[2] == 0 && vbuf[3] == 1) {
			type = vbuf[4] & 0x1f;
		}
		if (type != 5 && type != 1)
			goto chn0_video_again;
#endif
#if CHN0_HAS_AUDIO
		ret = get_next_audio_frame(afp0, &abuf, &asize);
		if (ret < 0) {
			fprintf(stderr, "get_next_audio_frame failed\n");
			break;
		}
		if (ret == 0) {
#if CHN0_HAS_VIDEO
			fseek(vfp0, 0, SEEK_SET);
#endif
			fseek(afp0, 0, SEEK_SET);
			continue;
		}
		rtsp_tx_audio(chn0, abuf, asize, ts);
#endif

#if CHN1_HAS_VIDEO
chn1_video_again:
		ret = get_next_video_frame(vfp1, &vbuf, &vsize);
		if (ret < 0) {
			fprintf(stderr, "get_next_video_frame failed\n");
			break;
		}
		if (ret == 0) {
			fseek(vfp1, 0, SEEK_SET);
#if CHN1_HAS_AUDIO
			fseek(afp1, 0, SEEK_SET);
#endif
			goto chn1_video_again;
		}

		rtsp_tx_video(chn1, vbuf, vsize, ts);

		type = 0;
		if (vbuf[0] == 0 && vbuf[1] == 0 && vbuf[2] == 1) {
			type = vbuf[3] & 0x1f;
		}
		if (vbuf[0] == 0 && vbuf[1] == 0 && vbuf[2] == 0 && vbuf[3] == 1) {
			type = vbuf[4] & 0x1f;
		}
		if (type != 5 && type != 1)
			goto chn1_video_again;
#endif
#if CHN1_HAS_AUDIO
		ret = get_next_audio_frame(afp1, &abuf, &asize);
		if (ret < 0) {
			fprintf(stderr, "get_next_audio_frame failed\n");
			break;
		}
		if (ret == 0) {
#if CHN1_HAS_VIDEO
			fseek(vfp1, 0, SEEK_SET);
#endif
			fseek(afp1, 0, SEEK_SET);
			continue;
		}
		rtsp_tx_audio(chn1, abuf, asize, ts);
#endif

		while (os_gettime() - ts < 1000000 / 25)
			usleep(1000000 / 25);
		ts += 1000000 / 25;
		printf(".");fflush(stdout);
	}

	free(vbuf);
	free(abuf);
#if CHN0_HAS_VIDEO
	fclose(vfp0);
#endif
#if CHN0_HAS_AUDIO
	fclose(afp0);
#endif
#if CHN1_HAS_VIDEO
	fclose(vfp1);
#endif
#if CHN1_HAS_AUDIO
	fclose(afp1);
#endif
	rtsp_del_session(chn1);	
	rtsp_del_session(chn0);
	rtsp_del_demo(demo);
	printf("Exit.\n");
	getchar();
	return 0;
}
#endif

