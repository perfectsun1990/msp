
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

std::shared_ptr<IMrender>
IMrenderFactory::createVideoMrender(const void* handle,
	std::shared_ptr<IMrenderObserver> observer)
{
	return std::make_shared<VideoMrender>(handle, observer);
}

VideoMrender::VideoMrender(const void* handle,
	std::shared_ptr<IMrenderObserver> observer):
	m_observer(observer)
{
	m_status = E_INVALID;
	/***do init thing***/
	m_config = std::make_shared<VrdrConfig>();
	m_config->window_hdwn = const_cast<void*>(handle);
	m_status = E_INITRES;
}

VideoMrender::~VideoMrender()
{
	stopd();
	av_yuv420p_freep(std::get<0>(m_buffer));
}

AVFrame*
rescale(AVFrame*  avframe, int32_t dst_w, int32_t dst_h, int32_t dst_f)
{
	if (nullptr == avframe)
		return nullptr;

	SwsContext *sws_ctx = nullptr;
	AVFrame* dst_frame  = nullptr;
	AVFrame* src_frame  = avframe;

	if (	src_frame->width  != dst_w ||src_frame->height != dst_h
		||	src_frame->format != dst_f)
	{
		//  1.获取转换句柄.
		if (nullptr != sws_ctx) {
			sws_freeContext(sws_ctx);
			sws_ctx = nullptr;
		}
		if (nullptr == (sws_ctx = sws_getCachedContext(sws_ctx,
			src_frame->width, src_frame->height, (AVPixelFormat)src_frame->format,
			dst_w, dst_h, (AVPixelFormat)dst_f,
			SWS_FAST_BILINEAR, NULL, NULL, NULL)))
			return nullptr;
		//  2.申请缓存数据.
		if (nullptr == (dst_frame = av_v_frame_alloc((AVPixelFormat)dst_f, dst_w, dst_h)))
			return nullptr;
// 		if (avpicture_fill((AVPicture *)dst_frame, (uint8_t*)av_malloc(avpicture_get_size(
// 			(AVPixelFormat)dst_frame->format, dst_frame->width, dst_frame->height)),
// 			(AVPixelFormat)dst_frame->format, dst_frame->width, dst_frame->height) < 0)
// 			return nullptr;
		//  3.进行图像转换.
		sws_scale(sws_ctx, (const uint8_t* const*)src_frame->data, src_frame->linesize, 0, dst_frame->height, dst_frame->data, dst_frame->linesize);	
	}else {
		dst_frame = src_frame;
	}
	return dst_frame;

}

void 
VideoMrender::onMFrm(std::shared_ptr<MRframe> av_frm)
{
	if (-1 == fmtconvert(av_frm->pars->codec_type, av_frm->pars->format))
	{// Note: Only rescale for unsupported foramts,convert to I420.
		AVFrame* pfrm = rescale(av_frm->pfrm,
			av_frm->pfrm->width,av_frm->pfrm->height, AV_PIX_FMT_YUV420P);
		if (pfrm == nullptr) {
			err("video: pfrm=%p rescale failed...\n", pfrm);
			return;
		}
		if (pfrm != av_frm->pfrm)
		{// update av_frm parameters...
			av_frame_free(&av_frm->pfrm);
			av_frm->pfrm		 = pfrm;
			av_frm->pars->width  = pfrm->width;
			av_frm->pars->height = pfrm->height;
			av_frm->pars->format = pfrm->format;
		}
	}
	if (CHK_PROPERTY(av_frm->prop, P_SEEK)){
		m_render_Q.clear();
		msg("video: clear render Que...\n");
	}
	if (CHK_PROPERTY(av_frm->prop, P_PAUS)){
		dbg("video: pause render Pkt...\n");
		return;
	}
	if (!m_signal_quit)
		m_render_Q.push(av_frm);
}

void
VideoMrender::config(void* config)
{	
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config)
		*((VrdrConfig*)config) = *m_config;
}

