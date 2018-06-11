
#include "pubcore.hpp"
#include "demuxer.hpp"

std::shared_ptr<IDemuxer> IDemuxer::create(const char * inputf, 
	std::shared_ptr<IDemuxerObserver> observer)
{
	return std::make_shared<MediaDemuxer>(inputf, observer);
}

MediaDemuxer::MediaDemuxer(const char * inputf, std::shared_ptr<IDemuxerObserver> observer)
	: m_observer(observer)
{
	SET_STATUS(m_status,E_INVALID);
	
	if (!effective(inputf))	return;

	m_config = std::make_shared<MdmxConfig>();
	m_config->urls = inputf;

	m_acache = std::make_shared<MPacket>();
	m_acache->type = AVMEDIA_TYPE_AUDIO;
	SET_PROPERTY(m_acache->prop, P_PAUS);
	m_vcache = std::make_shared<MPacket>();
	m_vcache->type = AVMEDIA_TYPE_VIDEO;
	SET_PROPERTY(m_vcache->prop, P_PAUS);

	SET_STATUS(m_status, E_INITRES);
}

MediaDemuxer::~MediaDemuxer()
{
	stopd(true);
}

void 
MediaDemuxer::start(void)
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);

	m_signal_quit = false;
	m_worker = std::thread([&](void)
	{
		while (!m_signal_quit)
		{
			// 1.探测并处理pause动作.
			if (m_config->pauseflag)
			{//send av cache packets to callback....
				if (!m_observer.expired()) {
					m_observer.lock()->onMPkt(m_acache);
					m_observer.lock()->onMPkt(m_vcache);
				}
				av_usleep(10 * 1000);//less then this.
				continue;
			}
			// 2.探测并处理reset动作.
			std::shared_ptr<MPacket> av_pkt = std::make_shared<MPacket>();
			if (!m_fmtctx || m_config->urls.compare(m_fmtctx->filename))
				m_signal_rset = true;
			if (m_signal_rset)
			{
				if (!resetMudemuxer(true))
					break;
				SET_PROPERTY(av_pkt->prop, P_BEGP);
				m_signal_rset = false;
			}

			// 3.探测并处理seekp动作.
			if (nullptr == av_pkt->ppkt || !handleSeekActs(m_config->seek_time))
				break;

			// 4.读取解码前data数据.
			int32_t	ret = -1;
			if ((ret = av_read_frame(m_fmtctx, av_pkt->ppkt)) < 0)
			{
				if (ret != AVERROR_EOF) {
					av_log(nullptr, AV_LOG_ERROR, "av_read_frame failed! ret=%d\n", ret);
					break;
				}
				av_usleep(10 * 1000);
				continue;
			}

			// 5.处理媒体属性及封包.
			av_pkt->type = m_fmtctx->streams[av_pkt->ppkt->stream_index]->codecpar->codec_type;
			av_pkt->sttb = m_fmtctx->streams[av_pkt->ppkt->stream_index]->time_base;
			av_pkt->upts = av_pkt->ppkt->pts * av_q2d(av_pkt->sttb);
			av_pkt->ufps = (av_pkt->type == AVMEDIA_TYPE_VIDEO) ? m_av_fps : av_pkt->ufps;
			if ((ret = avcodec_parameters_copy(av_pkt->pars, m_fmtctx->streams[av_pkt->ppkt->stream_index]->codecpar)) < 0) {
				av_log(nullptr, AV_LOG_ERROR, "avcodec_parameters_copy failed! ret=%d\n", ret);
				break;
			}
			if (av_pkt->type == AVMEDIA_TYPE_AUDIO)
			{// update audio cache and status.
				if (!m_seek_apkt) {
					SET_PROPERTY(av_pkt->prop, P_SEEK);
					m_seek_apkt = true;
				}
				m_acache->upts = av_pkt->upts;
			}
			if (av_pkt->type == AVMEDIA_TYPE_VIDEO)
			{// update video cache and status.
				if (!m_seek_vpkt) {
					SET_PROPERTY(av_pkt->prop, P_SEEK);
					m_seek_vpkt = true;
				}
				m_vcache->upts = av_pkt->upts;
			}

			// 6.callback...
			if (!m_observer.expired())
				m_observer.lock()->onMPkt(av_pkt);
		}
		av_log(nullptr, AV_LOG_WARNING, "Media Demuxer finished! m_signal_quit=%d\n", m_signal_quit);
	});

	SET_STATUS(m_status, E_STARTED);
}

void
MediaDemuxer::stopd(bool stop_quik)
{
	CHK_RETURN(E_STARTED != status());
	SET_STATUS(m_status, E_STOPING);
	m_signal_quit = true;
	if (m_worker.joinable()) m_worker.join();
	closeMudemuxer(true);
	SET_STATUS(m_status, E_STOPPED);
}

