﻿
#include "pubcore.hpp"
#include "decoder.hpp"

typedef enum AVPixelFormat(*AVGetFormatCb)(
	struct AVCodecContext *s, const enum AVPixelFormat * fmt);

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

static int32_t g_adecoder_ssidNo = 10000;
static int32_t g_vdecoder_ssidNo = 20000;

std::shared_ptr<IDecoder> IDecoderFactory::createAudioDecoder(std::shared_ptr<IDecoderObserver> observer){
	return std::make_shared<AudioDecoder>(observer);
}

AudioDecoder::AudioDecoder(std::shared_ptr<IDecoderObserver> observer):
	m_observer(observer)
{
	SET_STATUS(m_status, E_INVALID);
	
	m_config = std::make_shared<AdecConfig>();
	m_ssidNo = g_adecoder_ssidNo++;
	m_acache = std::make_shared<MRframe>();
	m_acache->ssid = m_ssidNo;
	m_acache->type = AVMEDIA_TYPE_AUDIO;
	SET_PROPERTY(m_acache->prop, P_PAUS);
	m_codec_par = avcodec_parameters_alloc();

	SET_STATUS(m_status, E_INITRES);
}

AudioDecoder::~AudioDecoder()
{
	stopd(true);
	avcodec_parameters_free(&m_codec_par);
}

void 
AudioDecoder::update(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config) {
		*m_config = *((AdecConfig*)config);
		updateAttributes();
	}
}

void
AudioDecoder::config(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config)
		*((AdecConfig*)config) = *m_config;
}

int32_t AudioDecoder::ssidNo(void)
{
	return 	m_ssidNo;
}

STATUS
AudioDecoder::status(void)
{
	return m_status;
}

int32_t
AudioDecoder::Q_size(void)
{
	return m_decoder_Q.size();
}

void
AudioDecoder::pause(bool pauseflag)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	m_config->pauseflag = pauseflag;
}

