
#include "pubcore.hpp"
#include "mrender.hpp"

static std::mutex			   window_lock;
static std::mutex			   speakr_lock;

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
			err("invalid video ff_fmt=%d\n", ff_fmt);
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
			err("invalid audio ff_fmt=%d\n", ff_fmt);
			return -1;
		}
	}
	return format;
}

void
OnceTaskInit()
{
	static std::once_flag once;
	std::call_once(once, [&](void)->void
	{
		uint32_t masks = SDL_INIT_EVERYTHING;
		if (masks != SDL_WasInit(masks))
		{// Note: init & quit once duraing a process.
			if (SDL_Init(SDL_INIT_EVERYTHING))
				err("Couldn't initialize SDL subsystem - %s!\n", SDL_GetError());
			if (std::atexit([](void)
			{
				SDL_AudioQuit();
				SDL_VideoQuit();
				SDL_Quit();
			}))
				err("Couldn't regisert cleanup for SDL - %s!\n", SDL_GetError());
		}
	});
}

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

void 
VideoMrender::onMFrm(std::shared_ptr<MRframe> av_frm)
{
	if (AV_PIX_FMT_YUV420P != av_frm->pfrm->format)
	{// Note: Only rescale for unsupported foramts, convert to I420.
		AVFrame* pfrm = video_frame_alloc(AV_PIX_FMT_YUV420P, av_frm->pfrm->width, av_frm->pfrm->height);
		if (!video_rescale(&m_swsctx, pfrm, av_frm->pfrm)) {
			err("video: pfrm=%p rescale failed...\n", pfrm);
			video_frame_freep(&pfrm);
			return;
		}
		// update the av_frm parameters...
		av_frame_free(&av_frm->pfrm);
		av_frm->pfrm = pfrm;
		av_frm->pars->width = pfrm->width;
		av_frm->pars->height = pfrm->height;
		av_frm->pars->format = pfrm->format;
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

	m_signal_quit = false;
	m_worker = std::thread([&]()
	{
		int32_t ret = -1;
		for (int64_t loop = 0; !m_signal_quit; ++loop, m_last_loop = av_gettime())
		{
			SDL_PollEvent(&m_vevent);//Avoid window block...
			// 1.receive and read raw pcm datas.
			std::shared_ptr<MRframe> av_frm = nullptr;
			if (!m_render_Q.try_peek(av_frm)) {
				sleepMs(STANDARDTK);
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
			if (m_signal_rset) {// Warning: Failed will stop current thread.
				if (!resetVideoDevice()) {
					SET_STATUS(m_status, E_STRTERR);
					for (ret = 0; ret < 300 && !m_signal_quit; ++ret)
						sleepMs(STANDARDTK);//3s.
					continue;
				}
				av_yuv420p_freep(std::get<0>(m_buffer));// cahce av_frm data buffer.
				std::get<0>(m_buffer) = av_yuv420p_clone(av_frm->pfrm);
				if (!std::get<0>(m_buffer)) break;
				std::get<1>(m_buffer) = av_frm->pfrm->width * av_frm->pfrm->height * 3 / 2;
				m_signal_rset = false;
				SET_STATUS(m_status, E_STARTED);
			}
			if (!(av_yuv420p_clone2buffer(av_frm->pfrm, std::get<0>(m_buffer), std::get<1>(m_buffer))))
				continue;
			// 3.get video data for playback.
			if (!m_config->window_paus)
			{// refresh.
				if (0 == (ret = SDL_UpdateTexture(m_textur, nullptr, std::get<0>(m_buffer), m_config->vcodec_pars.pix_with)))
				{
					if (ret = SDL_RenderClear(m_render)) {
						SET_STATUS(m_status, E_RUNNERR);
						err( "SDL_RenderClear: %s!\n", SDL_GetError());
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
					if (ret = SDL_RenderCopy(m_render, m_textur, nullptr, nullptr)) {
						SET_STATUS(m_status, E_RUNNERR);
						err( "SDL_RenderCopy: %s!\n", SDL_GetError());
						continue;
					}
					SDL_RenderPresent(m_render);
				}
				m_curpts = av_frm->upts;
				m_render_Q.popd(av_frm);
			}
			// 4.callback...
			if (!m_observer.expired())
				m_observer.lock()->onMPts(AVMEDIA_TYPE_VIDEO, m_curpts);
			SET_STATUS(m_status, ((m_status == E_STOPING) ? E_STOPING : E_RUNNING));
		}
		av_log(nullptr, AV_LOG_WARNING, "Video Mrender finished! m_signal_quit=%d\n", m_signal_quit);
	});
}

void
VideoMrender::stopd(bool stop_quik)
{
	CHK_RETURN(E_STOPPED == status() || E_STOPING == status());
	SET_STATUS(m_status,E_STOPING);
	m_signal_quit = true;
	if (m_worker.joinable()) m_worker.join();
	closeVideoDevice(true);
	clearVideoRqueue(true);
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
	OnceTaskInit();

	int32_t effort_driver_nums = 0, effort_device_nums = 0, effort_rdrdrv_nums = 0;
	char*   effort_driver_name = 0,*effort_device_name = 0,*effort_rdrdrv_name = 0;

	if (is_mrender)
	{
		std::lock_guard<std::mutex> locker(window_lock);

		// 1.打开视频设备驱动.
		if ((effort_driver_nums = SDL_GetNumVideoDrivers()) <= 0) {
			err( "No built-in video drivers\n\n");
			return false;
		}else {
			out("Built-in video drivers[%d]:\n", effort_driver_nums);
			effort_driver_name = (char*)SDL_GetVideoDriver(0);
			for (int32_t i = 0; i < effort_driver_nums; ++i) {
				const char* driver = SDL_GetVideoDriver(i);
				if(nullptr == driver) continue;
				m_config->vcodec_pars.drivers.insert(std::pair<int32_t, std::string>(i, driver));
				if (strstr(driver, "windows"))
					effort_driver_name = (char*)driver;
				out("	#%d: %s\n", i, SDL_GetVideoDriver(i));
			}
			static std::once_flag once;
			std::call_once(once, [&](void)->void
			{// Warning: Inner will close relative window. Ensure call once!
				m_config->window_driv = IsStrEffect(m_config->window_driv.c_str())
					? m_config->window_driv.c_str() : effort_driver_name;
				if (SDL_VideoInit(m_config->window_driv.c_str()))
					err("SDL_VideoInit %s\n\n", SDL_GetError());
				out("--->Using video driver: %s \n\n", m_config->window_driv.c_str());
			});
		}

		// 2.打开指定视频设备.
		if ((effort_device_nums = SDL_GetNumVideoDisplays()) <= 0) {
			err( "No display devices found.\n\n");
			return false;
		}else {
			for (int32_t i = 0; i < effort_device_nums; i++)
			{//default display:SDL_GetDisplayName(0)
				const char *device = SDL_GetDisplayName(i);
				if (nullptr == device) continue;
				m_config->vcodec_pars.devices.insert(std::pair<int32_t, std::string>(i, device));
				if (0 == i)//default window:SDL_GetDisplayName(0)
					effort_device_name = (char*)device;
				out("	#%d: %s\n", i, device);
			}
			m_config->window_disp = IsStrEffect(m_config->window_disp.c_str())
				? m_config->window_disp.c_str() : effort_device_name;
			out("--->Using video device: %s\n\n", m_config->window_disp.c_str());
		}

		// 3.创建视频渲染窗口.
		if (nullptr == (m_sample = SDL_CreateWindow("Sun Player",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
			m_config->vcodec_pars.pix_with, m_config->vcodec_pars.pix_high,
			SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN | SDL_WINDOW_RESIZABLE)))
		{
			err( "Couldn't create screen for video!: %s!\n", SDL_GetError());
			return false;
		}
		if (nullptr != m_config->window_hdwn)
		{//Note: Use opengl on windows need to set this.
			char sBuf[32];
			sprintf_s<32>(sBuf, "%p", m_sample);
			SDL_SetHint(SDL_HINT_VIDEO_WINDOW_SHARE_PIXEL_FORMAT, sBuf);
			if (nullptr == (m_window = SDL_CreateWindowFrom(m_config->window_hdwn)))
			{
				err( "Couldn't create screen for video!: %s!\n", SDL_GetError());
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

		// 4.打开视频渲染驱动
		int32_t best_effort_rdrdrv  = 0;
		if ((effort_rdrdrv_nums = SDL_GetNumRenderDrivers()) <= 0)
		{
			err( "No built-in render drivers\n\n");
			return false;
		}else {
			out("Built-in render driver[%d]:\n", effort_rdrdrv_nums);
			SDL_RendererInfo rdrdrv = { 0 };
			for (int32_t i = 0; i<effort_rdrdrv_nums; ++i)
			{//Note:direct3d & opengl is not threadsafe,maybe bugs of SDL2.
				if (SDL_GetRenderDriverInfo(i, &rdrdrv)) continue;
				m_config->vcodec_pars.rdrdrvs.insert(std::pair<int32_t, std::string>(i, rdrdrv.name));
				if (!strcmp(rdrdrv.name, "direct3d"))
					best_effort_rdrdrv = i;
				if (IsStrEffect(m_config->window_rdrv.c_str())
					&& !strcmp(rdrdrv.name, m_config->window_rdrv.c_str()) )
					best_effort_rdrdrv = i;
				out("	#%d %s\n", i, rdrdrv.name);
			}
			SDL_GetRenderDriverInfo(best_effort_rdrdrv, &rdrdrv);
			m_config->window_rdrv = IsStrEffect(m_config->window_rdrv.c_str())
				? m_config->window_rdrv.c_str() : rdrdrv.name;
			out("--->Using render driver: %s\n\n", m_config->window_rdrv.c_str());
		}
		if (nullptr == (m_render = SDL_CreateRenderer(m_window, 
			best_effort_rdrdrv, SDL_RENDERER_ACCELERATED|SDL_RENDERER_PRESENTVSYNC)))
		{
			err( "Couldn't create render for video!: %s!\n", SDL_GetError());
			return false;
		}

		// 5.创建视频渲染纹理
		int32_t v_format = fmtconvert(AVMEDIA_TYPE_VIDEO, m_config->vcodec_pars.v_format);
		if (nullptr == (m_textur = SDL_CreateTexture(m_render, v_format,// SDL_PIXELFORMAT_IYUV;=YUV420p
			SDL_TEXTUREACCESS_STREAMING, m_config->vcodec_pars.pix_with, m_config->vcodec_pars.pix_high)))
		{
			err( "Couldn't create texture for video!:%s!\n", SDL_GetError());
			return false;
		}
		out("@@@ Open Video device success! display=%s window_id= 0x%x (0-default)\n",
			m_config->window_disp.c_str(),(long)m_window);
	}

	return true;
}

void
VideoMrender::closeVideoDevice(bool is_mrender)
{
	if (is_mrender)
	{		
		std::lock_guard<std::mutex> locker(window_lock);

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

int32_t
AudioMrender::Q_size(void)
{
	return m_render_Q.size();
}

int32_t
AudioMrender::cached(void)
{
	int32_t queue_size = (int32_t)SDL_GetQueuedAudioSize(m_audio_devID);
	msg("SDL_GetQueuedAudioSize=%d\n", queue_size);
	return queue_size;
}

STATUS
AudioMrender::status(void)
{
	return m_status;
}

void
AudioMrender::pause(bool pauseflag)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	m_config->speakr_paus = pauseflag;
}

void 
AudioMrender::start()
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);
	
	m_signal_quit = false;
	m_worker = std::thread([&]()
	{
		int32_t ret = -1;
		for (int64_t loop = 0; !m_signal_quit; ++loop, m_last_loop = av_gettime())
		{
			// 1.receive and read raw pcm datas.
			std::shared_ptr<MRframe> av_frm = nullptr;
			if (!m_render_Q.try_peek(av_frm)) {
				sleepMs(STANDARDTK);
				continue;
			}
			// 2.check and reset SDL2 devices.
			if (	m_config->acodec_pars.a_format != av_frm->pfrm->format
				||	m_config->acodec_pars.smp_nums != av_frm->pfrm->nb_samples
				||	m_config->acodec_pars.channels != av_frm->pfrm->channels
				||	m_config->acodec_pars.smp_rate != av_frm->pfrm->sample_rate)
			{//update codec pars.
				m_config->acodec_pars.a_format = av_frm->pfrm->format;
				m_config->acodec_pars.smp_nums = av_frm->pfrm->nb_samples;
				m_config->acodec_pars.channels = av_frm->pfrm->channels;
				m_config->acodec_pars.smp_rate = av_frm->pfrm->sample_rate;
				m_signal_rset = true;
				if ((m_config->acodec_pars.smp_nums & (m_config->acodec_pars.smp_nums - 1)))
					war("Audio maybe brokenly for SDL2 limit![smp_nums=%d]\n", m_config->acodec_pars.smp_nums);
			}
			if (m_signal_rset)
			{//Bug: some audio->nb_samples is changed frequently.
				if (!resetAudioDevice(true)) {
					SET_STATUS(m_status, E_STRTERR);
					for (ret = 0; ret < 300 && !m_signal_quit; ++ret)
						sleepMs(STANDARDTK);//3s.
					continue;
				}
				av_pcmalaw_freep(std::get<0>(m_buffer));// cahce av_frm data buffer.
				std::get<0>(m_buffer) = av_pcmalaw_clone(av_frm->pfrm);
				if (nullptr == std::get<0>(m_buffer)) break;
				std::get<1>(m_buffer) = av_samples_get_buffer_size(nullptr, av_frm->pfrm->channels, 
					av_frm->pfrm->nb_samples, (enum AVSampleFormat)av_frm->pfrm->format, 1);
				m_signal_rset = false;
				SET_STATUS(m_status, E_STARTED);
			}
			if (!(av_pcmalaw_clone2buffer(av_frm->pfrm, std::get<0>(m_buffer), std::get<1>(m_buffer))))
				continue;
			// 3.get audio data for playback.
			if (!m_config->speakr_paus) {
				if ((ret = SDL_QueueAudio(m_audio_devID, std::get<0>(m_buffer), std::get<1>(m_buffer))))
					continue;
				m_curpts = av_frm->upts;
				m_render_Q.popd(av_frm);
			}
			// 4.callback...
			if (!m_observer.expired())
				m_observer.lock()->onMPts(AVMEDIA_TYPE_AUDIO, m_curpts);
			SET_STATUS(m_status, ((m_status == E_STOPING) ? E_STOPING : E_RUNNING));
		}
		av_log(nullptr, AV_LOG_WARNING, "Audio Mrender finished! m_signal_quit=%d\n", m_signal_quit);
	});
}

void
AudioMrender::stopd(bool stop_quik)
{
	CHK_RETURN(E_STOPPED == status() || E_STOPING == status());
	SET_STATUS(m_status,E_STOPING);
	m_signal_quit = true;
	if (m_worker.joinable()) m_worker.join();
	closeAudioDevice(true);
	clearAudioRqueue(true);
	SET_STATUS(m_status, E_STOPPED);
}

void
AudioMrender::onMFrm(std::shared_ptr<MRframe> av_frm)
{
	if (-1 == fmtconvert(av_frm->pars->codec_type, av_frm->pars->format))
	{// Note: Only resmple for unsupported foramts,convert to fltp.
		AVFrame* pfrm = audio_frame_alloc(AV_SAMPLE_FMT_FLTP,
			av_frm->pfrm->channel_layout, av_frm->pfrm->sample_rate, av_frm->pfrm->nb_samples);
		if (!audio_resmple(&m_swrctx, pfrm, av_frm->pfrm)) {
			err("Audio: pfrm=%p resmple failed...\n", pfrm);
			audio_frame_freep(&pfrm);
			return;
		}
		// update av_frm parameters...
		av_frame_free(&av_frm->pfrm);
		av_frm->pfrm = pfrm;
		av_frm->pars->sample_rate = av_frm->pfrm->sample_rate;
		av_frm->pars->channels = av_frm->pfrm->channels;
		av_frm->pars->channel_layout = av_frm->pfrm->channel_layout;
		av_frm->pars->format = av_frm->pfrm->format;

	}
	if (CHK_PROPERTY(av_frm->prop, P_SEEK)) {
		m_render_Q.clear();
		msg("audio: clear render Que...\n");
	}
	if (CHK_PROPERTY(av_frm->prop, P_PAUS)) {
		dbg("audio: pause render Pkt...\n");
		return;
	}
	if (!m_signal_quit)
		m_render_Q.push(av_frm);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void
AudioMrender::updateAttributes(void)
{
	if (m_audio_devID >= 2)
		SDL_PauseAudioDevice(m_audio_devID, m_config->speakr_paus);
}

void AudioMrender::clearAudioRqueue(bool is_mrender)
{
	if (is_mrender)
		m_render_Q.clear();
}

bool 
AudioMrender::opendAudioDevice(bool is_mrender )
{
	OnceTaskInit();

	int32_t effort_driver_nums = 0, effort_device_nums = 0, effort_rdrdrv_nums = 0;
	char*   effort_driver_name = 0, *effort_device_name = 0, *effort_rdrdrv_name = 0;

	if (is_mrender)
	{
		std::lock_guard<std::mutex> locker(speakr_lock);
		// 1.打开音频设备驱动.
		if ((effort_driver_nums = SDL_GetNumAudioDrivers()) <= 0) {
			err( "No built-in audio drivers\n\n");
			return false;
		}else {
			out("Built-in audio drivers[%d]:\n", effort_driver_nums);
			effort_driver_name = (char*)SDL_GetAudioDriver(0);
			for (int32_t i = 0; i < effort_driver_nums; ++i)
			{//default driver:SDL_GetAudioDriver(0)
				const char* driver = SDL_GetAudioDriver(i);
				if (nullptr == driver) continue;
				m_config->acodec_pars.drivers.insert(std::pair<int32_t, std::string>(i, driver));
				if (strstr(driver, "directsound"))
					effort_driver_name = (char*)driver;
				out("	#%d: %s\n", i, driver);
			}
			static std::once_flag once;
			std::call_once(once, [&](void)->void
			{// Warning: Inner will close relative window. Ensure call once!
				m_config->speakr_driv = IsStrEffect(m_config->speakr_driv.c_str())
					? m_config->speakr_driv.c_str() : effort_driver_name;
				if (SDL_AudioInit(m_config->speakr_driv.c_str()))
					err("SDL_AudioInit %s\n\n", SDL_GetError());
				out("--->Using audio driver: %s \n\n", m_config->speakr_driv.c_str());
			});
		}
		// 2.填充音频设备参数.
		m_desire_spec.freq		= m_config->acodec_pars.smp_rate;
		m_desire_spec.format	= fmtconvert(AVMEDIA_TYPE_AUDIO, m_config->acodec_pars.a_format);
		m_desire_spec.channels	= m_config->acodec_pars.channels;
		m_desire_spec.silence	= 0;
		m_desire_spec.samples	= m_config->acodec_pars.smp_nums;//must be 2^n samples,or noise.
		m_desire_spec.callback	= nullptr;//use push mode,not pull mode.
		m_desire_spec.userdata	= nullptr;
		// 3.打开具体音频设备. [0-渲染,1-采集]
		if ((effort_device_nums = SDL_GetNumAudioDevices(!is_mrender)) <= 0) {
			err( "No speaker devices found.\n\n");
			return false;
		}else {
			for (int32_t i = 0; i < effort_device_nums; i++) {
				const char *device = SDL_GetAudioDeviceName(i, !is_mrender);
				if (nullptr == device) continue;
				m_config->acodec_pars.devices.insert(std::pair<int32_t, std::string>(i, device));
				if (0 == i) //default speaker:SDL_GetAudioDeviceName(0, 0)
					effort_device_name = (char*)device;
				out("	#%d: %s\n", i, device);
			}
			m_config->speakr_name = IsStrEffect(m_config->speakr_name.c_str())
				? m_config->speakr_name.c_str() : effort_device_name;
		}
		m_audio_devID = SDL_OpenAudioDevice(m_config->speakr_name.c_str(), 0,
			&m_desire_spec, &m_device_spec, SDL_AUDIO_ALLOW_ANY_CHANGE);
		if (m_audio_devID < 2) {
			err( "Open device[%s] failed! %s!\n",
				m_config->speakr_name.c_str(), SDL_GetError());
			return false;
		}
		out("--->Using audio device: %s\n\n", m_config->speakr_name.c_str());
		// 4.启动音频渲染设备.
		SDL_PauseAudioDevice(m_audio_devID, 0);
		out("@@@ Open Audio device success! speaker[id=%d]=%s status=%d (1-playing)\n",
			m_audio_devID, m_config->speakr_name.c_str(), SDL_GetAudioDeviceStatus(m_audio_devID));
	}
	return true;
}

void 
AudioMrender::closeAudioDevice(bool is_mrender)
{
	if (is_mrender) {
		std::lock_guard<std::mutex> locker(speakr_lock);
		if (m_audio_devID > 0 && 
			SDL_AUDIO_STOPPED != SDL_GetAudioDeviceStatus(m_audio_devID))
		{
			if (SDL_GetQueuedAudioSize(m_audio_devID) > 0)
				SDL_ClearQueuedAudio(m_audio_devID);
			SDL_CloseAudioDevice(m_audio_devID);//Ensure safe for multiple instances.
			m_audio_devID = 0;//Fix!!! must reset to 0!!!
			m_signal_rset = true;
		}
	}
}

bool 
AudioMrender::resetAudioDevice(bool is_mrender)
{
	if (is_mrender) {
		closeAudioDevice(is_mrender);
		return opendAudioDevice(is_mrender);
	}
	return false;
}
