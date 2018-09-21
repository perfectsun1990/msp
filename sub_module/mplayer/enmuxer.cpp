
#include "pubcore.hpp"
#include "enmuxer.hpp"

static int32_t g_enmuxer_ssidNo = 0;

std::shared_ptr<IEnmuxer> IEnmuxer::create(const char * output,
	std::shared_ptr<IEnmuxerObserver> observer) {
	return std::make_shared<MediaEnmuxer>(output, observer);
}

MediaEnmuxer::MediaEnmuxer(const char * output, std::shared_ptr<IEnmuxerObserver> observer)
	: m_observer(observer)
{
	SET_STATUS(m_status, E_INVALID);

	if (!IsStrEffect(output))	return;

	m_config = std::make_shared<MemxConfig>();
	m_config->urls = output;
	m_config->rdtimeout = 10;

	m_ssidNo = g_enmuxer_ssidNo++;
	m_acache = std::make_shared<MPacket>();
	m_acache->ssid = m_ssidNo;
	m_acache->type = AVMEDIA_TYPE_AUDIO;
	SET_PROPERTY(m_acache->prop, P_PAUS);
	m_vcache = std::make_shared<MPacket>();
	m_vcache->ssid = m_ssidNo;
	m_vcache->type = AVMEDIA_TYPE_VIDEO;
	SET_PROPERTY(m_vcache->prop, P_PAUS);

	SET_STATUS(m_status, E_INITRES);
}

MediaEnmuxer::~MediaEnmuxer()
{
	stopd(true);
}

int32_t
MediaEnmuxer::ssidNo(void)
{
	return m_ssidNo;
}

void
MediaEnmuxer::update(void* config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config) {
		*m_config = *((MemxConfig*)config);
		updateAttributes();
	}
}

void
MediaEnmuxer::config(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config)
		*((MemxConfig*)config) = *m_config;
}

STATUS
MediaEnmuxer::status(void)
{
	return m_status;
}

double
MediaEnmuxer::durats(void)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	return m_config->mdmx_pars.duts_time;
}

void
MediaEnmuxer::pause(bool pauseflag)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	m_config->pauseflag = pauseflag;
	//msg("pause on %lf s \n", m_config->mdmx_pars.curr_time);
}

void
MediaEnmuxer::seekp(int64_t seektp)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (m_seek_done) {
		m_seek_done = false;
		m_config->seek_flag = AVSEEK_FLAG_BACKWARD;
		m_config->seek_time = seektp;
		msg("seek to %lld ms \n", m_config->seek_time / 1000);
	}
}

void
MediaEnmuxer::start(void)
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);

	m_signal_quit = false;
	m_worker = std::thread([&](void)
	{
		for (int64_t loop = 0; !m_signal_quit; ++loop, m_last_loop = av_gettime())
		{
			// Send pause packets to observer.
			if (m_config->pauseflag) {
				SET_STATUS(m_status, E_PAUSING);
				if (!m_observer.expired()) {
					m_observer.lock()->onEnmuxerPackt(m_acache);
					m_observer.lock()->onEnmuxerPackt(m_vcache);
				}
				sleepMs(STANDARDTK);
				continue;
			}
			// General new packets for demuxer.
			int32_t	ret = -1;
			std::shared_ptr<MPacket> av_pkt = std::make_shared<MPacket>();
			if (!m_fmtctx || m_config->urls.compare(m_fmtctx->filename))
				m_signal_rset = true;
			if (m_signal_rset) {
				if (!resetMudemuxer(true)) {
					SET_STATUS(m_status, E_STRTERR);
					for (ret = 0; ret < 300 && !m_signal_quit; ++ret)
						sleepMs(STANDARDTK);//3s.
					continue;
				}
				SET_PROPERTY(av_pkt->prop, P_BEGP);
				m_signal_rset = false;
				SET_STATUS(m_status, E_STARTED);
			}
			// Seek and read frame from source.
			if (nullptr == av_pkt->ppkt || !handleSeekAction()) {
				SET_STATUS(m_status, E_RUNNERR);
				sleepMs(STANDARDTK);
				continue;
			}
			if ((ret = av_read_frame(m_fmtctx, av_pkt->ppkt)) < 0) {
				SET_STATUS(m_status, E_RUNNERR);
				if (ret != AVERROR_EOF)
					err("av_read_frame failed! ret=%s\n", averr2str(ret));
				m_signal_rset = IsNetStream(m_fmtctx->filename);
				sleepMs(STANDARDTK);
				continue;
			}
			av_pkt->ssid = m_ssidNo;
			av_pkt->type = m_fmtctx->streams[av_pkt->ppkt->stream_index]->codecpar->codec_type;
			av_pkt->sttb = m_fmtctx->streams[av_pkt->ppkt->stream_index]->time_base;
			av_pkt->upts = av_pkt->ppkt->pts * av_q2d(av_pkt->sttb);
			av_pkt->ufps = (av_pkt->type == AVMEDIA_TYPE_VIDEO) ? m_av_fps : av_pkt->ufps;
			if ((ret = avcodec_parameters_copy(av_pkt->pars, m_fmtctx->streams[av_pkt->ppkt->stream_index]->codecpar)) < 0) {
				SET_STATUS(m_status, E_RUNNERR);
				err("avcodec_parameters_copy failed! ret=%s\n", averr2str(ret));
				continue;
			}
			if (av_pkt->type == AVMEDIA_TYPE_AUDIO) {// update audio cache and status.
				if (!m_seek_apkt) {
					SET_PROPERTY(av_pkt->prop, P_SEEK);
					m_seek_apkt = true;
				}
				m_acache->upts = av_pkt->upts;
			}
			if (av_pkt->type == AVMEDIA_TYPE_VIDEO) {// update video cache and status.
				if (!m_seek_vpkt) {
					SET_PROPERTY(av_pkt->prop, P_SEEK);
					m_seek_vpkt = true;
				}
				m_vcache->upts = av_pkt->upts;
			}
			// Send media packets to observer.
			if (!m_observer.expired())
				m_observer.lock()->onEnmuxerPackt(av_pkt);
			// Update readonly cfg attributes.
			m_config->mdmx_pars.curr_time = av_pkt->upts;
			SET_STATUS(m_status, ((m_status == E_STOPING) ? E_STOPING : E_RUNNING));
		}
		war("Media Demuxer finished! m_signal_quit=%d\n", m_signal_quit);
	});
}