void AudioDecoder::start(void)
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);
	
	m_signal_quit = false;
	m_worker = std::thread([&]()
	{
		std::map<double, int64_t> cache_props;
		for (int64_t loop = 0,try_count=0; !m_signal_quit; ++loop, m_last_loop = av_gettime())
		{
			int32_t ret = 0;
			std::shared_ptr<MPacket> av_pkt = nullptr;
			// 1.receive and dec MPacket.			
			if (!m_decoder_Q.try_peek(av_pkt)) {
				sleepMs(STANDARDTK);
				continue;
			}
			// 2.check and reset decoder.
			if (!m_codec_par
				|| m_codec_par->format != av_pkt->pars->format
				|| m_codec_par->sample_rate != av_pkt->pars->sample_rate
				|| m_codec_par->channels != av_pkt->pars->channels) {
				if ((ret = avcodec_parameters_copy(m_codec_par, av_pkt->pars)) < 0) {
					err("avcodec_parameters_copy failed! ret=%d\n", ret);
					break;
				}
				m_signal_rset = true;
			}
			if (m_signal_rset) {
				if (!(AV_PKT_FLAG_KEY & av_pkt->ppkt->flags)) {
					m_decoder_Q.popd(av_pkt);
					continue;
				}
				m_framerate = av_pkt->ufps;
				if (!resetCodecer(true)) {
					SET_STATUS(m_status, E_STRTERR);
					for (ret = 0; ret < 300 && !m_signal_quit; ++ret)
						sleepMs(STANDARDTK);//3s.
					continue;
				}
				m_signal_rset = false;
				cache_props.clear();
				SET_STATUS(m_status, E_STARTED);
				war("Reset audio Decoder!\n");
			}

			if (m_config->pauseflag) {
				SET_STATUS(m_status, E_PAUSING);
				if (!m_observer.expired())
					m_observer.lock()->onDecoderFrame(m_acache);
				sleepMs(STANDARDTK);
				continue;
			}
			if (CHK_PROPERTY(av_pkt->prop, P_SEEK))
				avcodec_flush_buffers(m_codec_ctx);
			if (av_pkt->prop != 0) {// cache different props.
				cache_props[av_pkt->upts] = av_pkt->prop;
				msg("audio cache upts=[%lf] prop=%ld\n", av_pkt->upts, av_pkt->prop);
			}

			// 3.scale AVPacket timebase. <stream->codec: eg.1/44100->44100>
			av_packet_rescale_ts(av_pkt->ppkt, av_pkt->sttb, m_codec_ctx->time_base);
			// 4. send packet to decoder.
			if ((ret = avcodec_send_packet(m_codec_ctx, av_pkt->ppkt)) < 0) {
				SET_STATUS(m_status, E_RUNNERR);
				if (ret != AVERROR_EOF) {
					err("avcodec_send_packet failed! ret=%s\n", averr2str(ret));
					m_decoder_Q.popd(av_pkt);
					m_signal_rset = true;
				}
				sleepMs(STANDARDTK);
				continue;
			}
			// 5.receive frame from decoder.
			for (int32_t sequence = 0;; ++sequence) {
				std::shared_ptr<MRframe> av_frm = std::make_shared<MRframe>();
				if ((ret = avcodec_receive_frame(m_codec_ctx, av_frm->pfrm))< 0) {
					if (ret != -(EAGAIN) && ret != AVERROR_EOF)
						err("Decoding failed!ret=%s\n", averr2str(ret));
					break;
				}				
				av_frm->ssid = m_ssidNo;
				av_frm->type = av_pkt->type;
				av_frm->ufps = av_pkt->ufps;
				av_frm->prop = 0;
				av_frm->sttb = m_codec_ctx->time_base;// eg.1/44100
				av_frm->pfrm->pts = av_frm->pfrm->best_effort_timestamp;
				av_frm->upts = av_frm->pfrm->pts* av_q2d(av_frm->sttb);
				if (!cache_props.empty()) {
					for (auto iter = cache_props.begin(); iter != cache_props.end();) {
						if (fabs(av_frm->upts - iter->first) < 0.001) {
							av_frm->prop = iter->second;
							cache_props.erase(iter++);
							war("audio remove upts=[%lf] prop=%ld\n\n", av_frm->upts, iter->second);							
						}else {
							if (av_frm->upts - iter->first > 0) {
								av_frm->prop = iter->second;
								cache_props.erase(iter);
								war("!!!audio remove upts=[%lf] prop=%ld\n\n", av_frm->upts, iter->second);
							}
							iter++;
						}
					}
				}
				if ((ret = avcodec_parameters_copy(av_frm->pars, av_pkt->pars)) < 0) {
					err("avcodec_parameters_copy failed! ret=%s\n", averr2str(ret));
					break;
				}
				m_acache->upts = av_frm->upts;
				if (!m_observer.expired() && av_frm->type == AVMEDIA_TYPE_AUDIO)
					m_observer.lock()->onDecoderFrame(av_frm);
			}
			// 6.deque &update execute status.
			SET_STATUS(m_status, ((m_status == E_STOPING) ? E_STOPING : E_RUNNING));
			m_decoder_Q.popd(av_pkt);
		}
		war("Audio decoder finished! m_signal_quit=%d\n", m_signal_quit);
	});
}

void AudioDecoder::stopd(bool stop_quik)
{
	CHK_RETURN(E_STOPPED == status() || E_STOPING == status());
	SET_STATUS(m_status, E_STOPING);
	m_signal_quit = true;
	if (m_worker.joinable()) m_worker.join();
	closeCodecer(true);
	clearCodec_Q(true);
	SET_STATUS(m_status, E_STOPPED);
}

