
#include "pubcore.hpp"
#include "mrender.hpp"

static std::once_flag oc;

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
			printf("err ff_fmt!=%d\n", ff_fmt);
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

#define REFRESH_VIDEO_EVENT     (SDL_USEREVENT + 1)
#define REFRESH_AUDIO_EVENT     (SDL_USEREVENT + 2)

std::shared_ptr<IVideoMrender>
IVideoMrender::create(const void* device,
	std::shared_ptr<IVideoMrenderObserver> observer)
{
	return std::make_shared<VideoMrender>(device, observer);
}

VideoMrender::VideoMrender(const void* device,
	std::shared_ptr<IVideoMrenderObserver> observer):
	m_window(device),
	m_observe(observer)
{
	m_status = E_INVALID;
	/***do init thing***/
	m_status = E_INITRES;
	start();
}

VideoMrender::~VideoMrender()
{
	stopd();
}

// 要求yuv420p- SDL_PIXELFORMAT_IYUV
void
VideoMrender::onVideoRFrame(const char *data, int32_t size,
	int32_t width, int32_t height, int32_t format, double upts)
{
	AVRenderFrm pfrm;
	pfrm.type = AVMEDIA_TYPE_VIDEO;
	pfrm.size = size;
	pfrm.width = width;
	pfrm.height = height;
	pfrm.videoformat = format;
	pfrm.upts = upts;
	if (!(pfrm.data = (char*)calloc(1, size)))
		return;
	memcpy(pfrm.data, data, size);

	if (!m_signal_quit)
	{
		std::lock_guard<std::mutex> vlocker(m_render_Q_mutx);
		m_render_Q.push(pfrm);
		m_render_Q_cond.notify_all();
	}
}

void VideoMrender::onVideoRFrame(std::shared_ptr<MRframe> avfrm)
{
	if (CHK_PROPERTY(avfrm->prop,  P_SEEK))
		clearVideoRqueue(true);

	if (!CHK_PROPERTY(avfrm->prop, P_PAUS)) 
	{
		char* data = av_yuv420p_clone(avfrm->pfrm);
		if (nullptr == data) return;
		int32_t size = avfrm->pfrm->width*avfrm->pfrm->height * 3 / 2;
		onVideoRFrame(data, size, avfrm->pfrm->width, avfrm->pfrm->height, avfrm->pfrm->format,avfrm->upts);
		av_yuv420p_freep(data);
	}else {
		m_render_Q_cond.notify_all();
	}
}

void
VideoMrender::update(const void *device, void* config)
{
	m_window = (nullptr != device) ? device : nullptr;
	m_config = config;//TODO:configure render status.
#if 0
	SDL_SetWindowPosition(m_screen, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);//居中.
	SDL_SetWindowSize(m_screen, &display_w, &display_h);	//设置用来渲染图像的窗体大小（注意：并不是图像大小）。				
	SDL_SetWindowMinimumSize(m_screen, 352, 288);
	SDL_RestoreWindow(m_screen);//恢复窗口默认大小。
	SDL_HideWindow(m_screen);//隐藏.
#endif
}

void
VideoMrender::SET_STATUS(STATUS status)
{
	m_status = status;
}

void VideoMrender::clearVideoRqueue(bool is_capture)
{
	std::lock_guard<std::mutex> locker(m_render_Q_mutx);
	while (!m_render_Q.empty())
	{
		AVRenderFrm avfrm = m_render_Q.front();
		m_render_Q.pop();
		free(avfrm.data);
	}
}

STATUS
VideoMrender::status(void)
{
	return m_status;
}

int32_t VideoMrender::Q_size(void)
{
	std::lock_guard<std::mutex> locker(m_render_Q_mutx);
	return m_render_Q.size();
}

