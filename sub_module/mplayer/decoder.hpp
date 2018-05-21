﻿
#include "pubcore.hpp"

class IAudioDecoderObserver
{
public:
	virtual	void onAudioRFrame(std::shared_ptr<MRframe> av_frm) = 0;
};
class IAudioDecoder
{
public:
	static std::shared_ptr<IAudioDecoder> create(std::shared_ptr<IAudioDecoderObserver> observer = nullptr);
	virtual void start(void)									= 0;
	virtual void stopd(bool stop_quik = false)					= 0;
	virtual void pause(bool pauseflag = false)					= 0;
	virtual void	update(void* config = nullptr)				= 0;
	virtual STATUS  status(void)								= 0;
	virtual int32_t	Q_size(void)								= 0;
	virtual	void onAudioPacket(std::shared_ptr<MPacket> av_pkt) = 0;
protected:
	virtual ~IAudioDecoder() = default;
};

// 解码器
class AudioDecoder :public IAudioDecoder
{
public:
	AudioDecoder(std::shared_ptr<IAudioDecoderObserver> observer = nullptr);
	~AudioDecoder();
	void start(void)											override;
	void stopd(bool stop_quik = false)							override;
	void pause(bool pauseflag = false)							override;
	void	update(void* config = nullptr)						override;
	STATUS	status(void)										override;
	int32_t	Q_size(void)										override;
	void onAudioPacket(std::shared_ptr<MPacket> av_pkt)			override;
private:
	void updateStatus(STATUS status);
	void clearCodec_Q(bool is_decoder = false);
	void closeCodecer(bool is_decoder = false);
	bool opendCodecer(bool is_decoder = false);
	bool resetCodecer(bool is_decoder = false);	
private:
	std::atomic<bool>		m_signal_quit{ true };
	std::atomic<STATUS>		m_status{ E_INVALID };
	const void* 			m_window{ nullptr };
	std::string 			m_inputf{ "" };
	void*					m_config;				//配置参数
	AVFormatContext*		m_fmtctx{ nullptr };

	std::thread 			m_worker;
	std::condition_variable	m_decoder_Q_cond;
	std::mutex				m_decoder_Q_mutx;
	int32_t					m_decoder_Q_size;
	std::queue<std::shared_ptr<MPacket>> m_decoder_Q;
	// 	std::atomic<bool>		m_done{ false };
	// 	std::atomic<bool>		m_stop{ false };	
	std::atomic<int64_t>	m_seek_time{ 0 };
	std::atomic<int32_t>	m_seek_flag{ 1 };
	std::atomic<bool>		m_seek_done{ true  };
	std::atomic<bool>		m_pauseflag{ false };
	std::weak_ptr<IAudioDecoderObserver> m_observe;

	AVCodec*				m_codec{ nullptr };
	AVRational				m_framerate{ 0,1 };
	AVCodecParameters*		m_codec_par{ nullptr };
	AVCodecContext*			m_codec_ctx{ nullptr };
};

class IVideoDecoderObserver
{
public:
	virtual void onVideoRFrame(std::shared_ptr<MRframe> av_frm) = 0;
};
class IVideoDecoder
{
public:
	static std::shared_ptr<IVideoDecoder> create(std::shared_ptr<IVideoDecoderObserver> observer = nullptr);
	virtual void start(void)									= 0;
	virtual void stopd(bool stop_quik = false)					= 0;
	virtual void pause(bool pauseflag = false)					= 0;
	virtual void	update(void* config = nullptr)				= 0;
	virtual STATUS  status(void)								= 0;
	virtual int32_t	Q_size(void) = 0;
	virtual void onVideoPacket(std::shared_ptr<MPacket> av_pkt) = 0;
protected:
	virtual ~IVideoDecoder() = default;
};


// 解码器
class VideoDecoder :public IVideoDecoder
{
public:
	VideoDecoder(std::shared_ptr<IVideoDecoderObserver> observer = nullptr);
	~VideoDecoder();
	void start(void)											override;
	void stopd(bool stop_quik = false)							override;
	void pause(bool pauseflag = false)							override;
	void	update(void* config = nullptr)						override;
	STATUS	status(void)										override;
	int32_t	Q_size(void)										override;
	void onVideoPacket(std::shared_ptr<MPacket> av_pkt)			override;
private:
	void updateStatus(STATUS status);
	void clearCodec_Q(bool is_decoder = false);
	void closeCodecer(bool is_decoder = false);
	bool opendCodecer(bool is_decoder = false);
	bool resetCodecer(bool is_decoder = false);
private:
	std::atomic<bool>		m_signal_quit{ true };
	std::atomic<STATUS>		m_status{ E_INVALID };
	const void* 			m_window{ nullptr };
	std::string 			m_inputf{ "" };
	void*					m_config;				//配置参数
	AVFormatContext*		m_fmtctx{ nullptr };
	std::thread 			m_worker;
	std::condition_variable	m_decoder_Q_cond;
	std::mutex				m_decoder_Q_mutx;
	int32_t					m_decoder_Q_size;
	std::queue<std::shared_ptr<MPacket>> m_decoder_Q;
	// 	std::atomic<bool>		m_done{ false };
	// 	std::atomic<bool>		m_stop{ false };	
	std::atomic<int64_t>	m_seek_time{ 0 };
	std::atomic<int32_t>	m_seek_flag{ 1 };
	std::atomic<bool>		m_seek_done{ true  };
	std::atomic<bool>		m_pauseflag{ false };
	std::weak_ptr<IVideoDecoderObserver> m_observe;
	AVCodec*				m_codec{ nullptr };
	AVRational				m_framerate{ 0,1 };
	AVCodecParameters*		m_codec_par{ nullptr };
	AVCodecContext*			m_codec_ctx{ nullptr };
};