void AudioDecoder::pushPackt(std::shared_ptr<MPacket> av_pkt)
{
	if (!m_signal_quit) {
		if (CHK_PROPERTY(av_pkt->prop,  P_SEEK))
			m_decoder_Q.clear();
		if (!CHK_PROPERTY(av_pkt->prop, P_PAUS))
			m_decoder_Q.push(av_pkt);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void AudioDecoder::updateAttributes(void)
{
}

void AudioDecoder::clearCodec_Q(bool is_decoder)
{
	if (is_decoder)
		m_decoder_Q.clear();
}

void AudioDecoder::closeCodecer(bool is_decoder)
{
	if (is_decoder)
		avcodec_free_context(&m_codec_ctx);
	m_codec		= nullptr;
	m_codec_ctx = nullptr;
	m_signal_rset = true;
}

bool AudioDecoder::opendCodecer(bool is_decoder)
{
	int32_t ret = 0;
	AVDictionary *opts = nullptr;

	av_register_all();

	if (is_decoder)
	{
		if (m_config->avhwaccel) {
			AVHWAccel *hwaccel = avfind_hwaccel_codec(m_codec_par->codec_id, m_codec_par->format);
			if (hwaccel) {
				msg("using hwaccel->name=%s\n", hwaccel->name);
				if (!(m_codec = avcodec_find_decoder_by_name(hwaccel->name)))
					err("Find decoder[H] for [%d] failed! Err:%s\n", m_codec_par->codec_id, averr2str(ret));
			}
		}
		if (nullptr == m_codec) {
			if (!(m_codec = avcodec_find_decoder(m_codec_par->codec_id))) {
				err("Find decoder[S] for [%d] failed! Err:%s\n", m_codec_par->codec_id, averr2str(ret));
				return false;
			}
		}
		if (!(m_codec_ctx = avcodec_alloc_context3(m_codec))) {
			err("avcodec_alloc_context3 failed! Err:%s\n", averr2str(ret));
			return false;
		}
		if ((ret = avcodec_parameters_to_context(m_codec_ctx, m_codec_par)) < 0) {
			err("avcodec_parameters_to_context failed! Err:%s\n", averr2str(ret));
			return false;
		}
		if (m_codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO) {
			m_codec_ctx->framerate = m_framerate;
			if (m_codec_ctx->codec_id == AV_CODEC_ID_H264) {
				av_dict_set(&opts, "preset", "superfast", 0);
				av_dict_set(&opts, "tune", "zerolatency", 0);
			}
			if (m_codec_ctx->codec_id == AV_CODEC_ID_H265) {
				av_dict_set(&opts, "x265-params", "qp=20", 0);
				av_dict_set(&opts, "tune", "zero-latency", 0);
				av_dict_set(&opts, "preset", "ultrafast", 0);
			}
		}
		// Open decoder base on m_codec & m_codec_ctx.
		if ((ret = avcodec_open2(m_codec_ctx, m_codec, &opts)) < 0) {
			err("avcodec_open2 failed! Err:%s\n", averr2str(ret));
			return false;
		}
	}
	return true;
}

bool AudioDecoder::resetCodecer(bool is_decoder)
{
	closeCodecer(is_decoder);
	return opendCodecer(is_decoder);
}

std::shared_ptr<IDecoder> IDecoderFactory::createVideoDecoder(
	std::shared_ptr<IDecoderObserver> observer) {
	return std::make_shared<VideoDecoder>(observer);
}

VideoDecoder::VideoDecoder(std::shared_ptr<IDecoderObserver> observer)
{
	SET_STATUS(m_status, E_INVALID);

	m_config = std::make_shared<VdecConfig>();
	m_ssidNo = g_vdecoder_ssidNo++;
	m_observer  = observer;
	m_codec_par = avcodec_parameters_alloc();
	m_vcache = std::make_shared<MRframe>();
	m_vcache->ssid = m_ssidNo;
	m_vcache->type = AVMEDIA_TYPE_VIDEO;
	SET_PROPERTY(m_vcache->prop, P_PAUS);

	SET_STATUS(m_status, E_INITRES);
}

VideoDecoder::~VideoDecoder()
{
	stopd(true);
	avcodec_parameters_free(&m_codec_par);
}

void
VideoDecoder::update(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config) {
		*m_config = *((VdecConfig*)config);
		updateAttributes();
	}
}

void
VideoDecoder::config(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config)
		*((VdecConfig*)config) = *m_config;
}

int32_t
VideoDecoder::ssidNo(void)
{
	return m_ssidNo;
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
VideoDecoder::pause(bool pauseflag)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	m_config->pauseflag = pauseflag;
}

