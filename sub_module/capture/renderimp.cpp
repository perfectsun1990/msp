
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

#undef main 
#ifdef __cplusplus
}
#endif

#include "renderimp.hpp"

static inline bool
effective(char* dev_name)
{
	return (NULL != dev_name  && '\0' != dev_name[0] && ' ' != dev_name[0]
		&& '\t' != dev_name[0] && '\n' != dev_name[0]);
}

static inline int32_t
fmtconvert(int32_t av_type, int32_t ff_fmt)
{
	int32_t format = -1;
	if (AVMEDIA_TYPE_VIDEO == av_type)
	{
		switch (ff_fmt)
		{
		case AV_PIX_FMT_YUV420P:
			format = SDL_PIXELFORMAT_IYUV;
			break;
		case AV_PIX_FMT_NV12:
			format = SDL_PIXELFORMAT_NV12;
			break;
		case AV_PIX_FMT_NV21:
			format = SDL_PIXELFORMAT_NV21;
			break;
		case AV_PIX_FMT_RGB24:
			format = SDL_PIXELFORMAT_RGB24;
			break;
		case AV_PIX_FMT_BGR24:
			format = SDL_PIXELFORMAT_BGR24;
			break;
		case AV_PIX_FMT_ARGB:
			format = SDL_PIXELFORMAT_BGRA32;
			break;
			break;
		default:
			printf("err fmt!=%d\n", format);
			return -1;
		}
	}

	if (AVMEDIA_TYPE_AUDIO == av_type)
	{
		switch (ff_fmt)
		{
		case AV_SAMPLE_FMT_U8:
		case AV_SAMPLE_FMT_U8P:
			format = AUDIO_U8;
			break;
		case AV_SAMPLE_FMT_S16:
		case AV_SAMPLE_FMT_S16P:
			format = AUDIO_S16SYS;
			break;
		case AV_SAMPLE_FMT_S32:
		case AV_SAMPLE_FMT_S32P:
			format = AUDIO_S32SYS;
			break;
		case AV_SAMPLE_FMT_FLT:
		case AV_SAMPLE_FMT_FLTP:
		case AV_SAMPLE_FMT_DBL:
		case AV_SAMPLE_FMT_DBLP:
			format = AUDIO_F32SYS;
			break;
		default:
			printf("err ff_fmt!=%d\n", ff_fmt);
			return -1;
		}
	}
	return format;
}

std::shared_ptr<IVideoRender> 
IVideoRender::create(void* winhdnw)
{
	return std::make_shared<AVrender>(winhdnw, nullptr);
}

std::shared_ptr<IAudioRender> 
IAudioRender::create(char* speaker)
{
	return std::make_shared<AVrender>(nullptr, speaker);
}

#define REFRESH_VIDEO_EVENT     (SDL_USEREVENT + 1)
#define REFRESH_AUDIO_EVENT     (SDL_USEREVENT + 2)

AVrender::AVrender(void* winhdnw, char* speaker)
{
	updateHandle(winhdnw);
	updateSpeakr(speaker);
}

AVrender::~AVrender()
{
	stopdVideoRender();
	stopdAudioRender();
}

void 
AVrender::updateHandle(void *winhdnw)
{
		m_winhdnw = winhdnw;
		stopdVideoRender();	
		startVideoRender();
}

void 
AVrender::updateSpeakr(char *speaker)
{	
		m_speaker = (nullptr== speaker)?"":speaker;
		stopdAudioRender();		
		startAudioRender();
}

void 
AVrender::onAudioFrame(const char* data, int32_t size,
	int32_t sample_rate, int32_t nb_channels, int32_t nb_samples, int32_t format)
{
	AVRenderFrm pfrm;
	pfrm.type = AVMEDIA_TYPE_AUDIO;
	pfrm.data = (char*)calloc(1, size);
	if (nullptr == pfrm.data)
		return;
	pfrm.size = size;
	pfrm.sample_rate = sample_rate;
	pfrm.nb_channels = nb_channels;
	pfrm.nb_asamples = nb_samples;
	pfrm.audioformat = format;
	memcpy(pfrm.data, data, size);

	m_arender_Q_mutx.lock();
	m_arender_Q.push(pfrm);
	m_arender_Q_mutx.unlock();
	m_arender_Q_cond.notify_all();
}

// 要求yuv420p- SDL_PIXELFORMAT_IYUV
void 
AVrender::onVideoFrame(const char *data, int32_t size,
	int32_t width, int32_t height, int32_t format)
{
	AVRenderFrm pfrm;
	pfrm.type = AVMEDIA_TYPE_VIDEO;
	pfrm.data = (char*)calloc(1, size);
	if (nullptr == pfrm.data)
		return;
	pfrm.size = size;
	pfrm.width = width;
	pfrm.height = height;
	pfrm.videoformat = format;
	memcpy(pfrm.data, data, size);
	
	m_vrender_Q_mutx.lock();
	m_vrender_Q.push(pfrm);
	m_vrender_Q_mutx.unlock();
	m_vrender_Q_cond.notify_all();
}