void
VideoMrender::start()
{
	if (E_STOPPED == status() || E_STOPING == status())
		return;
	SET_STATUS(E_STRTING);

	if (m_signal_quit)
	{
		m_signal_quit = false;
		m_worker = std::thread([&]()
		{
			int32_t ret = 0;
			while (!m_signal_quit)
			{
				m_vevent.type = REFRESH_AUDIO_EVENT;
				SDL_PushEvent(&m_vevent);//解决窗口卡死.
				SDL_WaitEvent(&m_vevent);

				if (m_pauseflag)
				{
					if (!m_observe.expired())
						m_observe.lock()->onVideoTimePoint(m_curpts);
					av_usleep(10 * 1000);
					continue;
				}
			
				// 1.receive and read raw pcm datas.
				AVRenderFrm avfrm;
				{
					std::unique_lock<std::mutex>lock(m_render_Q_mutx);
					m_render_Q_cond.wait(lock, [&]() {
						return (!m_render_Q.empty() || m_signal_quit);
					});
					if (m_pauseflag || m_signal_quit || m_render_Q.empty())
						continue;
					avfrm = m_render_Q.front();
					m_render_Q.pop();
				}

				// 2.check and reinit sdl2  devices.
				if (   m_codecp.videoformat != avfrm.videoformat
					|| m_codecp.width	!= avfrm.width
					|| m_codecp.height	!= avfrm.height)
				{
					m_codecp.videoformat = avfrm.videoformat;//SDL_PIXELFORMAT_IYUV
					m_codecp.width		 = avfrm.width;
					m_codecp.height		 = avfrm.height;
					if (!resetVideoDevice()) {
						break;
					}
				}

				// 3.get video data for playback.
				if (0 == (ret = SDL_UpdateTexture(m_textur, nullptr, avfrm.data, avfrm.width)))
				{		
					int32_t  display_w = avfrm.width, display_h = avfrm.height;
					SDL_GetWindowSize(m_screen, &display_w, &display_h);
					m_rectgl.x = 0;
					m_rectgl.y = 0;
					m_rectgl.w = display_w;
					m_rectgl.h = display_h;//修改图像纹理显示大小，和窗体大小无关，可通过将纹理显示大小设置为实时的窗体大小做到自适应显示 。
					if (ret = SDL_RenderClear(m_render))
					{
						SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RenderClear: %s!\n", SDL_GetError());
						free(avfrm.data);
						continue;
					}
					if (ret = SDL_RenderCopy(m_render, m_textur, nullptr, &m_rectgl))
					{
						SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RenderCopy: %s!\n", SDL_GetError());
						free(avfrm.data);
						continue;
					}
					SDL_RenderPresent(m_render);
				}
				
				// 4.callback...
				if (!m_observe.expired())
					m_observe.lock()->onVideoTimePoint(m_curpts=avfrm.upts);

				free(avfrm.data);
			}
			SDL_Log("Video render finished! ret=%d\n", ret);
		});
	}
	
	SET_STATUS(E_STARTED);
}

void
VideoMrender::stopd(bool stop_quik)
{
	if (E_STOPPED == status() || E_STOPING == status())
		return;
	SET_STATUS(E_STOPING);

	if (!m_signal_quit)
	{
		m_signal_quit = true;
		m_render_Q_cond.notify_all();
		if (m_worker.joinable()) m_worker.join();
		SDL_DestroyTexture(m_textur);
		SDL_DestroyRenderer(m_render);
		SDL_DestroyWindow(m_screen);
	}

	SET_STATUS(E_STOPPED);
}

void VideoMrender::pause(bool pauseflag)
{
	m_pauseflag = pauseflag;
}


bool
VideoMrender::opendVideoDevice(bool is_capture)
{
	if (!is_capture)
	{
		// 创建显示窗
		if (nullptr == (m_screen = (nullptr == m_window) ? SDL_CreateWindow("Sun Player",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			m_codecp.width, m_codecp.height,
			SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE) : SDL_CreateWindowFrom(m_window)))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create screen for video!: %s!\n", SDL_GetError());
			return false;
		}

		//SDL_SetWindowFullscreen(m_screen, SDL_WINDOW_FULLSCREEN);
		//创建渲染器
		if (nullptr == (m_render = SDL_CreateRenderer(m_screen, -1, 0)))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create render for video!: %s!\n", SDL_GetError());
			return false;
		}

		//创建纹理框
		int32_t videoformat = fmtconvert(AVMEDIA_TYPE_VIDEO, m_codecp.audioformat);
		if (nullptr == (m_textur = SDL_CreateTexture(m_render, videoformat,// SDL_PIXELFORMAT_IYUV;=YUV420p
			SDL_TEXTUREACCESS_STREAMING, m_codecp.width, m_codecp.height)))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture for video!:%s!\n", SDL_GetError());
			return false;
		}

		SDL_Log("@@@ Open Video device success! m_window= %ld (0-default)\n", (long)m_window);
	}

	return true;
}

