
#include "pubcore.hpp"


typedef struct AencConfig
{
	bool									pauseflag{ false };
	bool									avhwaccel{ false };
}AencConfig;

typedef struct VencConfig
{
	bool									pauseflag{ false };
	bool									avhwaccel{ false };
}VencConfig;

class IEncoderObserver
{
public:
	virtual	void onEncoderPackt(std::shared_ptr<MPacket> av_pkt) = 0;
// 	virtual void onEncoderStart(void) = 0;
// 	virtual void onEncoderStopd(void) = 0;
// 	virtual void onEncoderPause(void) = 0;
};
class IEncoder
{
public:
	/* configs functions. */
	virtual int32_t	ssidNo(void) = 0;
	virtual void	update(void* config) = 0;
	virtual void	config(void* config) = 0;
	virtual STATUS	status(void) = 0;
	virtual int32_t Q_size(void) = 0;
	virtual	void    pushFrame(std::shared_ptr<MRframe> av_frm) = 0;
	/* control functions. */
	virtual void	start(void) = 0;
	virtual void	stopd(bool stop_quik = false) = 0;
	virtual void	pause(bool pauseflag = false) = 0;
protected:
	virtual ~IEncoder() = default;
};

class IEncoderFactory
{
public:
	static std::shared_ptr<IEncoder>
		createAudioEncoder(std::shared_ptr<IEncoderObserver> observer = nullptr);
	static std::shared_ptr<IEncoder>
		createVideoEncoder(std::shared_ptr<IEncoderObserver> observer = nullptr);
};

class AudioEncoder :public IEncoder
{
public:
	AudioEncoder(std::shared_ptr<IEncoderObserver> observer = nullptr);
	~AudioEncoder();
	/* configs functions. */
	int32_t	ssidNo(void)									  override;
	STATUS	status(void)									  override;
	void	config(void* config)							  override;
	void	update(void* config)							  override;
	int32_t	Q_size(void)									  override;
	void    pushFrame(std::shared_ptr<MRframe> av_frm)	  override;
	/* control functions. */
	void	start(void)										  override;
	void	stopd(bool stop_quik = false)					  override;
	void	pause(bool pauseflag = false)					  override;
private:
	void updateAttributes(void);
	void clearCodec_Q(bool is_decoder = false);
	void closeCodecer(bool is_decoder = false);
	bool opendCodecer(bool is_decoder = false);
	bool resetCodecer(bool is_decoder = false);
private:
	std::weak_ptr<IEncoderObserver>				 m_observer;
	std::atomic<STATUS>							 m_status{ E_INVALID };
	int32_t										 m_ssidNo{ -1 };
	std::atomic<bool>							 m_signal_quit{ true };
	std::atomic<bool>							 m_signal_rset{ true };
												 
	std::mutex									 m_cmutex;
	int64_t										 m_last_loop{ av_gettime() };
	std::shared_ptr<MPacket>					 m_acache{ nullptr };
	std::shared_ptr<AencConfig>					 m_config;
	std::thread 								 m_worker;
	AT::SafeQueue<std::shared_ptr<MRframe>>		 m_encoder_Q;
	
	AVCodec*									 m_codec{ nullptr };
	AVStream*									 m_stream;
	AVRational									 m_framerate{ 0,1 };
	AVCodecParameters*							 m_codec_par{ nullptr };
	AVCodecContext*								 m_codec_ctx{ nullptr };
};

class VideoEncoder :public IEncoder
{
public:
	VideoEncoder(std::shared_ptr<IEncoderObserver> observer = nullptr);
	~VideoEncoder();
	/* configs functions. */
	int32_t	ssidNo(void)									  override;
	STATUS	status(void)									  override;
	void	config(void* config)							  override;
	void	update(void* config)							  override;
	int32_t	Q_size(void)									  override;
	void    pushFrame(std::shared_ptr<MRframe> av_frm)	  override;
	/* control functions. */
	void	start(void)										  override;
	void	stopd(bool stop_quik = false)					  override;
	void	pause(bool pauseflag = false)					  override;
private:
	void updateAttributes(void);
	void clearCodec_Q(bool is_decoder = false);
	void closeCodecer(bool is_decoder = false);
	bool opendCodecer(bool is_decoder = false);
	bool resetCodecer(bool is_decoder = false);
private:
	std::weak_ptr<IEncoderObserver>				 m_observer;
	std::atomic<STATUS>							 m_status{ E_INVALID };
	int32_t										 m_ssidNo{ -1 };
	std::atomic<bool>							 m_signal_quit{ true };
	std::atomic<bool>							 m_signal_rset{ true };
												 
	std::mutex									 m_cmutex;
	int64_t										 m_last_loop{ av_gettime() };
	std::shared_ptr<MPacket>					 m_vcache{ nullptr };
	std::shared_ptr<VencConfig>					 m_config;
	std::thread 								 m_worker;
	AT::SafeQueue<std::shared_ptr<MRframe>>		 m_encoder_Q;

	AVCodec*									 m_codec{ nullptr };
	AVRational									 m_framerate{ 0,1 };
	AVCodecParameters*							 m_codec_par{ nullptr };
	AVCodecContext*								 m_codec_ctx{ nullptr };
};