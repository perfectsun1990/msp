
#include "pubcore.hpp"
#include "encoder.hpp"

// typedef enum AVPixelFormat(*AVGetFormatCb)(
// 	struct AVCodecContext *s, const enum AVPixelFormat * fmt);

static AVHWAccel*
avfind_hwaccel_codec(enum AVCodecID codec_id, int32_t pix_fmt)
{
	AVHWAccel *hwaccel = NULL;
	while ((hwaccel = av_hwaccel_next(hwaccel))) {
		if (hwaccel->id == codec_id)
			break;
	}
	return hwaccel;
}

static enum AVPixelFormat 
get_hwaccel_format(struct AVCodecContext *s,
	const enum AVPixelFormat * fmt)
{
	(void)s;
	(void)fmt;
	// for now force output to common denominator
	return AV_PIX_FMT_YUV420P;
}

static void
avcodec_setlimits(AVCodecContext* codec_ctx)
{
	if (codec_ctx->codec_id == AV_CODEC_ID_PNG
		|| codec_ctx->codec_id == AV_CODEC_ID_TIFF
		|| codec_ctx->codec_id == AV_CODEC_ID_JPEG2000
		|| codec_ctx->codec_id == AV_CODEC_ID_WEBP)
		codec_ctx->thread_count = 1;
}

static int32_t g_aencoder_ssidNo = 10000;
static int32_t g_vencoder_ssidNo = 20000;

std::shared_ptr<IEncoder> IEncoderFactory::createAudioEncoder(std::shared_ptr<IEncoderObserver> observer) {
	return std::make_shared<AudioEncoder>(observer);
}

AudioEncoder::AudioEncoder(std::shared_ptr<IEncoderObserver> observer) :
	m_observer(observer)
{
	SET_STATUS(m_status, E_INVALID);

	m_config = std::make_shared<AencConfig>();
	m_ssidNo = g_aencoder_ssidNo++;
	m_acache = std::make_shared<MPacket>();
	m_acache->ssid = m_ssidNo;
	m_acache->type = AVMEDIA_TYPE_AUDIO;
	SET_PROPERTY(m_acache->prop, P_PAUS);
	m_codec_par = avcodec_parameters_alloc();

	SET_STATUS(m_status, E_INITRES);
}

AudioEncoder::~AudioEncoder()
{
	stopd(true);
	avcodec_parameters_free(&m_codec_par);
}

void
AudioEncoder::update(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config) {
		*m_config = *((AencConfig*)config);
		updateAttributes();
	}
}

void
AudioEncoder::config(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config)
		*((AencConfig*)config) = *m_config;
}

int32_t 
AudioEncoder::ssidNo(void)
{
	return 	m_ssidNo;
}

STATUS
AudioEncoder::status(void)
{
	return m_status;
}

int32_t
AudioEncoder::Q_size(void)
{
	return m_encoder_Q.size();
}

void
AudioEncoder::pause(bool pauseflag)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	m_config->pauseflag = pauseflag;
}