void
VideoMrender::update(void* config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	VrdrConfig* p_cfg = (VrdrConfig*)config;
	if (nullptr != config)
	{
		if (m_config->window_hdwn!=p_cfg->window_hdwn)
			m_signal_rset = true;
		*m_config = *p_cfg;
		updateAttributes();
	}
}

void 
VideoMrender::clearVideoRqueue(bool is_mrender)
{
	if (is_mrender)
	{
		m_render_Q.clear();
	}
}

STATUS
VideoMrender::status(void)
{
	return m_status;
}

int32_t 
VideoMrender::Q_size(void)
{
	return m_render_Q.size();
}

int32_t 
VideoMrender::cached(void)
{
	return 0;
}

void
VideoMrender::start()
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);

	if (m_signal_quit)
	{
		m_signal_quit = false;
		m_worker = std::thread([&]()
		{
			int32_t ret = -1;
			while (!m_signal_quit)
			{
				SDL_PollEvent(&m_vevent);//Avoid window block...

				// 1.receive and read raw pcm datas.
				std::shared_ptr<MRframe> av_frm = nullptr;
				if (!m_render_Q.try_peek(av_frm))
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					continue;
				}

				// 2.check and reinit sdl2  devices.
				if (m_config->vcodec_pars.v_format != av_frm->pars->format
					|| m_config->vcodec_pars.pix_with != av_frm->pars->width
					|| m_config->vcodec_pars.pix_high != av_frm->pars->height)
				{
					m_config->vcodec_pars.v_format = av_frm->pars->format;
					m_config->vcodec_pars.pix_with = av_frm->pars->width;
					m_config->vcodec_pars.pix_high = av_frm->pars->height;
					m_signal_rset = true;
				}
				if (m_signal_rset)
				{// Warning: Failed will stop current thread.
					if (!resetVideoDevice())	break;
					av_yuv420p_freep(std::get<0>(m_buffer));// cahce av_frm data buffer.
					std::get<0>(m_buffer) = av_yuv420p_clone(av_frm->pfrm);
					if (!std::get<0>(m_buffer)) break;
					std::get<1>(m_buffer) = av_frm->pfrm->width * av_frm->pfrm->height * 3 / 2;
					m_signal_rset = false;
				}

				if (!(av_yuv420p_clone2buffer(av_frm->pfrm, std::get<0>(m_buffer), std::get<1>(m_buffer))))
					continue;

				// 3.get video data for playback.
				if (!m_config->window_paus)
				{// refresh.
					if (0 == (ret = SDL_UpdateTexture(m_textur, nullptr, std::get<0>(m_buffer), m_config->vcodec_pars.pix_with)))
					{
						if (ret = SDL_RenderClear(m_render))
						{
							SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RenderClear: %s!\n", SDL_GetError());
							continue;
						}
#if 0
						int32_t  display_w = av_frm.width, display_h = av_frm.height;
						SDL_GetWindowSize(m_window, &display_w, &display_h);
						m_rectgl.x = 0;
						m_rectgl.y = 0;
						m_rectgl.w = display_w;
						m_rectgl.h = display_h;//修改图像纹理显示大小和区域，和窗体大小无关.
#endif
						if (ret = SDL_RenderCopy(m_render, m_textur, nullptr, nullptr))
						{
							SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_RenderCopy: %s!\n", SDL_GetError());
							continue;
						}
						SDL_RenderPresent(m_render);
					}
					m_curpts = av_frm->upts;
					m_render_Q.popd(av_frm);
				}

				// 4.callback...
				if (!m_observer.expired())
					m_observer.lock()->onMPoint(AVMEDIA_TYPE_VIDEO, m_curpts);
			}
			av_log(nullptr, AV_LOG_WARNING, "Video Mrender finished! m_signal_quit=%d\n", m_signal_quit);
		});
	}
	SET_STATUS(m_status, E_STARTED);
}

void
VideoMrender::stopd(bool stop_quik)
{
	CHK_RETURN(E_STARTED != status());
	SET_STATUS(m_status,E_STOPING);
	if (!m_signal_quit)
	{
		m_signal_quit = true;
		if (m_worker.joinable()) m_worker.join();
		closeVideoDevice(true);
		clearVideoRqueue(true);
	}
	SET_STATUS(m_status, E_STOPPED);
}

