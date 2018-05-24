
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

#define AV_SYNC_THRESHOLD 0.01
#define AV_NOSYNC_THRESHOLD 10.0

class Clock
{
public:
	Clock();
	~Clock();

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
		m_mdemuxer = IMediaDemuxer::create(m_inputf, shared_from_this());
		m_vdecoder = IVideoDecoder::create(shared_from_this());
		m_adecoder = IAudioDecoder::create(shared_from_this());
		m_vmrender = IVideoMrender::create(m_window, shared_from_this());
		m_amrender = IAudioMrender::create(m_speakr, shared_from_this());

		m_mdemuxer->start();
		m_vdecoder->start();
		m_adecoder->start();
		m_vmrender->start();
		m_amrender->start();

		m_vrefesh_worker = std::thread([&]()
		{
			while (!m_sig_quit)
			{
				std::shared_ptr<MRframe> av_frm;
				if (m_vrender_Q.empty()) {
					av_usleep(10 * 1000);
					continue;
				}else {
					std::lock_guard<std::mutex> locker(m_vrender_Q_mutx);
					av_frm = m_vrender_Q.front();
					m_vrender_Q.pop();
				}
				double pts_diff;
				double delay_until_next_wake;
				bool late_first_frame = false;
				
#if 1
				// 1.更新当前时间戳s 和当前时间:us.
				m_current_ptsv = av_frm->upts;
				m_current_pts_timev = av_gettime();
				// 计算时间戳差值，大致等于duration长度，但是更精确, 
				// 异常情况下，第一帧pts可能是负数或很大的一个数。
				pts_diff = av_frm->upts - m_previous_ptsv;

				if (m_first_framev)
				{
					late_first_frame = true;// pts_diff >= 1.0;//第一帧pts异常就修正一下吧,后续还是间隔很大就不管了,可能是seek导致的。
					m_first_framev = false;
				}
				
				// 修正渲染延时:pts_diff。。。,这个值吧就是delay的一个参考时间。delay不能超过这个值
				if (pts_diff <= 0 || late_first_frame)
				{//修正第一帧负的pts以及正的但是大于1s的pts，diff太大会导致阻塞，<0会导致不延时。
					//即时间戳回绕的问题，这里仅简单的保持为上次的延时。正常只进来一次，多次进入肯定是他妈编码的时间戳的出问题了。
					pts_diff = m_previous_pts_diffv;
				}
			
				// 保留当前pts和pts_diff供, 偶然异常时补偿。
				m_previous_pts_diffv = pts_diff;
				m_previous_ptsv = av_frm->upts;
#if 1
				// 视频渲染线程需要进行同步。音频不需要进入。因为我们用音频来同步视频。
				// 当然，你不嫌弃反向操作垃圾也可以反着来。或者用外部精确时钟来同步二者。
				if (1)
				{
					dbg("#1pts_diff:%g\n", pts_diff); 
					pts_diff = getSyncAdjustedPtsDiff(av_frm->upts, pts_diff);
					dbg("#2pts_diff:%g\n", pts_diff);
				}
					
				// 预测下次唤醒当前工作的时间点，系统时间-绝对的。
				m_next_wakev += pts_diff;
				// 计算下次唤醒需要delay的时长，由于第一次调用时m_next_wake=0，所以后面还需要修正第一次的delay时间。
				delay_until_next_wake = m_next_wakev - (av_gettime() / 1000000.0L);
			
				if (delay_until_next_wake < 0.010L)
				{
					m_next_wakev = (av_gettime() / 1000000.0L) + pts_diff;
					delay_until_next_wake = 0.010L;
				}

				if (delay_until_next_wake > pts_diff)
					delay_until_next_wake = pts_diff;
#endif
				m_vmrender->onVideoRFrame(av_frm);
				int offset = 0;
				std::this_thread::sleep_for(AT::micros_t((int)(delay_until_next_wake * 1000000 - offset)));
#endif	
			}
		});

	
		m_arefesh_worker = std::thread([&]()
		{
			while (!m_sig_quit)
			{
				//AT::Timer timer;
				double pts_diff;
				double delay_until_next_wake;
				bool late_first_frame = false;

				//pop..
				std::shared_ptr<MRframe> av_frm;
				if (m_arender_Q.empty()) {
					av_usleep(10 * 1000);
					continue;
				}else {
					std::lock_guard<std::mutex> locker(m_arender_Q_mutx);
					av_frm = m_arender_Q.front();
					m_arender_Q.pop();
				}

#if 1
				// 1.更新当前时间戳s 和当前时间:us.
				m_current_pts		= av_frm->upts;
				m_current_pts_time	= av_gettime();
				// 计算时间戳差值，大致等于duration长度，但是更精确, 
				// 异常情况下，第一帧pts可能是负数或很大的一个数。
				pts_diff = av_frm->upts - m_previous_pts;

				if (m_first_frame)
				{
					//clog(LOG_INFO, "receive first %s frame,pts_diff:%f", syncType() == SyncClock::AUDIO_MASTER ? "audio" : "video ", pts_diff);
					late_first_frame = pts_diff >= 1.0;//第一帧pts异常就修正一下吧,后续还是间隔很大就不管了,可能是seek导致的。
					m_first_frame = false;
				}
				// 修正渲染延时:pts_diff。。。,这个值吧就是delay的一个参考时间。delay不能超过这个值
				if (pts_diff <= 0 || late_first_frame)
				{//修正第一帧负的pts以及正的但是大于1s的pts，diff太大会导致阻塞，<0会导致不延时。
				 //即时间戳回绕的问题，这里仅简单的保持为上次的延时。正常只进来一次，多次进入肯定是他妈编码的时间戳的出问题了。
					pts_diff = m_previous_pts_diff;
				}

				// 保留当前pts和pts_diff供, 偶然异常时补偿。
				m_previous_pts_diff = pts_diff;
				m_previous_pts = av_frm->upts;

				// 视频渲染线程需要进行同步。音频不需要进入。因为我们用音频来同步视频。
				// 当然，你不嫌弃反向操作垃圾也可以反着来。或者用外部精确时钟来同步二者。
				if (0)
				{
					pts_diff = getSyncAdjustedPtsDiff(av_frm->upts, pts_diff);
				}
				// 预测下次唤醒当前工作的时间点，系统时间-绝对的。
				m_next_wake += pts_diff;
				// 计算下次唤醒需要delay的时长，由于第一次调用时m_next_wake=0，所以后面还需要修正第一次的delay时间。
				delay_until_next_wake = m_next_wake - (av_gettime() / 1000000.0L);
				
				if (delay_until_next_wake < 0.010L)
				{
					m_next_wake = (av_gettime() / 1000000.0L) + pts_diff;
					delay_until_next_wake = 0.010L;
				}

				if (delay_until_next_wake > pts_diff)
					delay_until_next_wake = pts_diff;
				
				m_amrender->onAudioRFrame(av_frm);
				int offset = 0;
				std::this_thread::sleep_for(AT::micros_t((int)(delay_until_next_wake * 1000000 - offset)));
#else
				auto beg = std::chrono::high_resolution_clock::now();
				static std::chrono::high_resolution_clock::time_point  pre = beg;
				millis_t tp_diff = std::chrono::duration_cast<std::chrono::milliseconds>(beg - pre);
				pre = beg;
				static int32_t delay_ms = 0;
				int32_t drift = tp_diff.count() - frame_duration;
				if ( drift < -2 )
					delay_ms += 1;
				if ( drift >  0 )
					delay_ms -= drift;
				int32_t size = av_samples_get_buffer_size(nullptr, av_frm->pfrm->channels, av_frm->pfrm->nb_samples, (enum AVSampleFormat)av_frm->pfrm->format, 1);
				if (m_amrender->cached() <= size)
					delay_ms -= 1;
				delay_ms = (delay_ms<0) ? 0 : delay_ms;
				std::cout << "call rendre interval: " << tp_diff.count() << " drift=" << drift << " delay_ms="<< delay_ms<< std::endl;
				m_amrender->onAudioRFrame(av_frm);//音频连续的关键是保证cahced不为0.
				std::this_thread::sleep_for(millis_t(23));
#endif					
			}
		});
	}

	double decodeClock()
	{
		double delta = (av_gettime() - m_current_pts_time) / 1000000.0;//s
		return m_current_pts + delta;
	}

	double getSyncAdjustedPtsDiff(double pts, double pts_diff)
	{
		double new_pts_diff = pts_diff;
		double sync_time = decodeClock();// m_sync_clock->syncClock();
		double diff = pts - sync_time;//s
		double sync_threshold = (pts_diff > AV_SYNC_THRESHOLD)
			? pts_diff : AV_SYNC_THRESHOLD;

		//if (fabs(diff) < AV_NOSYNC_THRESHOLD) 
		{
			if (diff <= -sync_threshold) {
				new_pts_diff = 0;
			}else if (diff >= sync_threshold) {
				new_pts_diff = 2 * pts_diff;
			}
		}
		return new_pts_diff;
	}


	void stopd() {
		m_mdemuxer->stopd();
		m_vdecoder->stopd();
		m_adecoder->stopd();
		m_vmrender->stopd();
		m_amrender->stopd();
		m_sig_quit = true;
		m_vrefesh_worker.join();
		m_arefesh_worker.join();
	}

	// Demuxer->Decoder
	void onAudioPacket(std::shared_ptr<MPacket> av_pkt) override {
		//printf("a----av_pkt: prop=%lld, type=%d upts=%g s\n", av_pkt->prop, av_pkt->type, av_pkt->upts);
		m_mdemuxer->pause((m_adecoder->Q_size() >= MAX_AUDIO_Q_SIZE));
		m_adecoder->onAudioPacket(av_pkt);
	}

	void onVideoPacket(std::shared_ptr<MPacket> av_pkt) override {
		//printf("v----av_pkt: prop=%lld, type=%d upts=%g s\n", av_pkt->prop, av_pkt->type, av_pkt->upts);
		m_mdemuxer->pause((m_vdecoder->Q_size() >= MAX_VIDEO_Q_SIZE));
		m_vdecoder->onVideoPacket(av_pkt);
	}

	// Decoder->Mrender
	void onAudioRFrame(std::shared_ptr<MRframe> av_frm) override {
#if 0
		m_adecoder->pause((m_amrender->Q_size() >= MAX_AUDIO_Q_SIZE));
		std::async(std::launch::async, [&]() 
		{
			if (!CHK_PROPERTY(av_frm->prop, P_PAUS))
			{
				m_amrender->onAudioRFrame(av_frm);
				int32_t delay_ms = (int32_t)(av_frm->pfrm->pkt_duration*av_q2d(av_frm->sttb) * 1000);
				//std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
				dbg("@prop=%lld, type=%d upts=%g delay=%d\n", av_frm->prop, av_frm->type, av_frm->upts, delay_ms);
			}
		});
#else
		std::lock_guard<std::mutex> locker(m_arender_Q_mutx);
		m_adecoder->pause((m_amrender->Q_size() >= MAX_AUDIO_Q_SIZE));
		if (!CHK_PROPERTY(av_frm->prop, P_PAUS))
		{
			m_arender_Q.push(av_frm);
			//m_arender_Q_cond.notify_all();
		}
#endif
	}

	void onVideoRFrame(std::shared_ptr<MRframe> av_frm) override {
#if 0
		//m_vdecoder->pause((m_vmrender->Q_size() >= MAX_VIDEO_Q_SIZE));
		if (!CHK_PROPERTY(av_frm->prop, P_PAUS))
		{
			m_vmrender->onVideoRFrame(av_frm);
			int32_t delay_ms = (int32_t)(av_frm->pfrm->pkt_duration*av_q2d(av_frm->sttb) * 1000);
			//std::this_thread::sleep_for(std::chrono::milliseconds(delay_ms));
			dbg("@prop=%lld, type=%d upts=%g delay=%d\n", av_frm->prop, av_frm->type, av_frm->upts, delay_ms);
		}
#else
		std::lock_guard<std::mutex> locker(m_vrender_Q_mutx);
		m_vdecoder->pause((m_vrender_Q.size() >= MAX_VIDEO_Q_SIZE));
		if (!CHK_PROPERTY(av_frm->prop, P_PAUS))
		{
			m_vrender_Q.push(av_frm);
			//m_vrender_Q_cond.notify_all();
		}
#endif
	}

	// Mrender->Maneger.
	void onAudioTimePoint(double upts) override {
		//msg("-->pts=%g\n", upts);
	}

	void onVideoTimePoint(double upts) override {
		//msg("-->pts=%g\n", upts);
	}