void
AudioEncoder::start(void)
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);

	m_signal_quit = false;
	m_worker = std::thread([&]()
	{
		for (int64_t loop=0,i=0; !m_signal_quit; ++loop, m_last_loop = av_gettime())
		{
			int32_t ret = 0;
			std::shared_ptr<MRframe> av_frm = nullptr;
			// 1.receive and dec MPacket.			
			if (!m_encoder_Q.try_peek(av_frm)) {
				sleepMs(STANDARDTK);
				continue;
			}
			// 2.check and reset encoder.
			if (!m_codec_par
				|| m_codec_par->format		!= av_frm->pars->format
				|| m_codec_par->sample_rate != av_frm->pars->sample_rate
				|| m_codec_par->channels	!= av_frm->pars->channels) {
				if ((ret = avcodec_parameters_copy(m_codec_par, av_frm->pars)) < 0) {
					err("avcodec_parameters_copy failed! ret=%d\n", ret);
					break;
				}
				m_signal_rset = true;
			}
			if (m_signal_rset) {
				av_frm->pfrm->key_frame = 1;//unuse
				m_framerate  = av_frm->ufps;//unuse
				if (!resetCodecer()) {
					SET_STATUS(m_status, E_STRTERR);
					for (ret = 0; ret < 300 && !m_signal_quit; ++ret)
						sleepMs(STANDARDTK);//3s.
					continue;
				}
				m_signal_rset = false;
				SET_STATUS(m_status, E_STARTED);
				war("Reset audio Encoder!\n");
			}

			if (m_config->pauseflag) {
				SET_STATUS(m_status, E_PAUSING);
				if (!m_observer.expired())
					m_observer.lock()->onEncoderPackt(m_acache);
				sleepMs(STANDARDTK);
				continue;
			}
			// 3. set AVFrame attributes.
			PUR_PROPERTY(av_frm->prop);
			av_frm->pfrm->pict_type = AV_PICTURE_TYPE_NONE;
			av_frm->pfrm->pts = (i+=av_frm->pfrm->nb_samples);
			// 4. send packet to decoder.
			if ((ret = avcodec_send_frame(m_codec_ctx, av_frm->pfrm)) < 0) {
				SET_STATUS(m_status, E_RUNNERR);
				if (ret != AVERROR_EOF) {
					err("avcodec_send_frame failed! ret=%s\n", err2str(ret));
					m_encoder_Q.popd(av_frm);
					m_signal_rset = true;
				}
				sleepMs(STANDARDTK);
				continue;
			}
			// 5.receive frame from decoder.
			while (true) {
				std::shared_ptr<MPacket> av_pkt = std::make_shared<MPacket>();
				if ((ret = avcodec_receive_packet(m_codec_ctx, av_pkt->ppkt)) < 0) {
					if (ret != -(EAGAIN) && ret != AVERROR_EOF)
						err("avcodec_receive_packet failed! ret=%s\n", err2str(ret));
					break;
				}
				// scale AVPacket timebase. <codec->stream: eg.1/44100->1/44100>
				AVRational dst_timebase = { 1, m_codec_par->sample_rate };//计算实际的时间戳33|23ms
				av_packet_rescale_ts(av_pkt->ppkt, m_codec_ctx->time_base, dst_timebase);
				av_pkt->ssid = m_ssidNo;
				av_pkt->type = av_frm->type;
				av_pkt->prop = av_frm->prop;
				av_pkt->sttb = dst_timebase; // Node: use stream->timebase.
				av_pkt->upts = av_pkt->ppkt->pts * av_q2d(av_pkt->sttb);
				av_pkt->ufps = (av_pkt->type == AVMEDIA_TYPE_VIDEO) ? m_codec_ctx->framerate : av_pkt->ufps;
				if ((ret = avcodec_parameters_copy(av_pkt->pars, av_frm->pars)) < 0) {
					err("avcodec_parameters_copy failed! ret=%d\n", ret);
					break;
				}				
				m_acache->upts = av_pkt->upts;
				if (!m_observer.expired() && av_pkt->type == AVMEDIA_TYPE_AUDIO)
					m_observer.lock()->onEncoderPackt(av_pkt);
			}
			// 6.deque &update execute status.
			SET_STATUS(m_status, ((m_status == E_STOPING) ? E_STOPING : E_RUNNING));
			m_encoder_Q.popd(av_frm);

		}
		war("Audio encoder finished! m_signal_quit=%d\n", m_signal_quit);
	});
}

void
AudioEncoder::stopd(bool stop_quik)
{
	CHK_RETURN(E_STOPPED == status() || E_STOPING == status());
	SET_STATUS(m_status, E_STOPING);
	m_signal_quit = true;
	if (m_worker.joinable()) m_worker.join();
	closeCodecer(true);
	clearCodec_Q(true);
	SET_STATUS(m_status, E_STOPPED);
}