void VideoMrender::pause(bool pauseflag)
{
	VrdrConfig cfg;
	config(static_cast<void*>(&cfg));
	cfg.window_paus = pauseflag;
	update(static_cast<void*>(&cfg));
}

void VideoMrender::updateAttributes()
{
	if (nullptr != m_window )
	{
		SDL_SetWindowResizable(m_window, (SDL_bool)(m_config->window_resizabl));
		SDL_SetWindowBordered( m_window, (SDL_bool)(m_config->window_bordered));
		SDL_SetWindowSize(m_window, std::get<0>(m_config->window_size), std::get<1>(m_config->window_size));
		SDL_SetWindowMinimumSize(m_window, std::get<0>(m_config->window_mininum_size), std::get<1>(m_config->window_mininum_size));
		SDL_SetWindowMaximumSize(m_window, std::get<0>(m_config->window_maxinum_size), std::get<1>(m_config->window_maxinum_size));
		SDL_SetWindowPosition(m_window, std::get<0>(m_config->window_position), std::get<1>(m_config->window_position));
		(m_config->window_hide) ? SDL_HideWindow(m_window) : SDL_ShowWindow(m_window);
		if (m_config->window_restored) SDL_RestoreWindow(m_window);
		if (m_config->window_raise_up) SDL_RaiseWindow(m_window);
		if (m_config->window_fuscreen) SDL_SetWindowFullscreen(m_window, SDL_WINDOW_FULLSCREEN_DESKTOP);
	}
}

