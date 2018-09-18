
#include "pubcore.hpp"

typedef struct DemuxerPars
{
	double									duts_time{ 0 }; //duration/s.
	double									curr_time{ 0 }; //demuxer not render ts/s.
	std::vector<std::tuple<int32_t,void*>>	strm_info;
}DemuxerPars;

typedef struct MdmxConfig
{
	std::string								urls{ "" };
	bool									pauseflag{ false };
	int64_t									seek_time{ 0 };	//ms.
	int32_t									seek_flag{ 1 };
	int32_t									rdtimeout{ 0 };	//0-block,other/s.
	DemuxerPars								mdmx_pars;		//read only cfgs.
}MdmxConfig;

class IDemuxerObserver
{// default use ffmpeg.
public:
	virtual void onMPkt(std::shared_ptr<MPacket> av_pkt) = 0;
// 	virtual void onDemuxerStart(void) = 0;
// 	virtual void onDemuxerStopd(void) = 0;
// 	virtual void onDemuxerPause(void) = 0;
// 	virtual void onDemuxerSeekp(void) = 0;
};

class IDemuxer
{
public:
	static std::shared_ptr<IDemuxer> create(const char* inputf,
		std::shared_ptr<IDemuxerObserver> observer = nullptr);
	/* configs functions. */
	virtual void	update(void* config) = 0;
	virtual void	config(void* config) = 0;
	virtual STATUS	status(void) = 0;
	virtual double  durats(void) = 0;
	/* control functions. */
	virtual void	start(void)  = 0;
	virtual void	stopd(bool stop_quik = false) = 0;
	virtual void	pause(bool pauseflag = false) = 0;
	virtual void	seekp(int64_t seektp) = 0;
protected:
	virtual ~IDemuxer() = default;
};

class MediaDemuxer : public IDemuxer
{
public:
	MediaDemuxer(const char* inputf, 
		std::shared_ptr<IDemuxerObserver> observer = nullptr);
	~MediaDemuxer();
	/* configs functions. */
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
	std::weak_ptr<IDemuxerObserver>		m_observer;
	std::atomic<STATUS>					m_status{ E_INVALID };
	std::atomic<bool>					m_signal_quit{ true };
	std::atomic<bool>					m_signal_rset{ true };

	std::mutex							m_cmutex;
	bool								m_seek_done{ true };
	bool								m_seek_apkt{ true };
	bool								m_seek_vpkt{ true };
	int64_t								m_last_loop{ av_gettime() };
	std::shared_ptr<MPacket>			m_acache{ nullptr };
	std::shared_ptr<MPacket>			m_vcache{ nullptr };
	std::shared_ptr<MdmxConfig>			m_config;
	std::thread 						m_worker;

	AVRational							m_av_fps{ 0,1 };
	AVFormatContext*					m_fmtctx{ nullptr };
};