void 
AVrender::startAudioRender()
{
	m_arender_worker = std::thread([&]()
	{
		int32_t ret = 0;

		while (!m_stop_audio)
		{
			std::unique_lock<std::mutex>lock(m_arender_Q_mutx);
			m_arender_Q_cond.wait(lock, [&]() {
				return (!m_arender_Q.empty() || m_stop_audio);
			});
			// 1.receive and read raw pcm datas.
			if (m_arender_Q.empty())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			AVRenderFrm avfrm = m_arender_Q.front();
			m_arender_Q.pop();

			// 2.check and reinit sdl2  devices.
			if (m_codecpar.audioformat != avfrm.audioformat
				|| m_codecpar.nb_asamples != avfrm.nb_asamples
				|| m_codecpar.nb_channels != avfrm.nb_channels
				|| m_codecpar.sample_rate != avfrm.sample_rate)
			{
				m_codecpar.audioformat = avfrm.audioformat;//AUDIO_F32
				m_codecpar.nb_asamples = avfrm.nb_asamples;
				m_codecpar.nb_channels = avfrm.nb_channels;
				m_codecpar.sample_rate = avfrm.sample_rate;
				if (ret = reOpenAVRenderDevice(AVMEDIA_TYPE_AUDIO)) {
					free(avfrm.data);
					continue;
				}
			}

			// 3.get audio data for playback.
			SDL_PauseAudioDevice(m_audio_devID, 0);	//SDL_Log("SDL_GetAudioStatus()=%d\n", SDL_GetAudioDeviceStatus(m_audio_devID));			
			if (SDL_AUDIO_PLAYING == SDL_GetAudioDeviceStatus(m_audio_devID))
			{
				if (SDL_QueueAudio(m_audio_devID, avfrm.data, avfrm.size))
				{
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_QueueAudio: %s!\n", SDL_GetError());
					free(avfrm.data);
					continue;
				}
				if (avfrm.size &(avfrm.size - 1))
					SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "audio maybe not smooth! pcm_data_size=%d!\n", avfrm.size);
			}
			free(avfrm.data);
		}
		SDL_Log("Audio render finished! ret=%d\n", ret);
	});
}


void 
AVrender::startVideoRender()
{
	m_vrender_worker = std::thread([&]()
	{
		int32_t ret = 0;

		while (!m_stop_video)
		{
			m_av_event.type = REFRESH_VIDEO_EVENT;
			SDL_PushEvent(&m_av_event);//解决窗口卡死.
			std::unique_lock<std::mutex>lock(m_vrender_Q_mutx);
			m_vrender_Q_cond.wait(lock, [&]() {
				return (!m_vrender_Q.empty() || m_stop_video);
			});
			SDL_WaitEvent(&m_av_event);
			if (m_av_event.type != REFRESH_VIDEO_EVENT || m_vrender_Q.empty())
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}

			// 1.receive and read raw pcm datas.
			AVRenderFrm avfrm = m_vrender_Q.front();
			m_vrender_Q.pop();

			// 2.check and reinit sdl2  devices.
			if (m_codecpar.videoformat != avfrm.videoformat
				|| m_codecpar.width != avfrm.width
				|| m_codecpar.height != avfrm.height)
			{
				m_codecpar.videoformat	= avfrm.videoformat;//SDL_PIXELFORMAT_IYUV
				m_codecpar.width		= avfrm.width;
				m_codecpar.height		= avfrm.height;
				if (ret = reOpenAVRenderDevice(AVMEDIA_TYPE_VIDEO)) {
					free(avfrm.data);
					continue;
				}
			}

			// 3.get video data for playback.
			if (0 == (ret = SDL_UpdateTexture(m_texture, nullptr, avfrm.data, avfrm.width)))
			{
#if 0
				SDL_SetWindowPosition(m_pscreen, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);//居中.
				SDL_SetWindowSize(m_pscreen, &display_w, &display_h);	//设置用来渲染图像的窗体大小（注意：并不是图像大小）。				
				SDL_SetWindowMinimumSize(m_pscreen, 352, 288);
				SDL_RestoreWindow(m_pscreen);//恢复窗口默认大小。
				SDL_HideWindow(m_pscreen);//隐藏.
#endif			
				int32_t  display_w = avfrm.width, display_h = avfrm.height;
				SDL_GetWindowSize(m_pscreen, &display_w, &display_h);
				m_rect.x = 0;
				m_rect.y = 0;
				m_rect.w = display_w;
				m_rect.h = display_h;//修改图像纹理显示大小，和窗体大小无关，可通过将纹理显示大小设置为实时的窗体大小做到自适应显示 。
				if (ret = SDL_RenderClear(m_prender))
				{
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RenderClear: %s!\n", SDL_GetError());
					free(avfrm.data);
					continue;
				}
				if (ret = SDL_RenderCopy(m_prender, m_texture, nullptr, &m_rect))
				{
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RenderCopy: %s!\n", SDL_GetError());
					free(avfrm.data);
					continue;
				}
				SDL_RenderPresent(m_prender);
			}
			free(avfrm.data);
		}
		SDL_Log("Video render finished! ret=%d\n", ret);
	});
}

