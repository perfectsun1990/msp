
#include "pubcore.hpp"
#include "demuxer.hpp"

std::shared_ptr<IMediaDemuxer> IMediaDemuxer::create(const char * inputf, 
	std::shared_ptr<IMediaDemuxerObserver> observer)
{
	return std::make_shared<MediaDemuxer>(inputf, observer);
}

MediaDemuxer::MediaDemuxer(const char * inputf, std::shared_ptr<IMediaDemuxerObserver> observer)
{
	updateStatus(E_INVALID);
	if (!effective(inputf))
		return;
	m_inputf  = inputf;
	m_observe = observer;
	updateStatus(E_INITRES);
}

MediaDemuxer::~MediaDemuxer()
{
	if (E_STOPPED != status())
		stopd(true);
	updateStatus(E_INVALID);
}

bool MediaDemuxer::seekingPoint(int64_t seek_tp)
{
	if (!m_seek_done)
	{
		m_seek_time = (seek_tp > m_fmtctx->duration) ? m_fmtctx->duration : seek_tp;
		m_seek_time = (seek_tp < 0) ? 0 : seek_tp;
		if (av_seek_frame(m_fmtctx, -1, m_seek_time + m_fmtctx->start_time, m_seek_flag) < 0)
		{
			//av_log(nullptr, AV_LOG_ERROR, "Can't seek stream: %lld", m_seek_time);
			return false;
		}
		m_seek_done = true;
		m_seek_apkt = false;
		m_seek_vpkt = false;
	}
	return true;
}

void MediaDemuxer::start(void)
{
	if (E_STARTED == status() || E_STRTING == status())
		return;
	updateStatus(E_STRTING);

	m_signal_quit = false;
	m_worker = std::thread([&](void)
	{
		int32_t	ret = -1;
		while (!m_signal_quit)
		{
			std::shared_ptr<MPacket> avpkt = std::make_shared<MPacket>();

			if (m_pauseflag)
			{
				SET_PROPERTY(avpkt->prop, P_PAUS);
				if (!m_observe.expired())
					m_observe.lock()->onAudioPacket(avpkt);
				if (!m_observe.expired())
					m_observe.lock()->onVideoPacket(avpkt);
				av_usleep(10*1000);
				continue;
			}

			if (!m_fmtctx || (m_fmtctx && m_inputf.compare(m_fmtctx->filename)))
			{
				if (!resetMudemer(true))
					break;
				SET_PROPERTY(avpkt->prop, P_BEGP);
			}

			// 1.探测并处理seek动作.
			if ( nullptr == avpkt->ppkt || !seekingPoint(m_seek_time))
				break;

			// 2.读取解码前data数据.
			if ((ret = av_read_frame(m_fmtctx, avpkt->ppkt)) < 0)
			{
				if (ret != AVERROR_EOF) {
					av_log(nullptr, AV_LOG_ERROR, "av_read_frame failed! ret=%d\n", ret);
					break;
				}
				av_usleep(10 * 1000);
				continue;
			}
			
			// 3.打包发送到解码器.
			avpkt->type = m_fmtctx->streams[avpkt->ppkt->stream_index]->codecpar->codec_type;
			avpkt->sttb = m_fmtctx->streams[avpkt->ppkt->stream_index]->time_base;
			avpkt->upts = avpkt->ppkt->pts * av_q2d(avpkt->sttb);
			avpkt->ufps = (avpkt->type == AVMEDIA_TYPE_VIDEO) ? m_av_fps : avpkt->ufps;
			if ((ret = avcodec_parameters_copy(avpkt->pars, m_fmtctx->streams[avpkt->ppkt->stream_index]->codecpar)) < 0) {
				av_log(nullptr, AV_LOG_ERROR, "avcodec_parameters_copy failed! ret=%d\n", ret);
				break;
			}
			if (avpkt->type == AVMEDIA_TYPE_AUDIO && !m_seek_apkt)
			{
				SET_PROPERTY(avpkt->prop, P_SEEK);
				m_seek_apkt = true;
			}
			if (avpkt->type == AVMEDIA_TYPE_VIDEO && !m_seek_vpkt)
			{
				SET_PROPERTY(avpkt->prop, P_SEEK);
				m_seek_vpkt = true;
			}

			// 4.callback...
			if (!m_observe.expired() && avpkt->type == AVMEDIA_TYPE_AUDIO)
				m_observe.lock()->onAudioPacket(avpkt);
			if (!m_observe.expired() && avpkt->type == AVMEDIA_TYPE_VIDEO)
				m_observe.lock()->onVideoPacket(avpkt);
		}
		av_log(nullptr, AV_LOG_WARNING, "Media Demuxer finished! ret=%d\n", ret);
	});

	updateStatus(E_STARTED);
}

void MediaDemuxer::stopd(bool stop_quik)
{
	if (E_STOPPED == status() || E_STOPING == status())
		return;
	updateStatus(E_STOPING);
	m_signal_quit = true;
	if (m_worker.joinable()) m_worker.join();
	updateStatus(E_STOPPED);
}

void MediaDemuxer::pause(bool pauseflag)
{
	m_pauseflag = pauseflag;
}

void MediaDemuxer::update(const char * inputf)
{
	if(effective(inputf))
		m_inputf = inputf;
}

STATUS MediaDemuxer::status(void)
{
	return m_status;
}

void MediaDemuxer::updateStatus(STATUS status)
{
	m_status = status;
}

int64_t 
MediaDemuxer::duration(void)
{
	return (E_INVALID == status()) ? -1 :
		m_fmtctx->duration / 1000;
}

void 
MediaDemuxer::seek2pos(int64_t seektp)
{
	if (m_seek_done)
	{
		m_seek_done = false;
		m_seek_time = seektp;
		m_seek_flag = AVSEEK_FLAG_BACKWARD;
		av_log(nullptr, AV_LOG_INFO, "seek to %lld ms \n", m_seek_time / 1000);
	}
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

bool 
MediaDemuxer::opendMudemer(bool is_demuxer)
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
			if ((ret = avformat_open_input(&m_fmtctx, m_inputf.c_str(), nullptr, nullptr)) < 0) {
				av_log(nullptr, AV_LOG_ERROR, "Open file failed! urls=%s!\n", m_inputf.c_str());
				return false;
			}
			if ((ret = avformat_find_stream_info(m_fmtctx, nullptr)) < 0) {
				av_log(nullptr, AV_LOG_ERROR, "Find stream info failed!%s\n", m_inputf.c_str());
				return false;
			}
			for (uint32_t i = 0; i < m_fmtctx->nb_streams; i++)
			{
				if ((int32_t)m_fmtctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
					m_av_fps = av_guess_frame_rate(m_fmtctx, m_fmtctx->streams[i], nullptr);
			}
				
			av_dump_format(m_fmtctx, 0, m_inputf.c_str(), !is_demuxer);
		}
	}

	return true;
}

void 
MediaDemuxer::closeMudemer(bool is_demuxer)
{
	if (is_demuxer)
	{
		if (nullptr != m_fmtctx)
			avformat_close_input(&m_fmtctx);
	}
}

bool 
MediaDemuxer::resetMudemer(bool is_demuxer)
{
	if (is_demuxer)
	{
		closeMudemer(is_demuxer);
		return opendMudemer(is_demuxer);
	}
	return false;
}