void 
AudioEncoder::pushFrame(std::shared_ptr<MRframe> av_frm)
{
	if (!m_signal_quit) {
		if (!CHK_PROPERTY(av_frm->prop, P_PAUS))
			m_encoder_Q.push(av_frm);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void 
AudioEncoder::updateAttributes(void)
{
}

void 
AudioEncoder::clearCodec_Q(bool is_decoder)
{
	if (is_decoder)
		m_encoder_Q.clear();
}

void
AudioEncoder::closeCodecer(bool is_decoder)
{
	avcodec_free_context(&m_codec_ctx);
	m_codec = nullptr;
	m_codec_ctx = nullptr;
	m_signal_rset = true;
}

bool 
AudioEncoder::opendCodecer(bool is_decoder)
{
	int32_t ret = 0;
	AVDictionary *opts = nullptr;

	av_register_all();

	if (!is_decoder)
	{
		AVDictionary *opts = nullptr;
		if (m_config->avhwaccel) {
			AVHWAccel *hwaccel = avfind_hwaccel_codec(m_codec_par->codec_id, m_codec_par->format);
			if (hwaccel) {
				msg("using hwaccel->name=%s\n", hwaccel->name);
				if (!(m_codec = avcodec_find_encoder_by_name(hwaccel->name)))
					err("Find encoder[H] for [%d] failed! Err:%s\n", m_codec_par->codec_id, err2str(ret));
			}
		}
		if (nullptr == m_codec) {
			if (!(m_codec = avcodec_find_encoder(m_codec_par->codec_id))) {
				err("Find encoder[S] for [%d] failed! Err:%s\n", m_codec_par->codec_id, err2str(ret));
				return false;
			}
		}
		if (!(m_codec_ctx = avcodec_alloc_context3(m_codec))) {
			err("avcodec_alloc_context3 failed! Err:%s\n", err2str(ret));
			return false;
		}
		if (AVMEDIA_TYPE_AUDIO == m_codec_par->codec_type) {
			m_codec_ctx->codec_id		= m_codec_par->codec_id;
			m_codec_ctx->frame_size		= m_codec_par->frame_size;	//default:1024spf
			m_codec_ctx->sample_rate	= m_codec_par->sample_rate;
			m_codec_ctx->channel_layout = m_codec_par->channel_layout;
			m_codec_ctx->channels		= av_get_channel_layout_nb_channels(m_codec_ctx->channel_layout);
			m_codec_ctx->sample_fmt		=(AVSampleFormat)m_codec_par->format;
			m_codec_ctx->time_base.num	= 1;
			m_codec_ctx->time_base.den	= m_codec_par->sample_rate;
			m_codec_ctx->profile		= m_codec_par->profile;// FF_PROFILE_AAC_LOW;
			m_codec_ctx->bit_rate		= m_codec_par->bit_rate;
			/* Third parameter can be used to pass settings to encoder */
			if (m_codec_ctx->codec_id == AV_CODEC_ID_AAC) {
				av_dict_set(&opts, "preset", "superfast", 0);
				av_dict_set(&opts, "tune", "zerolatency", 0);
			}
		}
		// Note: if you need sps/pps per frame,forbiden it.
		//m_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		if ((ret = avcodec_open2(m_codec_ctx, m_codec, &opts)) < 0) {
			err("avcodec_open2 failed! Err:%s\n", err2str(ret));
			return false;
		}
	}
	return true;
}

bool 
AudioEncoder::resetCodecer(bool is_decoder)
{
	closeCodecer(is_decoder);
	return opendCodecer(is_decoder);
}

std::shared_ptr<IEncoder> IEncoderFactory::createVideoEncoder(
	std::shared_ptr<IEncoderObserver> observer) {
	return std::make_shared<VideoEncoder>(observer);
}

VideoEncoder::VideoEncoder(std::shared_ptr<IEncoderObserver> observer):
	m_observer(observer)
{
	SET_STATUS(m_status, E_INVALID);

	m_config = std::make_shared<VencConfig>();
	m_ssidNo = g_vencoder_ssidNo++;
	m_vcache = std::make_shared<MPacket>();
	m_vcache->ssid = m_ssidNo;
	m_vcache->type = AVMEDIA_TYPE_VIDEO;
	SET_PROPERTY(m_vcache->prop, P_PAUS);
	m_codec_par = avcodec_parameters_alloc();

	SET_STATUS(m_status, E_INITRES);
}

VideoEncoder::~VideoEncoder()
{
	stopd(true);
	avcodec_parameters_free(&m_codec_par);
}

void
VideoEncoder::update(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config) {
		*m_config = *((VencConfig*)config);
		updateAttributes();
	}
}

void
VideoEncoder::config(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config)
		*((VencConfig*)config) = *m_config;
}

int32_t 
VideoEncoder::ssidNo(void)
{
	return 	m_ssidNo;
}

STATUS
VideoEncoder::status(void)
{
	return m_status;
}

int32_t
VideoEncoder::Q_size(void)
{
	return m_encoder_Q.size();
}

void
VideoEncoder::pause(bool pauseflag)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	m_config->pauseflag = pauseflag;
}

