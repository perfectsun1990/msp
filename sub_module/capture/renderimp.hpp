
#pragma  once

#include <iostream>
#include <memory>
#include <thread>
#include <future>
#include <atomic>
#include <chrono>
#include <vector>
#include <string>
#include <condition_variable>
#include <mutex>
#include <queue>
#include <functional>
#include <cassert>

#ifdef __cplusplus
extern "C"
{
#endif
#include <SDL.h>
#include <SDL_thread.h>
#ifdef __cplusplus
}
#endif

class IVideoRender
{
public:
	static std::shared_ptr<IVideoRender> create(void* winhdnw);
	virtual void updateHandle(void *window) = 0;
	virtual void onVideoFrame(const char *data, int32_t size,
		int32_t width, int32_t height, int32_t format) = 0;
protected:
	virtual ~IVideoRender() = default;
};

class IAudioRender
{
public:
	static std::shared_ptr<IAudioRender> create(char* speaker);
	virtual void updateSpeakr(char *speaker) = 0;
	virtual	void onAudioFrame(const char* data, int32_t size,
		int32_t sample_rate, int32_t nb_channels, int32_t nb_samples, int32_t format) = 0;
protected:
	virtual ~IAudioRender() = default;
};


using second_t = std::chrono::duration<int32_t>;
using millis_t = std::chrono::duration<int32_t, std::milli>;
using micros_t = std::chrono::duration<int32_t, std::micro>;
using nannos_t = std::chrono::duration<int32_t, std::nano >;

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
	int32_t		size{ 0 };
	char*		data{ nullptr };
};

class AVrender : public IVideoRender, public IAudioRender
{
public:
	AVrender(void* winhdnw = nullptr, char* speaker = nullptr);
	~AVrender();
	void updateHandle(void *winhdnw) override;
	void updateSpeakr(char *speaker) override;
	void onAudioFrame(const char* data, int32_t size,int32_t sample_rate, int32_t nb_channels,
		int32_t nb_samples, int32_t format)				override;
	void onVideoFrame(const char *data, int32_t size,
		int32_t width, int32_t height, int32_t format)	override;
private:
	void startAudioRender();
	void stopdAudioRender(bool stop_quik = false);
	void startVideoRender();
	void stopdVideoRender(bool stop_quik = false);
	int32_t reOpenAVRenderDevice(int32_t av_type);
private:
	// Audio 
	std::thread 			m_arender_worker;
	std::queue<AVRenderFrm>	m_arender_Q;
	std::mutex				m_arender_Q_mutx;
	std::condition_variable	m_arender_Q_cond;
	SDL_AudioSpec 			m_desire_spec{ 0 };
	SDL_AudioSpec 			m_device_spec{ 0 };
	SDL_AudioDeviceID 		m_audio_devID{ 0 };
	std::string 			m_speaker{ "" };
	// Video
	std::thread 			m_vrender_worker;
	std::queue<AVRenderFrm>	m_vrender_Q;
	std::mutex				m_vrender_Q_mutx;
	std::condition_variable	m_vrender_Q_cond;
	SDL_Window*    			m_pscreen{ nullptr };   //窗口句柄
	SDL_Renderer*   		m_prender{ nullptr };   //渲染句柄
	SDL_Texture*    		m_texture{ nullptr };   //纹理句柄
	SDL_Rect       			m_rect;					//矩形区域
	void* 					m_winhdnw{ nullptr };
	// AVdec.
	AVCodecPars				m_codecpar;
	std::atomic<bool>		m_SDL_init{ false };
	//SDL_Thread*     		refresh_thread = NULL;  //线程数据结构
	SDL_Event      			m_av_event;              //事件数据结构
	std::atomic<bool>		m_stop_audio{ false };
	std::atomic<bool>		m_stop_video{ false };
};