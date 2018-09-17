
#include "pubcore.hpp"
#include "decoder.hpp"

typedef enum AVPixelFormat(*AVGetFormatCb)(
	struct AVCodecContext *s, const enum AVPixelFormat * fmt);

AVHWAccel *avfind_hwaccel_codec(enum AVCodecID codec_id,int32_t pix_fmt)
{
	AVHWAccel *hwaccel = NULL;
	while ((hwaccel = av_hwaccel_next(hwaccel))) {
		if (hwaccel->id == codec_id)
			break;
	}
	return hwaccel;
}

enum AVPixelFormat get_hwaccel_format(struct AVCodecContext *s,
	const enum AVPixelFormat * fmt)
{
	(void)s;
	(void)fmt;
	// for now force output to common denominator
	return AV_PIX_FMT_YUV420P;
}

std::shared_ptr<IDecoder> IDecoderFactory::createAudioDecoder(std::shared_ptr<IDecoderObserver> observer)
{
	return std::make_shared<AudioDecoder>(observer);
}

AudioDecoder::AudioDecoder(std::shared_ptr<IDecoderObserver> observer):
	m_observer(observer)
{
	SET_STATUS(m_status, E_INVALID);

	m_config = std::make_shared<AdecConfig>();
	m_codec_par = avcodec_parameters_alloc();
	
	m_acache = std::make_shared<MRframe>();
	m_acache->type = AVMEDIA_TYPE_AUDIO;
	SET_PROPERTY(m_acache->prop, P_PAUS);
	
	SET_STATUS(m_status, E_INITRES);
}

AudioDecoder::~AudioDecoder()
{
	if (E_STOPPED != status())
		stopd(true);
	avcodec_parameters_free(&m_codec_par);
	SET_STATUS(m_status, E_INVALID);
}

void AudioDecoder::start(void)
{
	if (E_STARTED == status() || E_STRTING == status())
		return;
	SET_STATUS(m_status, E_STRTING);
	
	m_signal_quit = false;
	m_worker = std::thread([&]()
	{
		while (!m_signal_quit)
		{
			int32_t ret = 0;
			std::shared_ptr<MPacket> av_pkt = nullptr;
			// 1.receive and dec MPacket.			
			if (!m_decoder_Q.try_peek(av_pkt))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}

			// 2.check and reset decoder.
			if (  !m_codec_par
				|| m_codec_par->format		!= av_pkt->pars->format
				|| m_codec_par->sample_rate != av_pkt->pars->sample_rate
				|| m_codec_par->channels	!= av_pkt->pars->channels )
			{
				if ((ret = avcodec_parameters_copy(m_codec_par, av_pkt->pars)) < 0) 
				{
					err("avcodec_parameters_copy failed! ret=%d\n", ret);
					break;
				}
				m_signal_rset = true;
			}

			if (m_signal_rset)
			{
				m_framerate = av_pkt->ufps;
				if (!resetCodecer(true))
					break;
				m_signal_rset = false;
				SET_STATUS(m_status, E_STARTED);
				war("Reset audio Codecer!\n");
			}
			
			if ( m_config->pauseflag)
			{
				if (!m_observer.expired())
					m_observer.lock()->onMFrm(m_acache);
				av_usleep(10 * 1000);
				continue;
			}

			// 3.scale AVPacket timebase. <stream->codec: eg.1/25 1/44100>
			av_packet_rescale_ts(av_pkt->ppkt, av_pkt->sttb, m_codec_ctx->time_base);

			// 4. send packet to decoder.
			if ((ret = avcodec_send_packet(m_codec_ctx, av_pkt->ppkt)) < 0)
			{
				if (ret != AVERROR_EOF) {
					err("avcodec_send_packet failed! ret=%d\n", ret);
					break;
				}
				av_usleep(10 * 1000);
				continue;
			}
			
			// 5.receive frame from decoder.
			while (true)
			{
				std::shared_ptr<MRframe> av_frm = std::make_shared<MRframe>();

				if ((ret = avcodec_receive_frame(m_codec_ctx, av_frm->pfrm))< 0)
				{
					if (ret != -(EAGAIN) && ret != AVERROR_EOF)
						err("Decoding failed!ret=%d\n", ret);
					break;
				}
				
				// 6.打包发送到渲染器.				
				av_frm->type = av_pkt->type;
				av_frm->prop = av_pkt->prop;
				av_frm->sttb = m_codec_ctx->time_base;// av_pkt->sttb;
				av_frm->upts = av_frm->pfrm->pts* av_q2d(av_frm->sttb); //这样对于B帧是不对的。av_pkt->upts;
				if ((ret = avcodec_parameters_copy(av_frm->pars, av_pkt->pars)) < 0) {
					err("avcodec_parameters_copy failed! ret=%d\n", ret);
					break;
				}
				av_frm->pfrm->pts = av_frm->pfrm->best_effort_timestamp;
				m_acache->upts = av_frm->upts;
				if (!m_observer.expired() && av_frm->type == AVMEDIA_TYPE_AUDIO)
					m_observer.lock()->onMFrm(av_frm);
			}
			m_decoder_Q.popd(av_pkt);
		}
		war("Audio decoder finished! m_signal_quit=%d\n", m_signal_quit);
	});
}