void
VideoMrender::closeVideoDevice(bool is_capture)
{
	if (!is_capture)
	{
		SDL_DestroyTexture(m_textur);
		SDL_DestroyRenderer(m_render);
		SDL_DestroyWindow(m_screen);
	}
	return;
}

bool
VideoMrender::resetVideoDevice(bool is_capture)
{
	std::call_once(oc, [&]()
	{
		SDL_Quit();
		if (SDL_Init(SDL_INIT_EVERYTHING))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL - %s!\n", SDL_GetError());
			return;
		}
	});

	if (!is_capture)
	{
		closeVideoDevice(is_capture);
		return opendVideoDevice(is_capture);
	}

	return false;
}


std::shared_ptr<IAudioMrender>
IAudioMrender::create(const char* device, std::shared_ptr<IAudioMrenderObserver> observer)
{
	return std::make_shared<AudioMrender>(device, observer);
}

AudioMrender::AudioMrender(const char* device,
	std::shared_ptr<IAudioMrenderObserver> observer):
	m_speakr(device),
	m_observe(observer)
{
	SET_STATUS(E_INVALID);
	/***do init thing***/
	SET_STATUS(E_INITRES);
	start();
}

AudioMrender::~AudioMrender()
{
	stopd();
}

void 
AudioMrender::onAudioRFrame(const char* data, int32_t size,
	int32_t sample_rate, int32_t nb_channels, int32_t nb_samples, int32_t format, double upts)
{
	AVRenderFrm pfrm;
	pfrm.type = AVMEDIA_TYPE_AUDIO;
	pfrm.size = size;
	pfrm.sample_rate = sample_rate;
	pfrm.nb_channels = nb_channels;
	pfrm.nb_asamples = nb_samples;
	pfrm.audioformat = format;
	pfrm.upts = upts;
	if (!(pfrm.data = (char*)calloc(1, size)))
		return;
	memcpy(pfrm.data, data, size);

	if (!m_signal_quit)
	{
		std::lock_guard<std::mutex> locker(m_render_Q_mutx);
		m_render_Q.push(pfrm);
		m_render_Q_cond.notify_all();
	}
}

void AudioMrender::onAudioRFrame(std::shared_ptr<MRframe> avfrm)
{
	if (CHK_PROPERTY(avfrm->prop,  P_SEEK))
		clearAudioRqueue(true);

	if (!CHK_PROPERTY(avfrm->prop, P_PAUS)) 
	{
		char* data = av_pcm_clone(avfrm->pfrm);
		if (nullptr == data) return;
		int32_t size = av_samples_get_buffer_size(nullptr, 
			avfrm->pfrm->channels, avfrm->pfrm->nb_samples, (enum AVSampleFormat)avfrm->pfrm->format, 1);
		onAudioRFrame(data, size, avfrm->pfrm->sample_rate, avfrm->pfrm->channels, avfrm->pfrm->nb_samples, avfrm->pfrm->format, avfrm->upts);
		av_pcm_freep(data);
	}else{
		m_render_Q_cond.notify_all();
	}
}

void
AudioMrender::update(const char *device, void* config)
{
	m_speakr = effective(device) ? device : "";
	m_config = config;
}

int32_t AudioMrender::Q_size(void)
{
	std::lock_guard<std::mutex> locker(m_render_Q_mutx);
	return m_render_Q.size();
}

int32_t AudioMrender::cached(void)
{
	int32_t queue_size = (int32_t)SDL_GetQueuedAudioSize(m_audio_devID);
	//SDL_Log("SDL_GetQueuedAudioSize=%d\n", queue_size);
	return queue_size;
}

void
AudioMrender::SET_STATUS(STATUS status)
{
	m_status = status;
}

void AudioMrender::clearAudioRqueue(bool is_capture)
{
	std::lock_guard<std::mutex> locker(m_render_Q_mutx);
	while (!m_render_Q.empty())
	{
		AVRenderFrm avfrm = m_render_Q.front();
		m_render_Q.pop();
		free(avfrm.data);
	}
}

