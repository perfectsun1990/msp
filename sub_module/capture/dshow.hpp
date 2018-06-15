#ifndef getCameraInfo_h_
#define getCameraInfo_h_

#include <vector>


extern  "C"
{

#include "libavformat/avformat.h"
#include "libavutil/pixdesc.h"

#include "SDL.h"
#undef main
};

#ifdef WIN32
#include "DShow.h"
#pragma comment(lib,"strmiids.lib")
#pragma comment(lib,"quartz.lib")
#endif

using namespace std;

#define  MAX_CAMERAS_NUM 16

typedef struct PixelFormatTag 
{
	enum AVPixelFormat pix_fmt;//像素格式.
	unsigned int fourcc;
} PixelFormatTag;


typedef struct
{
	char formatName[32];
	VIDEO_STREAM_CONFIG_CAPS caps;//摄像头参数
	AUDIO_STREAM_CONFIG_CAPS caps_a;
}PixelInfo;

typedef struct
{
	char cameraName[64];
	vector <PixelInfo> formatInfo;
}cameraInfo;


typedef struct
{
	int nCount;
	cameraInfo nCameraInfo[MAX_CAMERAS_NUM];
}CameraInfoForUsb;


CameraInfoForUsb *getCameraInfoForUsb();
void show_CamerInfo();
int getCameraDevNum();
char * getCameraNameByIndex(int nIndex);
#endif