void 
VideoEncoder::start(void)
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);

	m_signal_quit = false;
	m_worker = std::thread([&]()
	{
		for (int64_t loop=0,i=0; !m_signal_quit; ++loop, m_last_loop = av_gettime())
		{
			int32_t ret = 0;
			std::shared_ptr<MRframe> av_frm = nullptr;
			// 1.receive and dec MPacket.			
			if (!m_encoder_Q.try_peek(av_frm)) {
				sleepMs(STANDARDTK);
				continue;
			}
			// 2.check and reset encoder.
			if (!m_codec_par
				|| m_codec_par->format != av_frm->pars->format
				|| m_codec_par->width  != av_frm->pars->width
				|| m_codec_par->height != av_frm->pars->height) {
				if ((ret = avcodec_parameters_copy(m_codec_par, av_frm->pars)) < 0) {
					SET_STATUS(m_status, E_RUNNERR);
					err("avcodec_parameters_copy failed! ret=%s\n", err2str(ret));
					break;
				}
				m_signal_rset = true;
			}
			if (m_signal_rset) {
				av_frm->pfrm->key_frame = 1;
				m_framerate  = av_frm->ufps;
				if (!resetCodecer()) {
					SET_STATUS(m_status, E_STRTERR);
					for (ret = 0; ret < 300 && !m_signal_quit; ++ret)
						sleepMs(STANDARDTK);//3s.
					continue;
				}
				m_signal_rset = false;
				SET_STATUS(m_status, E_STARTED);
				war("Reset video Encoder!\n");
			}
			if (m_config->pauseflag) {
				SET_STATUS(m_status, E_PAUSING);
				if (!m_observer.expired())
					m_observer.lock()->onEncoderPackt(m_vcache);
				sleepMs(STANDARDTK);
				continue;
			}
			// 3. set AVFrame attributes.
			PUR_PROPERTY(av_frm->prop);
			av_frm->pfrm->pict_type = AV_PICTURE_TYPE_NONE;
			av_frm->pfrm->pts = i++;
			// 4. send frames to encoder.
			if ((ret = avcodec_send_frame(m_codec_ctx, av_frm->pfrm)) < 0) {
				SET_STATUS(m_status, E_RUNNERR);
				if (ret != AVERROR_EOF) {
					err("avcodec_send_frame failed! ret=%s\n", err2str(ret));
					m_encoder_Q.popd(av_frm);
					m_signal_rset = true;
				}
				sleepMs(STANDARDTK);
				continue;
			}
			// 5.recv frame from decoder.
			while (true)
			{
				std::shared_ptr<MPacket> av_pkt = std::make_shared<MPacket>();
				if ((ret = avcodec_receive_packet(m_codec_ctx, av_pkt->ppkt)) < 0) {
					if (ret != -(EAGAIN) && ret != AVERROR_EOF)
						err("Encoding failed! ret=%s\n", err2str(ret));
					break;
				}
				// scale AVPacket timebase. <codec->stream: eg.1/25->1/90000|1/1000>
				AVRational dst_timebase = { 1, 90000 };//const stream timebase.
				av_packet_rescale_ts(av_pkt->ppkt, m_codec_ctx->time_base, dst_timebase);
				av_pkt->ssid = m_ssidNo;
				av_pkt->type = av_frm->type;
				av_pkt->prop = av_frm->prop;
				av_pkt->sttb = dst_timebase;
				av_pkt->upts = av_pkt->ppkt->pts* av_q2d(av_pkt->sttb); 
				av_pkt->ufps = (av_pkt->type == AVMEDIA_TYPE_VIDEO) ? m_codec_ctx->framerate : av_pkt->ufps;
				if ((ret = avcodec_parameters_copy(av_pkt->pars, av_frm->pars)) < 0) {
					SET_STATUS(m_status, E_RUNNERR);
					err("avcodec_parameters_copy failed! ret=%s\n", err2str(ret));
					break;
				}
				m_vcache->upts = av_pkt->upts;
				if (!m_observer.expired() && av_frm->type == AVMEDIA_TYPE_VIDEO)
					m_observer.lock()->onEncoderPackt(av_pkt);
			}
			// 6.deque &update execute status.
			SET_STATUS(m_status, ((m_status == E_STOPING) ? E_STOPING : E_RUNNING));
			m_encoder_Q.popd(av_frm);
		}
		war("Video encoder finished! m_signal_quit=%d\n", m_signal_quit);
	});
}

void
VideoEncoder::stopd(bool stop_quik)
{
	CHK_RETURN(E_STOPPED == status() || E_STOPING == status());
	SET_STATUS(m_status, E_STOPING);
	m_signal_quit = true;
	if (m_worker.joinable()) m_worker.join();
	closeCodecer(true);
	clearCodec_Q(true);
	SET_STATUS(m_status, E_STOPPED);
}