void VideoDecoder::start(void)
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);

	m_signal_quit = false;
	m_worker = std::thread([&]()
	{
		std::map<double, int64_t> cache_props;
		for (int64_t loop = 0; !m_signal_quit; ++loop, m_last_loop = av_gettime())
		{
			int32_t ret = 0;
			std::shared_ptr<MPacket> av_pkt = nullptr;
			// 1.receive and dec MPacket.			
			if (!m_decoder_Q.try_peek(av_pkt)) {
				sleepMs(STANDARDTK);
				continue;
			}
			// 2.check and reset decoder.
			if (  !m_codec_par
				|| m_codec_par->format != av_pkt->pars->format
				|| m_codec_par->width  != av_pkt->pars->width
				|| m_codec_par->height != av_pkt->pars->height)	{
				if ((ret = avcodec_parameters_copy(m_codec_par, av_pkt->pars)) < 0) {
					SET_STATUS(m_status, E_RUNNERR);
					err("avcodec_parameters_copy failed! ret=%s\n", averr2str(ret));
					break;
				}
				m_signal_rset = true;
			}
			if(m_signal_rset) {
				if (!(AV_PKT_FLAG_KEY & av_pkt->ppkt->flags)) {
					m_decoder_Q.popd(av_pkt);
					continue;
				}
				m_framerate = av_pkt->ufps;
				if (!resetCodecer(true)) {
					SET_STATUS(m_status, E_STRTERR);
					for (ret = 0; ret < 300 && !m_signal_quit; ++ret)
						sleepMs(STANDARDTK);//3s.
					continue;
				}				
				m_signal_rset = false;
				cache_props.clear();
				SET_STATUS(m_status, E_STARTED);
				war("Reset video Decoder!\n");
			}
			if (m_config->pauseflag) {
				SET_STATUS(m_status, E_PAUSING);
				if (!m_observer.expired())
					m_observer.lock()->onDecoderFrame(m_vcache);
				sleepMs(STANDARDTK);
				continue;
			}
			if (CHK_PROPERTY(av_pkt->prop, P_SEEK))
				avcodec_flush_buffers(m_codec_ctx);
			if (av_pkt->prop != 0) {// cache different props.
				cache_props[av_pkt->upts] = av_pkt->prop;
				msg("video cache upts=[%lf] prop=%ld\n", av_pkt->upts, av_pkt->prop);
			}

			// 3.scale AVPacket timebase. <stream->codec: eg.1/90000->1/25>
			av_packet_rescale_ts(av_pkt->ppkt, av_pkt->sttb, m_codec_ctx->time_base);			
			// 4. send packet to decoder.		
			if ((ret = avcodec_send_packet(m_codec_ctx, av_pkt->ppkt)) < 0) {
				SET_STATUS(m_status, E_RUNNERR);
				if (ret != AVERROR_EOF) {
					err("avcodec_send_packet failed! ret=%s\n", averr2str(ret));
					m_decoder_Q.popd(av_pkt);
					m_signal_rset = true;
				}
				sleepMs(STANDARDTK);
				continue;
			}
			// 5.recv frame from decoder.
			for (int32_t sequence = 0;; ++sequence) {
				std::shared_ptr<MRframe> av_frm = std::make_shared<MRframe>();
				if ((ret = avcodec_receive_frame(m_codec_ctx, av_frm->pfrm))< 0){
					if (ret != -(EAGAIN) && ret != AVERROR_EOF)
						err("Decoding failed! ret=%s\n", averr2str(ret));
					break;
				}
				av_frm->ssid = m_ssidNo;
				av_frm->type = av_pkt->type;				
				av_frm->ufps = av_pkt->ufps;
				av_frm->prop = 0;
				av_frm->sttb = m_codec_ctx->time_base;//av_frm->sttb=1/25
				av_frm->pfrm->pts = av_frm->pfrm->best_effort_timestamp;
				av_frm->upts = av_frm->pfrm->pts* av_q2d(av_frm->sttb);
				if (!cache_props.empty()) {
					for (auto iter = cache_props.begin(); iter != cache_props.end();) {
						if (fabs(av_frm->upts - iter->first) < 0.001) {
							av_frm->prop = iter->second;
							cache_props.erase(iter++);
							war("video remove upts=[%lf] prop=%ld\n\n", av_frm->upts, iter->second);
						}else {
							if (av_frm->upts - iter->first > 0) {
								av_frm->prop = iter->second;
								cache_props.erase(iter);
								war("!!!video remove upts=[%lf] prop=%ld\n\n", av_frm->upts, iter->second);
							}
							iter++;
						}
					}
				}
				if ((ret = avcodec_parameters_copy(av_frm->pars, av_pkt->pars)) < 0) {
					SET_STATUS(m_status, E_RUNNERR);
					err("avcodec_parameters_copy failed! ret=%s\n", averr2str(ret));
					break;
				}
				m_vcache->upts = av_frm->upts;
				if (!m_observer.expired() && av_frm->type == AVMEDIA_TYPE_VIDEO)
					m_observer.lock()->onDecoderFrame(av_frm);
			}
			// 6.deque &update execute status.
			SET_STATUS(m_status, ((m_status == E_STOPING) ? E_STOPING : E_RUNNING));
			m_decoder_Q.popd(av_pkt);
		}
		war("Video decoder finished! m_signal_quit=%d\n", m_signal_quit);
	});
}

