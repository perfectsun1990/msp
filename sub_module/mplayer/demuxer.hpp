
#include "pubcore.hpp"

class IMediaDemuxerObserver
{// default use ffmpeg.
public:
	virtual	void onAudioPacket(std::shared_ptr<MPacket> av_pkt) = 0;
	virtual void onVideoPacket(std::shared_ptr<MPacket> av_pkt) = 0;
};

class IMediaDemuxer
{
public:
	static std::shared_ptr<IMediaDemuxer> create(const char* inputf,
		std::shared_ptr<IMediaDemuxerObserver> observer = nullptr);
	virtual void start(void)									= 0;
	virtual void stopd(bool stop_quik = false) = 0;
	virtual void pause(bool pauseflag = false) = 0;
	virtual void	update(const char *inputf)					= 0;
	virtual STATUS  status(void)								= 0;
protected:
	virtual ~IMediaDemuxer() = default;
};

// 解复用器
class MediaDemuxer :public IMediaDemuxer
{
public:
	MediaDemuxer(const char* inputf, 
		std::shared_ptr<IMediaDemuxerObserver> observer = nullptr);
	~MediaDemuxer();

	void start(void)									   override;
	void stopd(bool stop_quik = false)					   override;
	void pause(bool pauseflag = false)					   override;
	void	update(const char *inputf)					   override;
	STATUS	status(void)								   override;
	int64_t duration(void);
	void	seek2pos(int64_t seektp);
private:
	void updateStatus(STATUS status);
	bool opendMudemer(bool is_demuxer = false);
	void closeMudemer(bool is_demuxer = false);
	bool resetMudemer(bool is_demuxer = false);
	bool seekingPoint(int64_t seek_tp = 0);
	int32_t streamNums(void);
	int32_t streamType(int32_t indx);
	void *  streamInfo(int32_t type);
private:
	std::atomic<bool>		m_signal_quit{ true };
	std::atomic<STATUS>		m_status{ E_INVALID };	
	const void* 			m_window{ nullptr };
	std::string 			m_inputf{""};
	void*					m_config;//配置参数
	AVFormatContext*		m_fmtctx{ nullptr };
	AVRational				m_av_fps{ 0,1 };
	std::thread 			m_worker;
// 	std::atomic<bool>		m_done{ false };
// 	std::atomic<bool>		m_stop{ false };	
	std::atomic<int64_t>	m_seek_time{ 0 };
	std::atomic<int32_t>	m_seek_flag{ 1 };
	std::atomic<bool>		m_seek_done{ true };
	std::atomic<bool>		m_seek_apkt{ true };
	std::atomic<bool>		m_seek_vpkt{ true };
	std::atomic<bool>		m_pauseflag{ false };
	std::weak_ptr<IMediaDemuxerObserver> m_observe;
};
