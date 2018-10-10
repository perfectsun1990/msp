
#include "pubcore.hpp"
#include "enmuxer.hpp"

static int32_t g_enmuxer_ssidNo = 0;

std::shared_ptr<IEnmuxer> IEnmuxer::create(const char * output,
	std::shared_ptr<IEnmuxerObserver> observer, int32_t av_type) {
	return std::make_shared<MediaEnmuxer>(output, observer, av_type);
}

MediaEnmuxer::MediaEnmuxer(const char * output, std::shared_ptr<IEnmuxerObserver> observer, int32_t av_type)
	: m_observer(observer)
{
	SET_STATUS(m_status, E_INVALID);

	if (!IsStrEffect(output))	return;

	m_config = std::make_shared<MemxConfig>();
	m_config->urls = output;
	m_config->wrtimeout = 10;
	m_config->muxertype = av_type;
	m_ssidNo = g_enmuxer_ssidNo++;
	m_acache = std::make_shared<MPacket>();
	m_acache->ssid = m_ssidNo;
	m_acache->type = AVMEDIA_TYPE_AUDIO;
	SET_PROPERTY(m_acache->prop, P_PAUS);
	m_vcache = std::make_shared<MPacket>();
	m_vcache->ssid = m_ssidNo;
	m_vcache->type = AVMEDIA_TYPE_VIDEO;
	SET_PROPERTY(m_vcache->prop, P_PAUS);
	m_vcodec_par = avcodec_parameters_alloc();
	m_acodec_par = avcodec_parameters_alloc();
	m_vcodec_par->format = AV_PIX_FMT_NONE;
	m_acodec_par->format = AV_PIX_FMT_NONE;

	SET_STATUS(m_status, E_INITRES);
}