void 
VideoDecoder::stopd(bool stop_quik)
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
VideoDecoder::pushPackt(std::shared_ptr<MPacket> av_pkt)
{
	if (!m_signal_quit) {
		if (CHK_PROPERTY(av_pkt->prop,  P_SEEK))
			m_decoder_Q.clear();
		if (!CHK_PROPERTY(av_pkt->prop, P_PAUS))
			m_decoder_Q.push(av_pkt);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void
VideoDecoder::updateAttributes(void)
{
}

void
VideoDecoder::clearCodec_Q(bool is_decoder)
{
	if (is_decoder)
		m_decoder_Q.clear();
}

void 
VideoDecoder::closeCodecer(bool is_decoder)
{
	avcodec_free_context(&m_codec_ctx);
	m_codec		= nullptr;
	m_codec_ctx = nullptr;
	m_signal_rset = true;
}

bool 
VideoDecoder::opendCodecer(bool is_decoder)
{
	int32_t ret = 0;
	AVDictionary *opts = nullptr;

	av_register_all();

	if (is_decoder)
	{
		if (m_config->avhwaccel) {
			AVHWAccel *hwaccel = avfind_hwaccel_codec(m_codec_par->codec_id, m_codec_par->format);
			if (hwaccel) {
				msg("using hwaccel->name=%s\n", hwaccel->name);
				if (!(m_codec = avcodec_find_decoder_by_name(hwaccel->name)))
					err("Find decoder[H] for [%d] failed! Err:%s\n", m_codec_par->codec_id, averr2str(ret));
			}
		}
		if (nullptr == m_codec) {
			if (!(m_codec = avcodec_find_decoder(m_codec_par->codec_id))) {
				err("Find decoder[S] for [%d] failed! Err:%s\n", m_codec_par->codec_id, averr2str(ret));
				return false;
			}
		}
		if (!(m_codec_ctx = avcodec_alloc_context3(m_codec))) {
			err("avcodec_alloc_context3 failed! Err:%s\n", averr2str(ret));
			return false;
		}
		if ((ret = avcodec_parameters_to_context(m_codec_ctx, m_codec_par)) < 0) {
			err("avcodec_parameters_to_context failed! Err:%s\n", averr2str(ret));
			return false;
		}
		if (m_codec_ctx->codec_type == AVMEDIA_TYPE_VIDEO)
			m_codec_ctx->framerate = m_framerate;
		// Open decoder base on m_codec & m_codec_ctx.
		if ((ret = avcodec_open2(m_codec_ctx, m_codec, &opts)) < 0) {
			err("avcodec_open2 failed! Err:%s\n", averr2str(ret));
			return false;
		}
	}
	return true;
}

bool 
VideoDecoder::resetCodecer(bool is_decoder)
{
	closeCodecer(is_decoder);
	return opendCodecer(is_decoder);
}