void AudioDecoder::stopd(bool stop_quik)
{
	if (E_STOPPED == status() || E_STOPING == status())
		return;
	SET_STATUS(m_status, E_STOPING);
	m_signal_quit = true;
	if (m_worker.joinable()) m_worker.join();
	closeCodecer(true);
	clearCodec_Q(true);
	SET_STATUS(m_status, E_STOPPED);
}

void AudioDecoder::pause(bool pauseflag)
{
	VdecConfig cfg;
	config(static_cast<void*>(&cfg));
	cfg.pauseflag = pauseflag;
	update(static_cast<void*>(&cfg));
}

void AudioDecoder::update(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	AdecConfig* p_cfg = (AdecConfig*)config;
	if (nullptr != config)
	{
		*m_config = *p_cfg;
		updateAttributes();
	}
}

STATUS AudioDecoder::status(void)
{
	return m_status;
}

void AudioDecoder::config(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config)
		*((AdecConfig*)config) = *m_config;
}

int32_t AudioDecoder::Q_size(void)
{
	return m_decoder_Q.size();
}

void AudioDecoder::onMPkt(std::shared_ptr<MPacket> av_pkt)
{
	if (!m_signal_quit)
	{
		if (CHK_PROPERTY(av_pkt->prop, P_SEEK))
			m_decoder_Q.clear();
		if (!CHK_PROPERTY(av_pkt->prop, P_PAUS))
			m_decoder_Q.push(av_pkt);
	}
}


void AudioDecoder::closeCodecer(bool is_decoder)
{
	if (is_decoder)
		avcodec_free_context(&m_codec_ctx);
	m_signal_rset = true;
}

