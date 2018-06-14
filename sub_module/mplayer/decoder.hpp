
#include "pubcore.hpp"


typedef struct AdecConfig
{
	bool									pauseflag{ false };
;
}AdecConfig;

typedef struct VdecConfig
{
	bool									pauseflag{ false };
}VdecConfig;

class IDecoderObserver
{
public:
	virtual	void onMFrm(std::shared_ptr<MRframe> av_frm) = 0;
// 	virtual void onDecoderStart(void) = 0;
// 	virtual void onDecoderStopd(void) = 0;
// 	virtual void onDecoderPause(void) = 0;
};
class IDecoder
{
public:
	/* configs functions. */
	virtual void	update(void* config) = 0;
	virtual void	config(void* config) = 0;
	virtual STATUS	status(void) = 0;
	virtual int32_t Q_size(void) = 0;
	virtual	void    onMPkt(std::shared_ptr<MPacket> av_pkt) = 0;
	/* control functions. */
	virtual void	start(void) = 0;
	virtual void	stopd(bool stop_quik = false) = 0;
	virtual void	pause(bool pauseflag = false) = 0;
protected:
	virtual ~IDecoder() = default;
};

class IDecoderFactory
{
public:
	static std::shared_ptr<IDecoder> 
		createAudioDecoder(std::shared_ptr<IDecoderObserver> observer = nullptr);
	static std::shared_ptr<IDecoder> 
		createVideoDecoder(std::shared_ptr<IDecoderObserver> observer = nullptr);
};

// 解码器
class AudioDecoder :public IDecoder
{
public:
	AudioDecoder(std::shared_ptr<IDecoderObserver> observer = nullptr);
	~AudioDecoder();
	/* configs functions. */
	STATUS	status(void)									  override;
	void	config(void* config)							  override;
	void	update(void* config)							  override;
	int32_t	Q_size(void)									  override;
	void	onMPkt(std::shared_ptr<MPacket> av_pkt)			  override;
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
	std::weak_ptr<IDecoderObserver> m_observer;
	std::atomic<STATUS>			m_status{ E_INVALID };
	std::atomic<bool>			m_signal_quit{ true };
	std::atomic<bool>			m_signal_rset{ true };

	std::mutex					m_cmutex;
	std::shared_ptr<MRframe>	m_acache{ nullptr };
	std::shared_ptr<AdecConfig>	m_config;				//配置参数
	std::thread 				m_worker;
	AT::SafeQueue<std::shared_ptr<MPacket>> m_decoder_Q;
	
	AVCodec*					m_codec{ nullptr };
	AVRational					m_framerate{ 0,1 };
	AVCodecParameters*			m_codec_par{ nullptr };
	AVCodecContext*				m_codec_ctx{ nullptr };
};

// 解码器
class VideoDecoder :public IDecoder
{
public:
	VideoDecoder(std::shared_ptr<IDecoderObserver> observer = nullptr);
	~VideoDecoder();
	/* configs functions. */
	STATUS	status(void)									  override;
	void	config(void* config)							  override;
	void	update(void* config)							  override;
	int32_t	Q_size(void)									  override;
	void	onMPkt(std::shared_ptr<MPacket> av_pkt)			  override;
	/* control functions. */								  
	void	start(void)										  override;
	void	stopd(bool stop_quik = false)					  override;
	void	pause(bool pauseflag = false)					  override;
private:
	void updateAttributes(void);
	void clearCodec_Q(bool is_decoder = true);
	void closeCodecer(bool is_decoder = true);
	bool opendCodecer(bool is_decoder = true);
	bool resetCodecer(bool is_decoder = true);
private:
	std::weak_ptr<IDecoderObserver> m_observer;
	std::atomic<STATUS>			m_status{ E_INVALID };
	std::atomic<bool>			m_signal_quit{ true };
	std::atomic<bool>			m_signal_rset{ true };

	std::mutex					m_cmutex;
	std::shared_ptr<MRframe>	m_vcache{ nullptr };
	std::shared_ptr<VdecConfig>	m_config;				//配置参数
	std::thread 				m_worker;
	AT::SafeQueue<std::shared_ptr<MPacket>> m_decoder_Q;	
	
	AVCodec*					m_codec{ nullptr };
	AVRational					m_framerate{ 0,1 };
	AVCodecParameters*			m_codec_par{ nullptr };
	AVCodecContext*				m_codec_ctx{ nullptr };
};