STATUS
AudioMrender::status(void)
{
	return m_status;
}


void 
AudioMrender::start()
{
	if (E_STARTED == status() || E_STRTING == status())
		return;
	SET_STATUS(E_STRTING);

	m_signal_quit = false;
	m_worker = std::thread([&]()
	{
		int32_t ret = 0;

		while (!m_signal_quit)
		{
// 			m_aevent.type = REFRESH_VIDEO_EVENT;
// 			SDL_PushEvent(&m_aevent);//解决窗口卡死.
// 			SDL_WaitEvent(&m_aevent);

			if (m_pauseflag)
			{
				if (!m_observe.expired())
					m_observe.lock()->onAudioTimePoint(m_curpts);
				av_usleep(10 * 1000);
				continue;
			}

			// 1.receive and read raw pcm datas.
			AVRenderFrm avfrm;
			{
				std::unique_lock<std::mutex>lock(m_render_Q_mutx);
				m_render_Q_cond.wait(lock, [&]() {
					return (!m_render_Q.empty() || m_signal_quit);
				});
				if (m_pauseflag || m_signal_quit || m_render_Q.empty())
					continue;
				avfrm = m_render_Q.front();
				m_render_Q.pop();
			}

			// 2.check and reset SDL2 devices.
			if (   m_codecp.audioformat != avfrm.audioformat
				|| m_codecp.nb_asamples != avfrm.nb_asamples
				|| m_codecp.nb_channels != avfrm.nb_channels
				|| m_codecp.sample_rate != avfrm.sample_rate)
			{
				m_codecp.audioformat = avfrm.audioformat;//AUDIO_F32
				m_codecp.nb_asamples = avfrm.nb_asamples;
				m_codecp.nb_channels = avfrm.nb_channels;
				m_codecp.sample_rate = avfrm.sample_rate;
				if (!resetAudioDevice()) {
					free(avfrm.data);
					break;
				}
			}

			// 3.get audio data for playback.			
			if (SDL_AUDIO_PLAYING == SDL_GetAudioDeviceStatus(m_audio_devID))
			{
				//SDL_Log("SDL_GetQueuedAudioSize=%d\n", SDL_GetQueuedAudioSize(m_audio_devID));
				if (SDL_QueueAudio(m_audio_devID, avfrm.data, avfrm.size))
				{
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_QueueAudio: %s!\n", SDL_GetError());
					free(avfrm.data);
					continue;
				}
				if (avfrm.size &(avfrm.size - 1))
					SDL_LogWarn(SDL_LOG_CATEGORY_AUDIO, "audio maybe not smooth! pcm_data_size=%d!\n", avfrm.size);
			}
			// 4.callback...
			if (!m_observe.expired())
				m_observe.lock()->onAudioTimePoint(m_curpts = avfrm.upts);

			free(avfrm.data);
		}

		SDL_Log("Audio render finished! ret=%d\n", ret);
	});

	SET_STATUS(E_STARTED);
}

void
AudioMrender::stopd(bool stop_quik)
{
	if (E_STOPPED == status() || E_STOPING == status())
		return;
	SET_STATUS(E_STOPING);

	if (!m_signal_quit)
	m_signal_quit = true;
	m_render_Q_cond.notify_all();
	if (m_worker.joinable()) m_worker.join();
	closeAudioDevice();
	
	SET_STATUS(E_STOPPED);
}

void AudioMrender::pause(bool pauseflag)
{
	m_pauseflag = pauseflag;
}


void print_devices(int iscapture)
{
	const char *typestr = ((iscapture) ? "capture" : "output");
	int n = SDL_GetNumAudioDevices(iscapture);

	printf("Found %d %s device%s:\n", n, typestr, n != 1 ? "s" : "");

	if (n == -1)
		printf("  Driver can't detect specific %s devices.\n\n", typestr);
	else if (n == 0)
		printf("  No %s devices found.\n\n", typestr);
	else {
		int i;
		for (i = 0; i < n; i++) {
			const char *name = SDL_GetAudioDeviceName(i, iscapture);
			if (name != NULL)
				printf("  %d: %s\n", i, name);
			else
				printf("  %d Error: %s\n", i, SDL_GetError());
		}
		printf("\n");
	}
}