bool AudioDecoder::opendCodecer(bool is_decoder)
{
	if (is_decoder)
	{
		int32_t ret = 0;
		AVDictionary *opts = nullptr;
		//av_dict_set(&opts, "rtsp_transport", "tcp", 0);	//rtsp udp maybe blurred.
		// Open decoder base on m_codec & m_codec_ctx.
		if (m_config->enablehdw) {
			AVHWAccel *hwaccel = avfind_hwaccel_codec(m_codec_par->codec_id, m_codec_par->format);
			m_codec = avcodec_find_decoder_by_name((hwaccel)?hwaccel->name:"null");
			if (m_codec != NULL) {
				if (!(m_codec_ctx = avcodec_alloc_context3(m_codec))) {
					err("Failed to allocate the decoder context for stream !");
					return false;
				}
				if ((ret = avcodec_parameters_to_context(m_codec_ctx, m_codec_par)) < 0) {
					err("Failed to copy decoder parameters to input decoder context\n");
					return false;
				}
				ret = avcodec_open2(m_codec_ctx, m_codec, &opts);
				if (ret < 0) {
					war("no hardware decoder found for codec with id %d", m_codec_ctx->codec_id);
					return false;
				}
			}
		}
		if (nullptr == m_codec) {
			if (!(m_codec = avcodec_find_decoder(m_codec_par->codec_id))) {
				err("Failed to find decoder for stream #%d\n", m_codec_par->codec_id);
				return false;
			}
			if (!(m_codec_ctx = avcodec_alloc_context3(m_codec))) {
				err("Failed to allocate the decoder context for stream !");
				return false;
			}
			if ((ret = avcodec_parameters_to_context(m_codec_ctx, m_codec_par)) < 0) {
				err("Failed to copy decoder parameters to input decoder context\n");
				return false;
			}
			// Open decoder base on m_codec & m_codec_ctx.
			if (m_codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
				m_codec_ctx->framerate = m_framerate;
			if ((ret = avcodec_open2(m_codec_ctx, m_codec, &opts)) < 0) {
				err("Failed to open decoder for stream\n");
				return false;
			}
		}
	}
	return true;
}

bool AudioDecoder::resetCodecer(bool is_decoder)
{
	if (is_decoder)
	{
		closeCodecer(is_decoder);
		return opendCodecer(is_decoder);
	}
	return false;
}

void AudioDecoder::updateAttributes(void)
{
}

void AudioDecoder::clearCodec_Q(bool is_decoder)
{
	if (is_decoder)
	{
		m_decoder_Q.clear();
	}
}

std::shared_ptr<IDecoder> IDecoderFactory::createVideoDecoder(
	std::shared_ptr<IDecoderObserver> observer)
{
	return std::make_shared<VideoDecoder>(observer);
}

VideoDecoder::VideoDecoder(std::shared_ptr<IDecoderObserver> observer)
{
	SET_STATUS(m_status, E_INVALID);

	m_config = std::make_shared<VdecConfig>();
	m_observer  = observer;
	m_codec_par = avcodec_parameters_alloc();
	m_vcache = std::make_shared<MRframe>();
	m_vcache->type = AVMEDIA_TYPE_VIDEO;
	SET_PROPERTY(m_vcache->prop, P_PAUS);

	SET_STATUS(m_status, E_INITRES);
}

VideoDecoder::~VideoDecoder()
{
	stopd(true);
	avcodec_parameters_free(&m_codec_par);
}

void VideoDecoder::start(void)
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);

	m_signal_quit = false;
	m_worker = std::thread([&]()
	{
		while (!m_signal_quit)
		{
			int32_t ret = 0;
			std::shared_ptr<MPacket> av_pkt = nullptr;
			// 1.receive and dec MPacket.			
			if (!m_decoder_Q.try_peek(av_pkt))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			
			// 2.check and reset decoder.
			if (  !m_codec_par
				|| m_codec_par->format != av_pkt->pars->format
				|| m_codec_par->width  != av_pkt->pars->width
				|| m_codec_par->height != av_pkt->pars->height)
			{
				if ((ret = avcodec_parameters_copy(m_codec_par, av_pkt->pars)) < 0) {
					err("avcodec_parameters_copy failed! ret=%d\n", ret);
					break;
				}
				m_signal_rset = true;
			}

			if(m_signal_rset)
			{
				m_framerate = av_pkt->ufps;
				if (!resetCodecer(true))
					break;
				m_signal_rset = false;
				SET_STATUS(m_status, E_STARTED);
				war("Reset video Codecer!\n");
			}

			if (m_config->pauseflag)
			{
				if (!m_observer.expired())
					m_observer.lock()->onMFrm(m_vcache);
				av_usleep(10 * 1000);
				continue;
			}

			// 3.scale AVPacket timebase. <stream->codec: eg.1/25 1/44100>
			av_packet_rescale_ts(av_pkt->ppkt, av_pkt->sttb, m_codec_ctx->time_base);
			
			// 4. send packet to decoder.
			if ((ret = avcodec_send_packet(m_codec_ctx, av_pkt->ppkt)) < 0)
			{
				if (ret != AVERROR_EOF) {
					err("avcodec_send_packet failed! ret=%d\n", ret);					
					m_decoder_Q.popd(av_pkt);
					m_signal_rset = true;
				}
				av_usleep(10 * 1000);
				continue;
			}

			// 5.recv frame from decoder.
			while (true) 
			{
				std::shared_ptr<MRframe> av_frm = std::make_shared<MRframe>();
				
				if ((ret = avcodec_receive_frame(m_codec_ctx, av_frm->pfrm))< 0)
				{
					if (ret != -(EAGAIN) && ret != AVERROR_EOF)
						err( "Decoding failed!ret=%d\n", ret);
					break;
				}
				
				// 6.打包发送到渲染器.				
				av_frm->type = av_pkt->type;
				av_frm->prop = av_pkt->prop;
				av_frm->sttb = m_codec_ctx->time_base;// av_pkt->sttb;
				av_frm->upts = av_frm->pfrm->pts* av_q2d(av_frm->sttb); //这样对于B帧是不对的。av_pkt->upts;
				if ((ret = avcodec_parameters_copy(av_frm->pars, av_pkt->pars)) < 0) {
					err("avcodec_parameters_copy failed! ret=%d\n", ret);
					break;
				}
				av_frm->pfrm->pts = av_frm->pfrm->best_effort_timestamp;
				m_vcache->upts = av_frm->upts;
				if (!m_observer.expired() && av_frm->type == AVMEDIA_TYPE_VIDEO)
					m_observer.lock()->onMFrm(av_frm);
			}
			m_decoder_Q.popd(av_pkt);
		}
		war("Video decoder finished! m_signal_quit=%d\n", m_signal_quit);
	});
}