bool
VideoMrender::opendVideoDevice(bool is_mrender)
{
	static std::mutex device_lock;

	auto once_func = [&](void)->void
	{
		uint32_t subsystem_mask = SDL_INIT_EVERYTHING;
		if (subsystem_mask != SDL_WasInit(subsystem_mask))
		{
			if (SDL_Init(SDL_INIT_EVERYTHING))
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL - %s!\n", SDL_GetError());
				return;
			}
		}
	};
	std::call_once(oc, once_func);

	int32_t best_effort_driver = 0, video_driver_nums = 0;
	int32_t best_effort_device = 0, video_device_nums = 0;
	int32_t best_effort_rdrdrv = 0, video_rdrdrv_nums = 0;
	char* effort_driver_name = nullptr;
	char* effort_device_name = nullptr;
	char* effort_rdrdrv_name = nullptr;

	if (is_mrender)
	{
		// 1.打开视频设备驱动.
		if ((video_driver_nums = SDL_GetNumVideoDrivers()) <= 0) 
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No built-in video drivers\n\n");
			return false;
		}else {
			SDL_Log("Built-in video drivers[%d]:\n", video_driver_nums);
			for (int32_t i = 0; i < video_driver_nums; ++i)
			{
				const char* driver = SDL_GetVideoDriver(i);
				m_config->vcodec_pars.drivers.insert(std::pair<int32_t, std::string>(i, driver));
				SDL_Log("	#%d: %s\n", i, SDL_GetVideoDriver(i));
				if (strstr(driver, "windows"))
					best_effort_driver = i;
			}
		}

		// Note: driver set by user or default.		
		if (0&&!m_config->window_driv.compare(""))
		{
			effort_driver_name = (char*)SDL_GetVideoDriver(best_effort_driver);
			m_config->window_driv = effective(m_config->window_driv.c_str()) ? m_config->window_driv.c_str() : effort_driver_name;
			if (SDL_VideoInit(m_config->window_driv.c_str()))
			{// Warning: Inner will call SDL_VideoQuit() to close pre window. Ensure call once!
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_AudioInit\n\n", SDL_GetError());
				return false;
			}
			SDL_Log("Using video driver: %s \n\n", m_config->window_driv.c_str());
		}	

		// 2.打开指定视频设备.
		if ((video_device_nums = SDL_GetNumVideoDisplays()) <= 0) 
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No display devices found.\n\n");
			return false;
		}else {
			for (int32_t i = 0; i < video_device_nums; i++)
			{//default display:SDL_GetDisplayName(0)
				const char *device = SDL_GetDisplayName(i);
				m_config->vcodec_pars.devices.insert(std::pair<int32_t, std::string>(i, device));
				if (device != nullptr)
					SDL_Log("	#%d: %s\n", i, device);
				else
					SDL_Log("	#%d Error: %s\n", i, SDL_GetError());
			}
		}
		// Note: Displayer set by user or default.
		effort_device_name    = (char*)SDL_GetDisplayName(best_effort_device);
		m_config->window_disp = effective(m_config->window_disp.c_str()) ? m_config->window_disp.c_str() : effort_device_name;
		SDL_Log("Using video device: %s\n\n", m_config->window_disp.c_str());

		
		device_lock.lock();
		// 3.创建视频渲染窗口.
		if (nullptr == (m_sample = SDL_CreateWindow("Sun Player",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			m_config->vcodec_pars.pix_with, m_config->vcodec_pars.pix_high,
			SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE)))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create screen for video!: %s!\n", SDL_GetError());
			return false;
		}
		if (nullptr != m_config->window_hdwn)
		{//Note: Use opengl on windows need to set this.
			char sBuf[32];
			sprintf_s<32>(sBuf, "%p", m_sample);
			SDL_SetHint(SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT, sBuf);
			if (nullptr == (m_window = SDL_CreateWindowFrom(m_config->window_hdwn)))
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create screen for video!: %s!\n", SDL_GetError());
				return false;
			}
			SDL_SetHint(SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT, nullptr);
			SDL_SetHint(SDL_HINT_RENDER_DIRECT3D_THREADSAFE,	"1"); 
			SDL_SetHint(SDL_HINT_RENDER_LOGICAL_SIZE_MODE,		"1");
			SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY,			"2"); 
			//SDL_SetHint(SDL_HINT_RENDER_DRIVER,		   "opengl");
			//SDL_SetHint(SDL_HINT_WINDOWS_ENABLE_MESSAGELOOP,	"0");
		}
		m_window = (nullptr == m_window) ? m_sample : m_window;
		SDL_ShowWindow(m_window);// It's important for reopen window.
		SDL_SetWindowBordered(m_window, SDL_TRUE);//窗体边界
		
		device_lock.unlock();

		// 4.打开视频渲染驱动
		SDL_RendererInfo rdrdrv = { 0 };
		if ((video_rdrdrv_nums = SDL_GetNumRenderDrivers()) <= 0) 
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No built-in render drivers\n\n");
			return false;
		}else {
			SDL_Log("Built-in video rdrdrv[%d]:\n", video_rdrdrv_nums);
			for (int32_t i = 0; i<video_rdrdrv_nums; ++i)
			{//Note:direct3d & opengl is not threadsafe,maybe bugs of SDL2.
				if (!SDL_GetRenderDriverInfo(i, &rdrdrv))
				{
					SDL_Log("	#%d %s\n", i, rdrdrv.name);
					if (!strcmp(rdrdrv.name, "direct3d"))
						best_effort_rdrdrv = i;
				}
			}
		}
		// Note: Render driver set by user or default.
		SDL_GetRenderDriverInfo(best_effort_rdrdrv,&rdrdrv);
		m_config->window_rdrv = effective(m_config->window_rdrv.c_str()) ? m_config->window_rdrv.c_str() : rdrdrv.name;
		if (nullptr == (m_render = SDL_CreateRenderer(m_window, 
			best_effort_rdrdrv, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC)))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create render for video!: %s!\n", SDL_GetError());
			return false;
		}
		SDL_Log("Using video rdrdrv: %s\n\n", m_config->window_rdrv.c_str());

		// 5.创建视频渲染纹理
		int32_t v_format = fmtconvert(AVMEDIA_TYPE_VIDEO, m_config->vcodec_pars.v_format);
		if (nullptr == (m_textur = SDL_CreateTexture(m_render, v_format,// SDL_PIXELFORMAT_IYUV;=YUV420p
			SDL_TEXTUREACCESS_STREAMING, m_config->vcodec_pars.pix_with, m_config->vcodec_pars.pix_high)))
		{
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create texture for video!:%s!\n", SDL_GetError());
			return false;
		}

		SDL_Log("@@@ Open Video device success! display=%s window_id= 0x%x (0-default)\n", 
			m_config->window_disp.c_str(),(long)m_window);
	}

	return true;
}