MediaEnmuxer::~MediaEnmuxer()
{
	stopd(true);
	avcodec_parameters_free(&m_vcodec_par);
	avcodec_parameters_free(&m_acodec_par);
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

int32_t MediaEnmuxer::Q_size(int32_t type)
{
	return ((type == AVMEDIA_TYPE_AUDIO) ?
		m_aenmuxer_Q.size() : m_venmuxer_Q.size());
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
MediaEnmuxer::start(void)
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);

	m_signal_quit = false;
	m_worker = std::thread([&]()
	{
		for (int64_t loop = 0; !m_signal_quit; ++loop, m_last_loop = av_gettime())
		{
			int32_t ret = 0;
			std::shared_ptr<MPacket> a_pkt = nullptr,v_pkt = nullptr, p_pkt = nullptr;
			// 1.pause to write  av packets.
			if (m_config->pauseflag) {
				SET_STATUS(m_status, E_PAUSING);
				if (!m_observer.expired()) {
					m_observer.lock()->onEnmuxerPackt(m_acache);
					m_observer.lock()->onEnmuxerPackt(m_vcache);
				}
				sleepMs(STANDARDTK);
				continue;
			}
			// 2.receive and mux av packets.
			switch (m_config->muxertype) {
			case AVMEDIA_TYPE_AUDIO:
				if (!m_aenmuxer_Q.try_peek(p_pkt)) {
					sleepMs(STANDARDTK);
					continue;
				}
				break;
			case AVMEDIA_TYPE_VIDEO:
				if (!m_venmuxer_Q.try_peek(p_pkt)) {
					sleepMs(STANDARDTK);
					continue;
				}
				break;
			default:
				if (!m_aenmuxer_Q.try_peek(a_pkt) || !m_venmuxer_Q.try_peek(v_pkt)) {
					sleepMs(STANDARDTK);
					continue;
				}
				p_pkt = (a_pkt->upts < v_pkt->upts) ? a_pkt : v_pkt;
				break;
			}
			// 3.check and reset av enmuxer.
			if (a_pkt && ( m_acodec_par->format	!= a_pkt->pars->format
				|| m_acodec_par->sample_rate != a_pkt->pars->sample_rate
				|| m_acodec_par->channels	 != a_pkt->pars->channels)) {
				if ((ret = avcodec_parameters_copy(m_acodec_par, a_pkt->pars)) < 0)
					err("avcodec_parameters_copy failed! ret=%s\n", err2str(ret));
				m_atime_base  = a_pkt->sttb;
				m_signal_rset = true;
			}
			if (v_pkt && ( m_vcodec_par->format != v_pkt->pars->format
				|| m_vcodec_par->width  != v_pkt->pars->width
				|| m_vcodec_par->height != v_pkt->pars->height)) {
				if (!(AV_PKT_FLAG_KEY & v_pkt->ppkt->flags)) {
					m_venmuxer_Q.popd(v_pkt);
					continue;
				}
				if ((ret = avcodec_parameters_copy(m_vcodec_par, v_pkt->pars)) < 0)
					err("avcodec_parameters_copy failed! ret=%s\n", err2str(ret));
				m_vtime_base  = v_pkt->sttb;
				m_signal_rset = true;
			}
			if (m_signal_rset) {
				if (!resetMudemuxer()) {
					SET_STATUS(m_status, E_STRTERR);
					for (ret = 0; ret < 300 && !m_signal_quit; ++ret)
						sleepMs(STANDARDTK);//3s.
					continue;
				}
				m_signal_rset = false;
				SET_STATUS(m_status, E_STARTED);
				war("Reset media Enmuxer!,has stream[A=%d,V=%d]\n", !!(int32_t)m_audio_stream, !!(int32_t)m_video_stream);
			}
			// 5.write audio or video packet.
			AVStream* p_stream = (AVMEDIA_TYPE_AUDIO == p_pkt->type) ? m_audio_stream : m_video_stream;
			av_packet_rescale_ts(p_pkt->ppkt, p_pkt->sttb, p_stream->time_base);
			dbg("p_pkt->ppkt.pts=%lf [%d,%d]\n", p_pkt->upts, p_pkt->sttb.den, p_stream->time_base.den);
			p_pkt->ppkt->stream_index = p_stream->index;
			p_pkt->ssid = m_ssidNo;
			if ((ret = av_interleaved_write_frame(m_fmtctx, p_pkt->ppkt)) < 0) {
				err("!!av_interleaved_write_frame failed! ret=%s\n", err2str(ret));
				sleepMs(STANDARDTK);
				continue;
			}			
			if (!m_observer.expired())
				m_observer.lock()->onEnmuxerPackt(p_pkt);
			// 6.deque &update execute status.
			if (AVMEDIA_TYPE_AUDIO == p_pkt->type) {
				m_aenmuxer_Q.popd(p_pkt);
				m_acache->upts = p_pkt->upts;
			}
			if (AVMEDIA_TYPE_VIDEO == p_pkt->type) {
				m_venmuxer_Q.popd(p_pkt);
				m_vcache->upts = p_pkt->upts;
			}
			SET_STATUS(m_status, ((m_status == E_STOPING) ? E_STOPING : E_RUNNING));
		}
		war("media enmuxer finished! m_signal_quit=%d\n", m_signal_quit);
	});
}

void
MediaEnmuxer::stopd(bool stop_quik)
{
	CHK_RETURN(E_STOPPED == status() || E_STOPING == status());
	SET_STATUS(m_status, E_STOPING);
	m_signal_quit = true;
	if (m_worker.joinable()) m_worker.join();
	closeMudemuxer();
	SET_STATUS(m_status, E_STOPPED);
}

