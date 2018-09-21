
#include "pubcore.hpp"

typedef struct EnmuxerPars
{
	double									duts_time{ 0 }; //duration/s.
	double									curr_time{ 0 }; //demuxer not render ts/s.
	std::vector<std::tuple<int32_t, void*>>	strm_info;
}EnmuxerPars;

typedef struct MemxConfig
{
	std::string								urls{ "" };
	bool									pauseflag{ false };
	int64_t									seek_time{ 0 };	//ms.
	int32_t									seek_flag{ 1 };
	int32_t									rdtimeout{ 0 };	//0-block,other/s.
	EnmuxerPars								mdmx_pars;		//read only cfgs.
}MemxConfig;

class IEnmuxerObserver
{// default use ffmpeg.
public:
	virtual void onEnmuxerPackt(std::shared_ptr<MPacket> av_pkt) = 0;
	// 	virtual void onDemuxerStart(void) = 0;
	// 	virtual void onDemuxerStopd(void) = 0;
	// 	virtual void onDemuxerPause(void) = 0;
	// 	virtual void onDemuxerSeekp(void) = 0;
};

class IEnmuxer
{
public:
	static std::shared_ptr<IEnmuxer> create(const char* inputf,
		std::shared_ptr<IEnmuxerObserver> observer = nullptr);
	/* configs functions. */
	virtual int32_t	ssidNo(void) = 0;
	virtual void	update(void* config) = 0;
	virtual void	config(void* config) = 0;
	virtual STATUS	status(void) = 0;
	virtual double  durats(void) = 0;
	/* control functions. */
	virtual void	start(void) = 0;
	virtual void	stopd(bool stop_quik = false) = 0;
	virtual void	pause(bool pauseflag = false) = 0;
	virtual void	seekp(int64_t seektp) = 0;
protected:
	virtual ~IEnmuxer() = default;
};

class MediaEnmuxer : public IEnmuxer
{
public:
	MediaEnmuxer(const char* output,
		std::shared_ptr<IEnmuxerObserver> observer = nullptr);
	~MediaEnmuxer();
	/* configs functions. */
	int32_t	ssidNo(void)							 override;
	void	update(void* config)					 override;
	void	config(void* config)					 override;
	STATUS	status(void)							 override;
	double  durats(void)							 override;
	/* control functions. */
	void start(void)								 override;
	void stopd(bool stop_quik = false)				 override;
	void pause(bool pauseflag = false)				 override;
	void seekp(int64_t seektp)						 override;
private:
	void updateAttributes(void);
	bool handleSeekAction(void);
	bool opendMudemuxer(bool is_demuxer = false);
	void closeMudemuxer(bool is_demuxer = false);
	bool resetMudemuxer(bool is_demuxer = false);
private:
	std::weak_ptr<IEnmuxerObserver>		m_observer;
	std::atomic<STATUS>					m_status{ E_INVALID };
	int32_t								m_ssidNo{ -1 };
	std::atomic<bool>					m_signal_quit{ true };
	std::atomic<bool>					m_signal_rset{ true };

	std::mutex							m_cmutex;
	bool								m_seek_done{ true };
	bool								m_seek_apkt{ true };
	bool								m_seek_vpkt{ true };
	int64_t								m_last_loop{ av_gettime() };
	std::shared_ptr<MPacket>			m_acache{ nullptr };
	std::shared_ptr<MPacket>			m_vcache{ nullptr };
	std::shared_ptr<MemxConfig>			m_config;
	std::thread 						m_worker;

	AVRational							m_av_fps{ 0,1 };
	AVFormatContext*					m_fmtctx{ nullptr };
};