void
VideoMrender::closeVideoDevice(bool is_mrender)
{
	if (is_mrender)
	{		
		if (nullptr != m_textur) {
			SDL_DestroyTexture(m_textur);
			m_textur = nullptr;
		}
		if (nullptr != m_render) {
			SDL_DestroyRenderer(m_render);
			m_render = nullptr;
		}
		if (nullptr != m_window){
			if (m_window == m_sample)
				m_sample = nullptr;
			SDL_DestroyWindow(m_window);
			m_window = nullptr;
		}
		if (nullptr != m_sample) {
			SDL_DestroyWindow(m_sample);
			m_sample = nullptr;
		}
		m_signal_rset = true;
	}
	atexit(SDL_VideoQuit);//Ensure safe for multiple instances.
}

bool
VideoMrender::resetVideoDevice(bool is_mrender)
{
	if (is_mrender)
	{
		closeVideoDevice(is_mrender);
		return opendVideoDevice(is_mrender);
	}

	return false;
}

std::shared_ptr<IMrender>
IMrenderFactory::createAudioMrender(const char* speakr,
	std::shared_ptr<IMrenderObserver> observer)
{
	return std::make_shared<AudioMrender>(speakr, observer);
}

AudioMrender::AudioMrender(const char* speakr, 
	std::shared_ptr<IMrenderObserver> observer):
	m_observer(observer)
{
	SET_STATUS(m_status, E_INVALID);
	/***do init thing***/
	m_config = std::make_shared<ArdrConfig>();
	m_config->speakr_name = speakr;
	SET_STATUS(m_status, E_INITRES);
}

AudioMrender::~AudioMrender()
{
	stopd();
	av_pcmalaw_freep(std::get<0>(m_buffer));
}

void
AudioMrender::config(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config)
		*((ArdrConfig*)config) = *m_config;
}

void
AudioMrender::update(void* config)
{//Note: configure render.
	std::lock_guard<std::mutex> locker(m_cmutex);
	ArdrConfig* p_cfg = (ArdrConfig*)config;
	if (nullptr != config)
	{
		if (m_config->speakr_name.compare(p_cfg->speakr_name))
			m_signal_rset = true;
		*m_config = *p_cfg;
		updateAttributes();
	}
}

void
AudioMrender::pause(bool pauseflag)
{
	ArdrConfig cfg;
	config(static_cast<void*>(&cfg));
	cfg.speakr_paus = pauseflag;
	update(static_cast<void*>(&cfg));
}


AVFrame*
resmple(AVFrame*  avframe, int32_t dst_s, int32_t dst_n, uint64_t dst_l, int32_t dst_f )
{
	assert(nullptr != avframe);

	int32_t dst_nb_samples  = -1;
	SwrContext *swr_ctx = nullptr;
	AVFrame* dst_frame  = nullptr;
	AVFrame* src_frame  = avframe;
	
	if (src_frame->sample_rate	 != dst_s || src_frame->nb_samples != dst_n || 
		src_frame->channel_layout!= dst_l || src_frame->format	   != dst_f)
	{
		//  1.获取转换句柄.
		if (nullptr != swr_ctx) 
			swr_free(&swr_ctx);
		if (nullptr == (swr_ctx = swr_alloc_set_opts(swr_ctx, dst_l, (AVSampleFormat)dst_f, dst_s,
			src_frame->channel_layout, (AVSampleFormat)src_frame->format, src_frame->sample_rate,
			0, 0)))
			return nullptr;
		if (swr_init(swr_ctx) < 0){
			swr_free(&swr_ctx);
			return nullptr;
		}
		//  2.申请缓存数据.
		if (nullptr == (dst_frame = av_a_frame_alloc((AVSampleFormat)dst_f, dst_l, dst_s, dst_n))){
			swr_free(&swr_ctx);
			return nullptr;
		}
		//  3.进行音频转换.
		if((dst_nb_samples = swr_convert(swr_ctx, dst_frame->data, dst_frame->nb_samples,
			(const uint8_t**)src_frame->data, src_frame->nb_samples)) <= 0)
		{
			swr_free(&swr_ctx);
			return nullptr;
		}
		if (dst_nb_samples  != dst_frame->nb_samples)
			war("expect nb_samples=%d, dst_nb_samples=%d\n", dst_frame->nb_samples, dst_nb_samples);
	}else {
		dst_frame = src_frame;
	}
	return dst_frame;
}