void 
AVrender::stopdAudioRender(bool stop_quik)
{
	m_stop_audio = true;	
	m_arender_Q_cond.notify_all();
	if (m_arender_worker.joinable()) m_arender_worker.join();	
	m_stop_audio = false;
	if (effective((char*)m_speaker.c_str()))
	{
		SDL_CloseAudio();
		SDL_CloseAudioDevice(m_audio_devID);
	}
}

void 
AVrender::stopdVideoRender(bool stop_quik)
{
	m_stop_video = true;
	m_vrender_Q_cond.notify_all();
	if (m_vrender_worker.joinable()) m_vrender_worker.join();
	m_stop_video = false;
	if (m_winhdnw != nullptr)
	{
		SDL_DestroyTexture(m_texture);
		SDL_DestroyRenderer(m_prender);
		SDL_DestroyWindow(m_pscreen);	
	}
}

int32_t 
AVrender::reOpenAVRenderDevice(int32_t av_type)
{
	static std::once_flag oc;
	std::call_once(oc, [&]()
	{
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL - %s!\n", SDL_GetError());
			return;
		}
	});

	// 初始化音频输出设备
	if (av_type == AVMEDIA_TYPE_AUDIO)
	{
		SDL_CloseAudio();
		SDL_CloseAudioDevice(m_audio_devID);

		// 默认输出设备.
		const char *devname = SDL_GetAudioDeviceName(0, 0);//default.
		if (nullptr == devname) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_GetAudioDeviceName: %s!\n", SDL_GetError());
			return -1;
		}

		// 打开输出设备。
		m_speaker = effective((char*)m_speaker.c_str()) ? m_speaker: devname;
		m_desire_spec.freq = m_codecpar.sample_rate;
		m_desire_spec.format = fmtconvert(AVMEDIA_TYPE_AUDIO, m_codecpar.audioformat);// AUDIO_F32;
		m_desire_spec.channels = m_codecpar.nb_channels;
		m_desire_spec.silence = 0;
		m_desire_spec.samples = 1024;//每次回调取数据时必须保证sdl能够取到2^n次方个样本，
		m_desire_spec.callback = NULL;// fill_audio;
		m_audio_devID = SDL_OpenAudioDevice(m_speaker.c_str(), 0,
			&m_desire_spec, &m_device_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
		if (m_audio_devID <= 0)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open an audio device for playback: %s!\n", SDL_GetError());
			return -2;
		}

		SDL_Log("@@@ SDL_OpenAudioDevice:%s success!\n", m_speaker.c_str());
	}

	// 初始化视频输出设备
	if (av_type == AVMEDIA_TYPE_VIDEO)
	{
		SDL_DestroyTexture(m_texture);
		SDL_DestroyRenderer(m_prender);
		SDL_DestroyWindow(m_pscreen);
	
		// 创建显示窗
		if (nullptr == (m_pscreen = (nullptr == m_winhdnw) ? SDL_CreateWindow("Sun Player",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			m_codecpar.width, m_codecpar.height,
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE) : SDL_CreateWindowFrom(m_winhdnw)))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create screen for video!: %s!\n", SDL_GetError());
			return -1;
		}
		//SDL_SetWindowFullscreen(m_pscreen, SDL_WINDOW_FULLSCREEN);
		//创建渲染器
		if (nullptr == (m_prender = SDL_CreateRenderer(m_pscreen, -1, 0)))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create render for video!: %s!\n", SDL_GetError());
			return -2;
		}
		//创建纹理框
		int32_t videoformat = fmtconvert(AVMEDIA_TYPE_VIDEO, m_codecpar.audioformat);
		if (nullptr == (m_texture = SDL_CreateTexture(m_prender, videoformat,// SDL_PIXELFORMAT_IYUV;=YUV420p
			SDL_TEXTUREACCESS_STREAMING, m_codecpar.width, m_codecpar.height)))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture for video!:%s!\n", SDL_GetError());
			return -3;
		}

		std::cout << "@@@ SDL_CreateTexture:%s success! m_winhdnw=\n" << m_winhdnw << std::endl;
	}
	return 0;
}