void
MediaDemuxer::pause(bool pauseflag)
{
	MdmxConfig cfg;
	config(&cfg);
	cfg.pauseflag = pauseflag;
	update(&cfg);
}

void
MediaDemuxer::config(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config)
		*((MdmxConfig*)config) = *m_config;
}

void
MediaDemuxer::update(void* config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	MdmxConfig* cfg = (MdmxConfig*)config;
	if (nullptr != config)
	{
		if (m_config->urls != cfg->urls)
			m_signal_rset = true;
		*m_config = *cfg;
		updateAttributes();
	}
}

STATUS 
MediaDemuxer::status(void)
{
	return m_status;
}

int64_t 
MediaDemuxer::durats(void)
{
	return (E_INVALID == status()) ? -1 :
		m_fmtctx->duration / 1000;
}

void 
MediaDemuxer::seekp(int64_t seektp)
{
	MdmxConfig cfg;
	config(&cfg);	
	if (m_seek_done)
	{
		m_seek_done = false;
		cfg.seek_flag = AVSEEK_FLAG_BACKWARD;
		cfg.seek_time = seektp;
		update(&cfg);
		av_log(nullptr, AV_LOG_INFO, "seek to %lld ms \n", cfg.seek_time / 1000);
	}
}

bool
MediaDemuxer::handleSeekActs(int64_t seek_tp)
{
	MdmxConfig cfg;
	config(&cfg);
	if (!m_seek_done)
	{
		cfg.seek_time = (seek_tp > m_fmtctx->duration) ? m_fmtctx->duration : seek_tp;
		cfg.seek_time = (seek_tp < 0) ? 0 : seek_tp;
		if (av_seek_frame(m_fmtctx, -1, cfg.seek_time + m_fmtctx->start_time, cfg.seek_flag) < 0)
		{
			av_log(nullptr, AV_LOG_ERROR, "Can't seek stream: %lld", cfg.seek_time);
			return false;
		}
		m_seek_done = true;
		m_seek_apkt = false;
		m_seek_vpkt = false;
		update(&cfg);
	}
	return true;
}

int32_t
MediaDemuxer::streamNums(void)
{
	return (E_INVALID == status()) ? -1 :
		m_fmtctx->nb_streams;
}

int32_t
MediaDemuxer::streamType(int32_t indx)
{
	return (E_INVALID == status()
		|| indx < (int32_t)AVMEDIA_TYPE_VIDEO
		|| indx >(int32_t)AVMEDIA_TYPE_NB) ? -1 :
		(int32_t)m_fmtctx->streams[indx]->codecpar->codec_type;
}

void*
MediaDemuxer::streamInfo(int32_t type)
{
	if (E_INVALID != status())
	{
		for (uint32_t i = 0; i < m_fmtctx->nb_streams; i++)
			if ((int32_t)m_fmtctx->streams[i]->codecpar->codec_type == type)
				return m_fmtctx->streams[i];
	}
	return nullptr;
}

void 
MediaDemuxer::updateAttributes(void)
{

}

bool
MediaDemuxer::opendMudemuxer(bool is_demuxer)
{
	// 1.Register all codec,foramt,filter,device,protocol...
	av_register_all();
	avdevice_register_all();
	avfilter_register_all();
	avformat_network_init();

	if (is_demuxer)
	{
		if (nullptr == m_fmtctx)
		{
			// 2.Fill m_fmtctx and m_fmtctx->iformat<AVInputFormat> etc.
			int32_t ret = -1;
			if ((ret = avformat_open_input(&m_fmtctx, m_config->urls.c_str(), nullptr, nullptr)) < 0) {
				av_log(nullptr, AV_LOG_ERROR, "Open file failed! urls=%s!\n", m_config->urls.c_str());
				return false;
			}
			if ((ret = avformat_find_stream_info(m_fmtctx, nullptr)) < 0) {
				av_log(nullptr, AV_LOG_ERROR, "Find stream info failed!%s\n", m_config->urls.c_str());
				return false;
			}
			for (uint32_t i = 0; i < m_fmtctx->nb_streams; i++)
			{
				if ((int32_t)m_fmtctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
					m_av_fps = av_guess_frame_rate(m_fmtctx, m_fmtctx->streams[i], nullptr);
			}
				
			av_dump_format(m_fmtctx, 0, m_config->urls.c_str(), !is_demuxer);
		}
	}

	return true;
}

void 
MediaDemuxer::closeMudemuxer(bool is_demuxer)
{
	if (is_demuxer)
	{
		if (nullptr != m_fmtctx)
			avformat_close_input(&m_fmtctx);
		m_signal_rset = true;
	}
}

bool
MediaDemuxer::resetMudemuxer(bool is_demuxer)
{
	if (is_demuxer)
	{
		closeMudemuxer(is_demuxer);
		return opendMudemuxer(is_demuxer);
	}
	return false;
}