void
AudioMrender::onMFrm(std::shared_ptr<MRframe> av_frm)
{
	if (-1 == fmtconvert(av_frm->pars->codec_type, av_frm->pars->format))
	{// Note: Only resmple for unsupported foramts,convert to s16.
		AVFrame* pfrm = resmple(av_frm->pfrm, av_frm->pfrm->sample_rate,
			av_frm->pfrm->nb_samples, av_frm->pfrm->channel_layout, AV_SAMPLE_FMT_FLTP);
		if (pfrm == nullptr) {
			err("video: pfrm=%p rescale failed...\n", pfrm);
			return;
		}
		if (pfrm != av_frm->pfrm)
		{// update av_frm parameters...
			av_frame_free(&av_frm->pfrm);
			av_frm->pfrm				 = pfrm;
			av_frm->pars->sample_rate    = pfrm->sample_rate;
			av_frm->pars->channels		 = pfrm->channels;
			av_frm->pars->channel_layout = pfrm->channel_layout;
			av_frm->pars->format		 = pfrm->format;
		}
	}
	if (CHK_PROPERTY(av_frm->prop, P_SEEK)) {
		m_render_Q.clear();
		msg("audio: clear render Que...\n");
	}
	if (CHK_PROPERTY(av_frm->prop, P_PAUS)) {
		dbg("audio: pause render Pkt...\n");
		return;
	}
	//msg("cache=%d\n",cached());
	if (!m_signal_quit)
		m_render_Q.push(av_frm);
}

int32_t 
AudioMrender::Q_size(void)
{
	return m_render_Q.size();
}

int32_t 
AudioMrender::cached(void)
{
	int32_t queue_size = (int32_t)SDL_GetQueuedAudioSize(m_audio_devID);
	SDL_Log("SDL_GetQueuedAudioSize=%d\n", queue_size);
	return queue_size;
}

void 
AudioMrender::updateAttributes(void)
{
	if (m_audio_devID >= 2)
	{
		SDL_PauseAudioDevice(m_audio_devID, m_config->speakr_paus);
	}
}