void
MediaEnmuxer::stopd(bool stop_quik)
{
	CHK_RETURN(E_STOPPED == status() || E_STOPING == status());
	SET_STATUS(m_status, E_STOPING);
	m_signal_quit = true;
	if (m_worker.joinable()) m_worker.join();
	closeMudemuxer(true);
	SET_STATUS(m_status, E_STOPPED);
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void
MediaEnmuxer::updateAttributes(void)
{
}

bool
MediaEnmuxer::handleSeekAction(void)
{
	if (!m_seek_done) {
		MemxConfig cfg;
		config(&cfg);
		cfg.seek_time = (cfg.seek_time > m_fmtctx->duration) ? m_fmtctx->duration : cfg.seek_time;
		cfg.seek_time = (cfg.seek_time < 0) ? 0 : cfg.seek_time;
		if (av_seek_frame(m_fmtctx, -1, cfg.seek_time + m_fmtctx->start_time, cfg.seek_flag) < 0) {
			err("Can't seek stream: %lld", cfg.seek_time);
			return false;
		}
		m_seek_apkt = false;
		m_seek_vpkt = false;
		m_seek_done = true;
		update(&cfg);
	}
	return true;
}

void
MediaEnmuxer::closeMudemuxer(bool is_demuxer)
{
	if (is_demuxer) {
		if (nullptr != m_fmtctx)
			avformat_close_input(&m_fmtctx);
		m_signal_rset = true;
	}
}

bool
MediaEnmuxer::opendMudemuxer(bool is_demuxer)
{
	int32_t ret = -1;
	AVDictionary *opts = nullptr;

	// 1.Register all codec,foramt,filter,device,protocol...
	av_register_all();
	avdevice_register_all();
	avfilter_register_all();
	avformat_network_init();

	if (is_demuxer)
	{// 2.Fill m_fmtctx and m_fmtctx->iformat<AVInputFormat> etc.
		if (nullptr == m_fmtctx) {
			// Policy for connect timeout,protcol etc...		
			if (IsNetStream(m_config->urls.c_str())) {
				av_dict_set(&opts, "rtsp_transport", "tcp", 0);	//rtsp udp maybe blurred.
				av_dict_set(&opts, "rtmp_transport", "tcp", 0); //rtmp default.
				av_dict_set(&opts, "fflags", "nobuffer", 0);	//specify don't cache.buffersize
				av_dict_set(&opts, "stimeout", "3000000", 0);	//connect delay 3s.
			}
			// Policy for resove av_read_frame block,play low delay.
			m_fmtctx = avformat_alloc_context();
			m_fmtctx->probesize = 128 * 1024;					//def:5M,can be set smaller.
			m_fmtctx->max_delay = 100 * 1000;					//m_fmtctx->flush_packets = 1;
			m_fmtctx->max_analyze_duration = 1000000;			//rapid load, max_duration:2s
			m_fmtctx->interrupt_callback.opaque = this;
			m_fmtctx->interrupt_callback.callback = [](void* ctx)->int32_t {
				MediaEnmuxer* pthis = (MediaEnmuxer*)ctx;		//break when read frame block.
				if ((av_gettime() - pthis->m_last_loop) > pthis->m_config->rdtimeout * 1000 * 1000) {
					war("--->>> Hi! stream is diconnected!\n");
					pthis->m_last_loop = av_gettime();
					return AVERROR_EOF;
				}
				return 0;
			};
			if ((ret = avformat_open_input(&m_fmtctx, m_config->urls.c_str(), NULL, &opts)) < 0) {
				err("Open  urls=%s failed! Err:%s\n", m_config->urls.c_str(), averr2str(ret));
				return false;
			}
			if ((ret = avformat_find_stream_info(m_fmtctx, nullptr)) < 0) {
				err("Purse urls=%s failed! Err:%s\n", m_config->urls.c_str(), averr2str(ret));
				return false;
			}
			for (uint32_t i = 0; i < m_fmtctx->nb_streams; i++) {
				int32_t codec_type = (int32_t)m_fmtctx->streams[i]->codecpar->codec_type;
				if (codec_type == AVMEDIA_TYPE_VIDEO)
					m_av_fps = av_guess_frame_rate(m_fmtctx, m_fmtctx->streams[i], nullptr);
				m_config->mdmx_pars.strm_info.push_back(std::tuple<int32_t, void*>(codec_type, m_fmtctx->streams[i]));
			}
			m_config->mdmx_pars.duts_time = (double)m_fmtctx->duration / AV_TIME_BASE;
			av_dump_format(m_fmtctx, 0, m_config->urls.c_str(), !is_demuxer);
		}
	}
	return true;
}

bool
MediaEnmuxer::resetMudemuxer(bool is_demuxer)
{
	if (is_demuxer) {
		closeMudemuxer(is_demuxer);
		return opendMudemuxer(is_demuxer);
	}
	return false;
}