void 
VideoDecoder::stopd(bool stop_quik)
{
	CHK_RETURN(E_STARTED != status());
	SET_STATUS(m_status, E_STOPING);

	m_signal_quit = true;
	if (m_worker.joinable()) m_worker.join();
	closeCodecer(true);
	clearCodec_Q(true);

	SET_STATUS(m_status, E_STOPPED);
}

void 
VideoDecoder::pause(bool pauseflag)
{
	VdecConfig cfg;
	config(static_cast<void*>(&cfg));
	cfg.pauseflag = pauseflag;
	update(static_cast<void*>(&cfg));
}

void 
VideoDecoder::config(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config)
		*((VdecConfig*)config) = *m_config;
}

void
VideoDecoder::update(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	VdecConfig* p_cfg = (VdecConfig*)config;
	if (nullptr != config)
	{
		*m_config = *p_cfg;
		updateAttributes();
	}
}

STATUS 
VideoDecoder::status(void)
{
	return m_status;
}

int32_t 
VideoDecoder::Q_size(void)
{
	return m_decoder_Q.size();
}

void 
VideoDecoder::onMPkt(std::shared_ptr<MPacket> av_pkt)
{
	if (!m_signal_quit)
	{
		if (CHK_PROPERTY(av_pkt->prop,  P_SEEK))
			m_decoder_Q.clear();
		if (!CHK_PROPERTY(av_pkt->prop, P_PAUS))
			m_decoder_Q.push(av_pkt);
	}
}

void
VideoDecoder::updateAttributes(void)
{
}

void
VideoDecoder::clearCodec_Q(bool is_decoder)
{
	if (is_decoder)
	{
		m_decoder_Q.clear();
	}
}

void 
VideoDecoder::closeCodecer(bool is_decoder)
{
	if (is_decoder)
		avcodec_free_context(&m_codec_ctx);
	m_signal_rset = true;
}

bool 
VideoDecoder::opendCodecer(bool is_decoder)
{
	if (is_decoder)
	{
		int32_t ret = 0;
		AVDictionary *opts = nullptr;
		//av_dict_set(&opts, "rtsp_transport", "tcp", 0);	//rtsp udp maybe blurred.
		// Open decoder base on m_codec & m_codec_ctx.
		if (m_config->enablehdw) {
			AVHWAccel *hwaccel = avfind_hwaccel_codec(m_codec_par->codec_id, m_codec_par->format);
			m_codec = avcodec_find_decoder_by_name((hwaccel) ? hwaccel->name : "null");
			if (m_codec != NULL) {
				if (!(m_codec_ctx = avcodec_alloc_context3(m_codec))) {
					err("Failed to allocate the decoder context for stream !");
					return false;
				}
				if ((ret = avcodec_parameters_to_context(m_codec_ctx, m_codec_par)) < 0) {
					err("Failed to copy decoder parameters to input decoder context\n");
					return false;
				}
				ret = avcodec_open2(m_codec_ctx, m_codec, &opts);
				if (ret < 0) {
					war("no hardware decoder found for codec with id %d", m_codec_ctx->codec_id);
					return false;
				}
			}
		}
		if (nullptr == m_codec) {
			if (!(m_codec = avcodec_find_decoder(m_codec_par->codec_id))) {
				err("Failed to find decoder for stream #%d\n", m_codec_par->codec_id);
				return false;
			}
			if (!(m_codec_ctx = avcodec_alloc_context3(m_codec))) {
				err("Failed to allocate the decoder context for stream !");
				return false;
			}
			if ((ret = avcodec_parameters_to_context(m_codec_ctx, m_codec_par)) < 0){
				err("Failed to copy decoder parameters to input decoder context\n");
				return false;
			}
			// Open decoder base on m_codec & m_codec_ctx.
			if (m_codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
				m_codec_ctx->framerate = m_framerate;
			if ((ret = avcodec_open2(m_codec_ctx, m_codec, &opts)) < 0) {
				err("Failed to open decoder for stream\n");
				return false;
			}
		}
	}
	return true;
}

bool 
VideoDecoder::resetCodecer(bool is_decoder)
{
	if (is_decoder)
	{
		closeCodecer(is_decoder);
		return opendCodecer(is_decoder);
	}
	return false;
}

