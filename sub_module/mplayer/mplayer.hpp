
#pragma  once

#include "pubcore.hpp"
#include "demuxer.hpp"
#include "decoder.hpp"
#include "mrender.hpp"

class IMpTimerObserver
{
public:
	IMpTimerObserver();
	~IMpTimerObserver();

private:

};

class Mplayer :
	//public IMpTimerObserver,
	public IMediaDemuxerObserver,
	public IAudioDecoderObserver,
	public IVideoDecoderObserver,
	public IAudioMrenderObserver,
	public IVideoMrenderObserver,
	public std::enable_shared_from_this<Mplayer>
{
public:
	Mplayer(const char* url, const char* speakr = "", const void* window = nullptr) 
		:m_inputf(url), m_speakr(speakr), m_window(window)
	{

	}
	
	~Mplayer() = default;

	void start() {
		mdemuxer = IMediaDemuxer::create(m_inputf, shared_from_this());
		vdecoder = IVideoDecoder::create(shared_from_this());
		adecoder = IAudioDecoder::create(shared_from_this());
		vmrender = IVideoMrender::create(m_window, shared_from_this());
		amrender = IAudioMrender::create(m_speakr, shared_from_this());

		mdemuxer->start();
		vdecoder->start();
		adecoder->start();
		vmrender->start();
		amrender->start();
	}

	void stopd() {
		mdemuxer->stopd();
		vdecoder->stopd();
		adecoder->stopd();
		vmrender->stopd();
		amrender->stopd();
	}

	// Demuxer->Decoder
	void onAudioPacket(std::shared_ptr<MPacket> av_pkt) override {
		//msg("@prop=%lld, type=%d upts=%g s\n", av_pkt->prop, av_pkt->type, av_pkt->upts);
		mdemuxer->pause((adecoder->Q_size() >= MAX_AUDIO_Q_SIZE));
		adecoder->onAudioPacket(av_pkt);
	}

	void onVideoPacket(std::shared_ptr<MPacket> av_pkt) override {
		//msg("@prop=%lld, type=%d upts=%g s\n", av_pkt->prop, av_pkt->type, av_pkt->upts);
		mdemuxer->pause((vdecoder->Q_size() >= MAX_VIDEO_Q_SIZE));
		vdecoder->onVideoPacket(av_pkt);
	}

	// Decoder->Mrender
	void onAudioRFrame(std::shared_ptr<MRframe> av_frm) override {
		adecoder->pause((amrender->Q_size() >= MAX_VIDEO_Q_SIZE));
		amrender->onAudioRFrame(av_frm);
		if (!CHK_PROPERTY(av_frm->prop, P_PAUS))
		{
			int32_t delay_ms = (int32_t)(av_frm->pfrm->pkt_duration*av_q2d(av_frm->sttb) * 1000);
			std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
			msg("@ prop=%lld, type=%d upts=%g delay=%d\n", av_frm->prop, av_frm->type, av_frm->upts, delay_ms);
		}
	}

	void onVideoRFrame(std::shared_ptr<MRframe> av_frm) override {
	
		adecoder->pause((amrender->Q_size() >= MAX_VIDEO_Q_SIZE));
		vmrender->onVideoRFrame(av_frm);
		if (!CHK_PROPERTY(av_frm->prop, P_PAUS))
		{
			int32_t delay_ms = (int32_t)(av_frm->pfrm->pkt_duration*av_q2d(av_frm->sttb) * 1000);
			std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
			msg("@ prop=%lld, type=%d upts=%g delay=%d\n", av_frm->prop, av_frm->type, av_frm->upts, delay_ms);
		}
	}

	// Mrender->Maneger.
	void onAudioTimePoint(double upts) override {		
		msg("-->pts=%g\n", upts);
	}

	void onVideoTimePoint(double upts) override {		
		msg("-->pts=%g\n", upts);
	}

private:
	const char* m_inputf{ nullptr };
	const char* m_speakr{ nullptr };
	const void* m_window{ nullptr };
	std::shared_ptr<IMediaDemuxer> 	mdemuxer{ nullptr };
	std::shared_ptr<IAudioDecoder> 	adecoder{ nullptr };
	std::shared_ptr<IVideoDecoder> 	vdecoder{ nullptr };
	std::shared_ptr<IAudioMrender> 	amrender{ nullptr };
	std::shared_ptr<IVideoMrender> 	vmrender{ nullptr };
};