bool 
AudioMrender::opendAudioDevice(bool is_capture )
{
	if (!is_capture)
	{
		// 1.获取筛算音频设备驱动.
		int32_t best_effort_driver = 0, best_effort_device = 0;
		int32_t audio_driver_nums = SDL_GetNumAudioDrivers();
		int32_t audio_device_nums = 0;// SDL_GetNumAudioDevices(is_capture);
		if (audio_driver_nums <= 0) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No built-in audio drivers\n\n");
			return false;
		}else {
			SDL_Log("Built-in audio drivers:\n");
			for (int32_t i = 0; i < audio_driver_nums; ++i)
			{
				const char* driver = SDL_GetAudioDriver(i);
				SDL_Log("	#%d: %s\n", i, driver);
				if (strstr(driver, "winmm"))
					best_effort_driver = i;
				if (strstr(driver, "directsound")) {
					best_effort_driver = i;
					break;
				}
			}
		}
	
		// 2.打开音频设备驱动.
		if (SDL_AudioInit(SDL_GetAudioDriver(best_effort_driver)))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_AudioInit\n\n", SDL_GetError());
			return false;
		}

		SDL_Log("Using audio driver[%d]: %s \n\n", best_effort_driver, SDL_GetCurrentAudioDriver());
		
		// 3.音频硬件设备信息.
		// 参数:0-输出设备,1-采集设备
		audio_device_nums = SDL_GetNumAudioDevices(is_capture);
		if (audio_device_nums <= 0) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No speaker devices found.\n\n");
			return false;
		}
		for (int32_t i = 0; i < audio_device_nums; i++)
		{//default speaker:SDL_GetAudioDeviceName(0,0)
			const char *name = SDL_GetAudioDeviceName(i, is_capture);
			if (name != nullptr)
				SDL_Log("	#%d: %s\n", i, name);
			else
				SDL_Log("	#%d Error: %s\n",  i, SDL_GetError());
		}

		// Note: Speaker set by user or default.
		m_speakr = effective(m_speakr.c_str()) ? m_speakr.c_str() : SDL_GetAudioDeviceName(0, is_capture);
		
		SDL_Log("Using audio device: %s\n\n", m_speakr.c_str());

		// 4.打开音频输出设备.
		m_desire_spec.freq = m_codecp.sample_rate;
		m_desire_spec.format = fmtconvert(AVMEDIA_TYPE_AUDIO, m_codecp.audioformat);
		m_desire_spec.channels = m_codecp.nb_channels;
		m_desire_spec.silence = 0;
		m_desire_spec.samples = m_codecp.nb_asamples;//每次回调取数据时必须保证SDL能够取到2^n次方个样本，否则会有杂音.
		m_desire_spec.callback = nullptr;
		m_desire_spec.userdata = nullptr;
		m_audio_devID = SDL_OpenAudioDevice(m_speakr.c_str(),0,
			&m_desire_spec, &m_device_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
		if (m_audio_devID < 2)
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't open an audio device for playback: %s!\n", SDL_GetError());
			return false;
		}

		// 5.启动音频输出设备.
		SDL_PauseAudioDevice(m_audio_devID, 0);
		SDL_Log("@@@ Open Audio device success! speaker=%s status=%d (1-playing)\n", 
			m_speakr.c_str(), SDL_GetAudioDeviceStatus(m_audio_devID));
	}

	return true;
}

void 
AudioMrender::closeAudioDevice(bool is_capture)
{
	if (m_audio_devID >= 2)
	{
		while (SDL_GetQueuedAudioSize(m_audio_devID) > 0)
		{
			SDL_ClearQueuedAudio(m_audio_devID);
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		}
		SDL_PauseAudioDevice(m_audio_devID, 1);
		SDL_CloseAudioDevice(m_audio_devID);
	}
	SDL_AudioQuit();
}

bool 
AudioMrender::resetAudioDevice(bool is_capture)
{
	std::call_once(oc, [&]()
	{
		SDL_Quit();
		if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL - %s!\n", SDL_GetError());
			return;
		}
	});

	if (!is_capture)
	{
		closeAudioDevice(is_capture);
		return opendAudioDevice(is_capture);
	}

	return false;
}