void AudioMrender::clearAudioRqueue(bool is_mrender)
{
	if (is_mrender)
	{
		m_render_Q.clear();
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
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);
	
	if (m_signal_quit)
	{
		m_signal_quit = false;
		m_worker = std::thread([&]()
		{
			int32_t ret = -1;
			while (!m_signal_quit)
			{
				// 1.receive and read raw pcm datas.
				std::shared_ptr<MRframe> av_frm = nullptr;
				if (!m_render_Q.try_peek(av_frm))
				{
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					continue;
				}
				m_render_Q.popd(av_frm);
				// 2.check and reset SDL2 devices.
				if (m_config->acodec_pars.a_format != av_frm->pfrm->format
					|| m_config->acodec_pars.smp_nums != av_frm->pfrm->nb_samples
					|| m_config->acodec_pars.channels != av_frm->pfrm->channels
					|| m_config->acodec_pars.smp_rate != av_frm->pfrm->sample_rate)
				{//update codec pars.
					m_config->acodec_pars.a_format = av_frm->pfrm->format;
					m_config->acodec_pars.smp_nums = av_frm->pfrm->nb_samples;
					m_config->acodec_pars.channels = av_frm->pfrm->channels;
					m_config->acodec_pars.smp_rate = av_frm->pfrm->sample_rate;
					m_signal_rset = true;
				}
				if (m_signal_rset)
				{
					if (!resetAudioDevice())			  break;
					av_pcmalaw_freep(std::get<0>(m_buffer));// cahce av_frm data buffer.
					std::get<0>(m_buffer) = av_pcmalaw_clone(av_frm->pfrm);
					if (nullptr == std::get<0>(m_buffer)) break;
					std::get<1>(m_buffer) = av_samples_get_buffer_size(nullptr, av_frm->pfrm->channels, av_frm->pfrm->nb_samples, (enum AVSampleFormat)av_frm->pfrm->format, 1);
					m_signal_rset = false;
				}

				if (!(av_pcmalaw_clone2buffer(av_frm->pfrm, std::get<0>(m_buffer), std::get<1>(m_buffer))))
					continue;

				// 3.get audio data for playback.
				if (!m_config->speakr_paus)
				{
					if ((ret = SDL_QueueAudio(m_audio_devID, std::get<0>(m_buffer), std::get<1>(m_buffer))))
						continue;
					m_curpts = av_frm->upts;
					//m_render_Q.popd(av_frm);
				}

				// 4.callback...
				if (!m_observer.expired())
					m_observer.lock()->onMPoint(AVMEDIA_TYPE_AUDIO, m_curpts);

			}
			av_log(nullptr, AV_LOG_WARNING, "Audio Mrender finished! m_signal_quit=%d\n", m_signal_quit);
		});
	}
	SET_STATUS(m_status, E_STARTED);
}

void
AudioMrender::stopd(bool stop_quik)
{
	CHK_RETURN(E_STARTED != status());
	SET_STATUS(m_status,E_STOPING);
	if (!m_signal_quit)
	{
		m_signal_quit = true;
		if (m_worker.joinable()) m_worker.join();
		closeAudioDevice(true);
		clearAudioRqueue(true);
	}
	SET_STATUS(m_status, E_STOPPED);
}

static std::mutex speakr_lock;