void
VideoEncoder::pushFrame(std::shared_ptr<MRframe> av_frm)
{
	if (!m_signal_quit) {
		if (!CHK_PROPERTY(av_frm->prop, P_PAUS))
			m_encoder_Q.push(av_frm);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void
VideoEncoder::updateAttributes(void)
{
}

void
VideoEncoder::clearCodec_Q(bool is_decoder)
{
	if (is_decoder)
		m_encoder_Q.clear();
}

void
VideoEncoder::closeCodecer(bool is_decoder)
{
	avcodec_free_context(&m_codec_ctx);
	m_codec = nullptr;
	m_codec_ctx = nullptr;
	m_signal_rset = true;
}

bool
VideoEncoder::opendCodecer(bool is_decoder)
{
	int32_t ret = 0;
	AVDictionary *opts = nullptr;

	av_register_all();

	if (!is_decoder)
	{
		if (m_config->avhwaccel) {
			AVHWAccel *hwaccel = avfind_hwaccel_codec(m_codec_par->codec_id, m_codec_par->format);
			if (hwaccel) {
				msg("using hwaccel->name=%s\n", hwaccel->name);
				if (!(m_codec = avcodec_find_encoder_by_name(hwaccel->name)))
					err("Find encoder[H] for [%d] failed! Err:%s\n", m_codec_par->codec_id, err2str(ret));
			}
		}
		if (nullptr == m_codec) {
			if (!(m_codec = avcodec_find_encoder(m_codec_par->codec_id))) {
				err("Find encoder[S] for [%d] failed! Err:%s\n", m_codec_par->codec_id, err2str(ret));
				return false;
			}
		}
		if (!(m_codec_ctx = avcodec_alloc_context3(m_codec))) {
			err("avcodec_alloc_context3 failed! Err:%s\n", err2str(ret));
			return false;
		}
		if (AVMEDIA_TYPE_VIDEO == m_codec_par->codec_type) {
			m_codec_ctx->codec_id		= m_codec_par->codec_id;
			m_codec_ctx->pix_fmt		= (AVPixelFormat)m_codec_par->format;
			m_codec_ctx->bit_rate		= m_codec_par->bit_rate;
			m_codec_ctx->framerate		= m_framerate;//must be correct.
			m_codec_ctx->time_base.num	= m_codec_ctx->framerate.den;
			m_codec_ctx->time_base.den	= m_codec_ctx->framerate.num;
			m_codec_ctx->width			= m_codec_par->width;
			m_codec_ctx->height			= m_codec_par->height;
			m_codec_ctx->profile		= m_codec_par->profile;
			m_codec_ctx->level			= m_codec_par->level;
			m_codec_ctx->qmin			= 10;
			m_codec_ctx->qmax			= 30;//[51]more bigger more bad quality.
			m_codec_ctx->qcompress		= 0.6f;//quality of compress.
			m_codec_ctx->gop_size		= 12;//or 15.IPBBBPBBBPBBBI.
			m_codec_ctx->max_b_frames	= 0; //realtime set to 0.
 			m_codec_ctx->refs			= 3; //more bigger more slow.
			/* Third parameter can be used to pass settings to encoder */
			if (m_codec_ctx->codec_id == AV_CODEC_ID_H264) {
				av_dict_set(&opts, "preset",   "superfast", 0);
				av_dict_set(&opts, "tune",	 "zerolatency", 0);
				//av_dict_set(&opts, "profile",		"main", 0);
				//av_opt_set(m_codec_ctx->priv_data, "crf", "51", AV_OPT_SEARCH_CHILDREN);
			}
			if (m_codec_ctx->codec_id == AV_CODEC_ID_H265) {
				av_dict_set(&opts, "x265-params", "qp=20", 0);
				av_dict_set(&opts, "preset",  "ultrafast", 0);
				av_dict_set(&opts, "tune", "zero-latency", 0);
				//av_dict_set(&opts, "profile", "main", 0);
			}
		}
		// Note: if you need sps/pps per frame,forbiden it.
		//m_codec_ctx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
		if ((ret = avcodec_open2(m_codec_ctx, m_codec, &opts)) < 0) {
			err("avcodec_open2 failed! Err:%s\n", err2str(ret));
			return false;
		}
	}
	return true;
}

bool
VideoEncoder::resetCodecer(bool is_decoder)
{
	closeCodecer(is_decoder);
	return opendCodecer(is_decoder);
}