private:
	std::atomic<bool>				m_sig_quit{ false };
	const char*						m_inputf{ nullptr };
	const char*						m_speakr{ nullptr };
	const void*						m_window{ nullptr };
	std::shared_ptr<IMediaDemuxer> 	m_mdemuxer{ nullptr };
	std::shared_ptr<IAudioDecoder> 	m_adecoder{ nullptr };
	std::shared_ptr<IVideoDecoder> 	m_vdecoder{ nullptr };
	std::shared_ptr<IAudioMrender> 	m_amrender{ nullptr };
	std::shared_ptr<IVideoMrender> 	m_vmrender{ nullptr };
	std::queue<std::shared_ptr<MRframe>> m_vrender_Q;
	std::mutex						m_vrender_Q_mutx;
	std::condition_variable			m_vrender_Q_cond;
	std::queue<std::shared_ptr<MRframe>> m_arender_Q;
	std::mutex						m_arender_Q_mutx;
	std::condition_variable			m_arender_Q_cond;
	std::thread						m_vrefesh_worker;
	std::thread						m_arefesh_worker;

	double m_current_ptsv = 0;
	int64_t m_current_pts_timev = 0;
	double m_previous_ptsv = 0;
	double m_previous_pts_diffv = 40e-3;
	int64_t m_start_ptsv = 0;
	double m_predicted_ptsv = 0.0;
	bool m_first_framev = true;
	double m_next_wakev = 0.0;

	double m_current_pts = 0;
	int64_t m_current_pts_time = 0;
	double m_previous_pts = 0;
	double m_previous_pts_diff = 40e-3;
	int64_t m_start_pts = 0;
	double m_predicted_pts = 0.0;
	bool m_first_frame = true;
	double m_next_wake = 0.0;

};