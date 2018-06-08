
#pragma  once

#include "pubcore.hpp"

/************************************************************************/
/*	                    	IMrender Beg                                */
/************************************************************************/
typedef struct AVCodecPars
{
	int32_t		a_format{ 0 };
	int32_t		channels{ 0 };
	int32_t		smp_rate{ 0 };
	int32_t		smp_nums{ 0 };
	int32_t		v_format{ 0 };
	int32_t		pix_with{ 0 };
	int32_t		pix_high{ 0 };
	std::map <int32_t, std::string> drivers;
	std::map <int32_t, std::string> devices;
	std::map <int32_t, std::string> rdrdrvs;
}AVCodecPars;

typedef struct ArdrConfig
{
	std::string						speakr_name{ "" };
	std::string 					speakr_driv{ "" };
	bool							speakr_mute{ 0 };
	bool							speakr_paus{ 0 };
	AVCodecPars						acodec_pars;//read only.
}ArdrConfig;

typedef struct VrdrConfig
{
	void*							window_hdwn{ nullptr };
	std::string 					window_driv{ "" };
	std::string 					window_disp{ "" };
	std::string 					window_rdrv{ "" };
	bool							window_hide{ 0 };//Hide.
	bool							window_paus{ 0 };
	bool							window_raise_up{ 0 };
	bool							window_fuscreen{ 0 };	
	bool							window_restored{ 0 };
	bool							window_resizabl{ 1 };
	bool							window_bordered{ 1 };
	std::tuple<int32_t, int32_t>	window_position{ SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED };
	std::tuple<int32_t, int32_t>	window_size{ 640, 480 };
	std::tuple<int32_t, int32_t>	window_default_size{ 1920 ,1080 };
	std::tuple<int32_t, int32_t>	window_mininum_size{ 0 , 0 };
	std::tuple<int32_t, int32_t>	window_maxinum_size{ 4096, 2160 };
	AVCodecPars						vcodec_pars;//read only.
}VrdrConfig;

class IMrenderObserver
{
public:
	virtual void onMPoint(int32_t type, double upts) = 0;
// 	virtual void onRStart(void) = 0;
// 	virtual void onRStopd(void) = 0;
// 	virtual void onRPause(void) = 0;
};
class IMrender
{
public:
	/* configs functions. */
	virtual void	update(void* config) = 0;
	virtual void	config(void* config) = 0;
	virtual STATUS	status(void) = 0;
	virtual int32_t Q_size(void) = 0;
	virtual int32_t cached(void) = 0;// driver cached size.
	virtual	void    onMFrm(std::shared_ptr<MRframe> avfrm) = 0;
	/* control functions. */
	virtual void	start(void)  = 0;
	virtual void	stopd(bool stop_quik = false) = 0;
	virtual void	pause(bool pauseflag = false) = 0;
protected:
	virtual ~IMrender() = default;
};

class IMrenderFactory
{
public:
	static std::shared_ptr<IMrender>
		createAudioMrender(const char* speakr, std::shared_ptr<IMrenderObserver> observer = nullptr);
	static std::shared_ptr<IMrender>
		createVideoMrender(const void* handle, std::shared_ptr<IMrenderObserver> observer = nullptr);
};

/************************************************************************/
/*	                    	IMrender End                                */
/************************************************************************/

class AudioMrender : public IMrender
{
public:
	AudioMrender(const char* speakr,
		std::shared_ptr<IMrenderObserver> observer = nullptr);
	~AudioMrender();
	/* configs functions. */
	STATUS	status(void)							 override;
	void	config(void* config)					 override;
	void	update(void* config)					 override;
	int32_t	Q_size(void)							 override;
	int32_t	cached(void)							 override;
	void	onMFrm(std::shared_ptr<MRframe> av_frm)	 override;
	/* control functions. */
	void	start(void)								 override;
	void	stopd(bool stop_quik = false)			 override;
	void	pause(bool pauseflag = false)			 override;
private:
	void updateAttributes(void);
	void clearAudioRqueue(bool is_mrender = true);
	bool opendAudioDevice(bool is_mrender = true);
	void closeAudioDevice(bool is_mrender = true);
	bool resetAudioDevice(bool is_mrender = true);
private:// Audio 
	std::weak_ptr<IMrenderObserver> m_observer;
	std::atomic<STATUS>			m_status{ E_INVALID };
	std::atomic<bool>			m_signal_quit{ true };
	std::atomic<bool>			m_signal_rset{ true };

	std::mutex					m_cmutex;
	std::tuple<char*, int32_t>	m_buffer{ nullptr,0 };
	double						m_curpts{ 0 };
	std::shared_ptr<ArdrConfig>	m_config;		    //配置参数
	std::thread 				m_worker;		    //线程句柄
	AT::safe_queue<std::shared_ptr<MRframe>> m_render_Q;

	SDL_AudioSpec 				m_desire_spec{ 0 }; //音频参数
	SDL_AudioSpec 				m_device_spec{ 0 };
	SDL_AudioDeviceID 			m_audio_devID{ 0 }; //音频设备
	SDL_Event      				m_aevent;
};

class VideoMrender : public IMrender
{
public:
	VideoMrender(const void* handle = nullptr, 
		std::shared_ptr<IMrenderObserver> observer = nullptr);
	~VideoMrender();
	/* configs functions. */
	STATUS	status(void)							 override;
	void	config(void* config)					 override;
	void	update(void* config)					 override;
	int32_t	Q_size(void)							 override;
	int32_t	cached(void)							 override;
	void	onMFrm(std::shared_ptr<MRframe> av_frm)	 override;
	/* control functions. */
	void	start(void)								 override;
	void	stopd(bool stop_quik = false)			 override;
	void	pause(bool pauseflag = false)			 override;
private:
	void updateAttributes(void);
	void clearVideoRqueue(bool is_mrender = true);
	bool opendVideoDevice(bool is_mrender = true);
	void closeVideoDevice(bool is_mrender = true);
	bool resetVideoDevice(bool is_mrender = true);
private:
	std::weak_ptr<IMrenderObserver> m_observer;
	std::atomic<STATUS>			m_status{ E_INVALID };
	std::atomic<bool>			m_signal_quit{ true };
	std::atomic<bool>			m_signal_rset{ true };

	std::mutex					m_cmutex;
	std::tuple<char* , int32_t>	m_buffer{ nullptr,0 };
	double						m_curpts{ 0 };
	std::shared_ptr<VrdrConfig>	m_config;			//配置参数
	std::thread 				m_worker;
	AT::safe_queue<std::shared_ptr<MRframe>> m_render_Q;

	SDL_Window*    				m_window{ nullptr };//窗口句柄
	SDL_Window*    				m_sample{ nullptr };//窗口句柄<外部参考>
	SDL_Renderer*   			m_render{ nullptr };//渲染句柄
	SDL_Texture*    			m_textur{ nullptr };//纹理句柄
	SDL_Rect       				m_rectgl;			//矩形区域	
	SDL_Event      				m_vevent;			//事件循环
};