bool 
AudioMrender::opendAudioDevice(bool is_mrender )
{
	auto once_func = [&](void)->void
	{
		uint32_t subsystem_mask = SDL_INIT_EVERYTHING;
		if (subsystem_mask != SDL_WasInit(subsystem_mask))
		{
			if (SDL_Init(SDL_INIT_EVERYTHING))
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL - %s!\n", SDL_GetError());
				return;
			}
			atexit(SDL_Quit);
		}
	};
	std::call_once(oc, once_func);

	int32_t best_effort_driver = 0, audio_driver_nums = 0;
	int32_t best_effort_device = 0, audio_device_nums = 0;
	char* effort_driver_name = nullptr;
	char* effort_device_name = nullptr;

	if (is_mrender)
	{
		std::lock_guard<std::mutex> locker(speakr_lock);

		// 1.打开音频设备驱动.
		if ((audio_driver_nums = SDL_GetNumAudioDrivers()) <= 0) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No built-in audio drivers\n\n");
			return false;
		}else {
			SDL_Log("Built-in audio drivers[%d]:\n", audio_driver_nums);
			for (int32_t i = 0; i < audio_driver_nums; ++i)
			{//default driver:SDL_GetAudioDriver(0)
				const char* driver = SDL_GetAudioDriver(i);
				m_config->acodec_pars.drivers.insert(std::pair<int32_t, std::string>(i, driver));
				SDL_Log("	#%d: %s\n", i, driver);
				if (strstr(driver, "directsound"))
					best_effort_driver = i;
			}
			effort_driver_name = (char*)SDL_GetAudioDriver(best_effort_driver);
		}

		// Note: driver set by user or default.		
		if (m_config->speakr_driv.compare(""))
		{
			//std::lock_guard<std::mutex> locker(speakr_lock);
			m_config->speakr_driv = effective(m_config->speakr_driv.c_str())
				? m_config->speakr_driv.c_str() : effort_driver_name;
			if (SDL_AudioInit(m_config->speakr_driv.c_str()))
			{// Warning: Inner call SDL_AudioQuit() will cause voice noise. Ensure call once!
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "SDL_AudioInit\n\n", SDL_GetError());
				return false;
			}
			SDL_Log("Using audio driver: %s \n\n", m_config->speakr_driv.c_str());
		}	

		// 2.打开具体音频设备. [0-渲染,1-采集]
		if ((audio_device_nums = SDL_GetNumAudioDevices(!is_mrender)) <= 0) {
			SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "No speaker devices found.\n\n");
			return false;
		}
		for (int32_t i = 0; i < audio_device_nums; i++)
		{
			const char *deivce = SDL_GetAudioDeviceName(i, !is_mrender);			
// 			(deivce != nullptr) ? SDL_Log("	#%d: %s\n", i, deivce)
// 				: SDL_Log("	#%d Error: %s\n", i, SDL_GetError());
			if (0 == i) {//default speaker:SDL_GetAudioDeviceName(0, 0)
				best_effort_device = i;
				effort_device_name = (char*)SDL_GetAudioDeviceName(best_effort_device, !is_mrender);
			}			
			m_config->acodec_pars.devices.insert(std::pair<int32_t, std::string>(i, deivce));
		}

		// Note: Speaker set by user or default.
		m_config->speakr_name = effective(m_config->speakr_name.c_str()) 
			? m_config->speakr_name.c_str() : effort_device_name;
		m_desire_spec.freq		= m_config->acodec_pars.smp_rate;
		m_desire_spec.format	= fmtconvert(AVMEDIA_TYPE_AUDIO, m_config->acodec_pars.a_format);
		m_desire_spec.channels	= m_config->acodec_pars.channels;
		m_desire_spec.silence	= 0;
		m_desire_spec.samples	= m_config->acodec_pars.smp_nums;//每次回调取数据时必须保证SDL能够取到2^n次方个样本，否则会有杂音.
		m_desire_spec.callback	= nullptr;
		m_desire_spec.userdata	 = nullptr;
		//if ()
		{
			//std::lock_guard<std::mutex> locker(speakr_lock);
			m_audio_devID = SDL_OpenAudioDevice(m_config->speakr_name.c_str(), 0,
				&m_desire_spec, &m_device_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
			if (m_audio_devID < 2)
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Open device failed: %s!\n", SDL_GetError());
				return false;
			}
		}
		SDL_Log("Using audio device: %s\n\n", m_config->speakr_name.c_str());

		// 3.启动音频渲染设备.
		SDL_PauseAudioDevice(m_audio_devID, 0);

		SDL_Log("@@@ Open Audio device success! speaker[id=%d]=%s status=%d (1-playing)\n", 
			m_audio_devID, m_config->speakr_name.c_str(), SDL_GetAudioDeviceStatus(m_audio_devID));
	}

	return true;
}

void 
AudioMrender::closeAudioDevice(bool is_mrender)
{
	if (is_mrender)
	{
		if (m_audio_devID >= 2)
		{
			speakr_lock.lock();
			SDL_PauseAudioDevice(m_audio_devID, 1);
			//while (SDL_GetQueuedAudioSize(m_audio_devID) > 0)
			{
				SDL_ClearQueuedAudio(m_audio_devID);
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				msg("wating....\n");
			}
			
			SDL_CloseAudioDevice(m_audio_devID); //Ensure safe for multiple instances.
			m_signal_rset = true;
			speakr_lock.unlock();
		}
	}
	atexit(SDL_AudioQuit);//Ensure safe for multiple instances.
}

bool 
AudioMrender::resetAudioDevice(bool is_mrender)
{
	if (is_mrender)
	{
		closeAudioDevice(is_mrender);
		return opendAudioDevice(is_mrender);
	}
	return false;
}