void
MediaEnmuxer::pushPackt(std::shared_ptr<MPacket> av_pkt)
{
	if (!m_signal_quit) {
		if (AVMEDIA_TYPE_AUDIO == av_pkt->type) {
			if (!CHK_PROPERTY(av_pkt->prop, P_PAUS))
				m_aenmuxer_Q.push(av_pkt);
		}
		if (AVMEDIA_TYPE_VIDEO == av_pkt->type) {
			if (!CHK_PROPERTY(av_pkt->prop, P_PAUS))
				m_venmuxer_Q.push(av_pkt);
		}
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void
MediaEnmuxer::updateAttributes(void)
{
}


void
MediaEnmuxer::closeMudemuxer(bool is_demuxer)
{
	if (!is_demuxer) {
		if (m_fmtctx) {
			if (m_fmtctx->pb) {
				av_write_trailer(m_fmtctx);
				if (!(m_fmtctx->flags & AVFMT_NOFILE))
					avio_closep(&m_fmtctx->pb);
			}			
			avformat_free_context(m_fmtctx);
			m_fmtctx = nullptr;
			av_bsf_free(&bsf_ctx);
		}
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

	if (!is_demuxer)
	{// 2.Fill m_fmtctx and m_fmtctx->oformat<AVOutputFormat> etc.
		if (nullptr == m_fmtctx) {
			if (strstr(m_config->urls.c_str(), "rtmp:")) {
				msg("network....\n");
				avformat_alloc_output_context2(&m_fmtctx, nullptr,   "flv", m_config->urls.c_str());
			}else {// Base on urls to deduce the oformat.
				avformat_alloc_output_context2(&m_fmtctx, nullptr, nullptr, m_config->urls.c_str());
			}
			if (!m_fmtctx) {
				err("Could not create output context!\n");
				return false;
			}
			//m_format = m_fmtctx->oformat;
			if (AV_PIX_FMT_NONE != m_acodec_par->format) {// Add audio stream to m_fmtctx.				
				m_fmtctx->audio_codec_id = m_acodec_par->codec_id;
				m_fmtctx->oformat->audio_codec = m_acodec_par->codec_id;
				m_audio_stream = avformat_new_stream(m_fmtctx, avcodec_find_encoder(m_acodec_par->codec_id));
				if (!m_audio_stream) {
					err("Failed allocating output stream\n");
					return false;
				}
				if ((ret = avcodec_parameters_copy(m_audio_stream->codecpar, m_acodec_par)) < 0) {
					err("avcodec_parameters_copy failed! ret=%s\n", err2str(ret));
					return false;
				}
				m_audioindex = m_audio_stream->index;
				m_audio_stream->codecpar->codec_tag = 0;//没有补充信息，Additional information about the codec
// 				if (m_fmtctx->oformat->flags & AVFMT_GLOBALHEADER)
// 					m_audio_stream->codecpar->flags |= CODEC_FLAG_GLOBAL_HEADER;
			}
			if (AV_PIX_FMT_NONE != m_vcodec_par->format) {	// Add video stream to m_fmtctx.
				m_fmtctx->video_codec_id = m_vcodec_par->codec_id;
				m_fmtctx->oformat->video_codec = m_vcodec_par->codec_id;
				m_video_stream = avformat_new_stream(m_fmtctx, avcodec_find_encoder(m_vcodec_par->codec_id));
				if (!m_video_stream) {
					err("Failed allocating output stream\n");
					return false;
				}
				if ((ret = avcodec_parameters_copy(m_video_stream->codecpar, m_vcodec_par)) < 0) {
					err("avcodec_parameters_copy failed! ret=%s\n", err2str(ret));
					return false;
				}
				m_videoindex = m_video_stream->index;
				m_video_stream->codecpar->codec_tag = 0;//没有补充信息，Additional information about the codec
// 				if (m_fmtctx->oformat->flags & AVFMT_GLOBALHEADER)
// 					m_video_stream->codec->flags |= CODEC_FLAG_GLOBAL_HEADER;
			}
			if (!(m_fmtctx->oformat->flags & AVFMT_NOFILE)) {//open output local file if needed.
				if ((ret = avio_open(&m_fmtctx->pb, m_config->urls.c_str(), AVIO_FLAG_WRITE)) < 0) {
					err("Could not open output file '%s'", m_config->urls.c_str());
					return false;
				}
			}
			filter = av_bsf_get_by_name("aac_adtstoasc");
			int ret = av_bsf_alloc(filter, &bsf_ctx);
// 			if ((strstr(m_fmtctx->oformat->name, "flv") != NULL) ||
// 				(strstr(m_fmtctx->oformat->name, "mp4") != NULL) ||
// 				(strstr(m_fmtctx->oformat->name, "mov") != NULL) ||
// 				(strstr(m_fmtctx->oformat->name, "3gp") != NULL)){
// 				if (m_acodec_par->codec_id == AV_CODEC_ID_AAC)
// 				{
// 					vbsf_aac_adtstoasc = av_bitstream_filter_init("aac_adtstoasc");
// 				}
// 			}
		
			/* init muxer, write output file header */
			if ((ret = avformat_write_header(m_fmtctx, nullptr)) < 0) {
				err("Error occurred when opening output file\n");
				return false;
			}
			av_dump_format(m_fmtctx, 0, m_config->urls.c_str(), !is_demuxer);
		}
	}
	return true;
}

bool
MediaEnmuxer::resetMudemuxer(bool is_demuxer)
{
	closeMudemuxer(is_demuxer);
	return opendMudemuxer(is_demuxer);
}
