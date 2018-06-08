
#include "pubcore.hpp"
#include "mplayer.hpp"
Mplayer::Mplayer(const char* url, const char* speakr, const void* window)
	: m_window(window)
{
	char tmp_uf8[512] = { 0 };
	Mul2Utf8(url,		tmp_uf8, sizeof(tmp_uf8));
	m_inputf = tmp_uf8;
	Mul2Utf8(speakr,	tmp_uf8, sizeof(tmp_uf8));
	m_speakr = tmp_uf8;
}

Mplayer::~Mplayer() 
{
	stopd();
}

STATUS Mplayer::status(void)
{
	return STATUS();
}

void Mplayer::config(void * config)
{
}

void Mplayer::update(void * config)
{
}

bool Mplayer::mInit(void)
{
	if (!m_signal_init)
	{
		m_mdemuxer = IDemuxer::create(m_inputf.c_str(),  shared_from_this());
		m_vdecoder = IDecoderFactory::createVideoDecoder(shared_from_this());
		m_adecoder = IDecoderFactory::createAudioDecoder(shared_from_this());
		m_vmrender = IMrenderFactory::createVideoMrender(m_window,			shared_from_this());
		m_amrender = IMrenderFactory::createAudioMrender(m_speakr.c_str(),	shared_from_this());
		m_signal_init = (E_INVALID != m_mdemuxer->status() 
			&& E_INVALID != m_vdecoder->status()
			&& E_INVALID != m_adecoder->status() 
			&& E_INVALID != m_vmrender->status()
			&& E_INVALID != m_amrender->status());		
	}
	return m_signal_init;
}

void
Mplayer::start() 
{
	if (mInit())
	{
		m_vmrender->start();
		m_amrender->start();
		m_vdecoder->start();
		m_adecoder->start();
		m_mdemuxer->start();
	}

#if 1
	m_signal_quit = false;
	m_vrefesh_worker = std::thread([&]()
	{
		m_current_ptsv = 0;
		m_current_pts_timev = 0;
		m_previous_ptsv = 0;
		m_previous_pts_diffv = 40e-3;
		m_start_ptsv = 0;
		m_predicted_ptsv = 0.0;
		m_first_framev = true;
		m_next_wakev = 0.0;
		
		m_vrender_Q.clear();
		
		while (!m_signal_quit)
		{
			std::shared_ptr<MRframe> av_frm = nullptr;
			if (!m_vrender_Q.try_peek(av_frm))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			m_vrender_Q.popd(av_frm);

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

			// 保留当前pts和pts_diff供,偶然异常时补偿。
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

			// 计算下次唤醒当前工作的时间点，m_next_wakev表示系统时间。
			m_next_wakev += pts_diff;
			// 计算下次唤醒需要delay的时长，由于第一次调用时m_next_wake=0，所以后面还需要修正第一次的delay时间。
			delay_until_next_wake = m_next_wakev - (av_gettime() / 1000000.0L);
			 
			if (delay_until_next_wake < 0.010L)
			{//第一次进来delay_until_next_wake肯定是负数要修正一下。因为m_next_wakev=0+40ms <(av_gettime() / 1000000.0L);
			 //预测下次醒来的系统时间，实际中肯定会早或者晚，
				//pts_diff=40ms时，第一次睡10ms很可能会早醒来,(a+40)-(a+10) >0
				m_next_wakev = (av_gettime() / 1000000.0L) + pts_diff;
				delay_until_next_wake = 0.010L;//由于系统时钟不稳定，所以sleep40ms 实际
				//很可能已经过了60ms，此时(a+40)-(a+60) <0.则会再次进入该
				//分支，这里延时10ms是经验值。
			}

			//pts_diff=40ms时，第一次睡10ms很可能会早醒来,(a+40)-(a+10) >0,也就是说
			// 如果delay 在10-40ms的范围内，就正常sleep，<10 证明醒来的晚了，就快速播放。
			//》pts_diff  正常情况下，系统是不可能提前1个pts_diff醒来的。
			//这总情况可能是pts_diff调整，如视频同步导致。
			//也就是说视频慢太多了pts_diff被修改为0了，这时delay_until_next_wake=0，立即刷新视频。
			//直到追到音频的时间戳附近，pts_diff回复正常，匀速播放，所以在不进行音视频
			//同步的情况下，上面的分支进入的可能性更大。。
			if (delay_until_next_wake > pts_diff)
				delay_until_next_wake = pts_diff;
#endif
			m_vmrender->onMFrm(av_frm);
			int offset = 0;
			std::this_thread::sleep_for(AT::micros_t((int)(delay_until_next_wake * 1000000 - offset)));
#endif	
		}
	});


	m_arefesh_worker = std::thread([&]()
	{
		m_current_pts = 0;
		m_current_pts_time = 0;
		m_previous_pts = 0;
		m_previous_pts_diff = 40e-3;
		m_start_pts = 0;
		m_predicted_pts = 0.0;
		m_first_frame = true;
		m_next_wake = 0.0;
		m_arender_Q.clear();
		while (!m_signal_quit)
		{
			//pop..
			std::shared_ptr<MRframe> av_frm = nullptr;
			if (!m_arender_Q.try_peek(av_frm))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			m_arender_Q.popd(av_frm);

			//AT::Timer timer;
			double pts_diff;
			double delay_until_next_wake;
			bool late_first_frame = false;

			// 1.更新当前时间戳s 和当前时间:us.
			m_current_pts = av_frm->upts;
			m_current_pts_time = av_gettime();
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
				m_next_wake = (av_gettime() / 1000000.0L) + 0.010L;
				delay_until_next_wake = 0.010L;
			}
			if (delay_until_next_wake > pts_diff)
				delay_until_next_wake = pts_diff;

// 			int32_t buff_size = av_samples_get_buffer_size(nullptr, av_frm->pfrm->channels, av_frm->pfrm->nb_samples, (enum AVSampleFormat)av_frm->pfrm->format, 1);
// 			if (m_amrender->cached() >= buff_size)
// 				delay_until_next_wake += 0.005L;
			m_amrender->onMFrm(av_frm);
			int offset = 0;
			std::this_thread::sleep_for(AT::micros_t((int)(delay_until_next_wake * 1000000 - offset)));		
		}
	});
#endif
}

double Mplayer::decodeClock()
{
	double delta = (av_gettime() - m_current_pts_time) / 1000000.0;//s
	return m_current_pts + delta;//ts+调用该函数时增量.
}

double Mplayer::getSyncAdjustedPtsDiff(double pts, double pts_diff)
{
	double new_pts_diff = pts_diff;
	double sync_time = decodeClock();// m_sync_clock->syncClock();
	double diff = pts - sync_time;//s 音视频时间戳的差值.负数表示视频慢了，正数表示视频快了。
	double sync_threshold = (pts_diff > AV_SYNC_THRESHOLD)
		? pts_diff : AV_SYNC_THRESHOLD;
	//设置同步精度，最小10ms，最大是视频两次时间戳的差值，也就是duratino，如40ms 23ms。

	if (fabs(diff) < AV_NOSYNC_THRESHOLD) //时间戳相差不超过10s进行同步.
	{
		if (diff <= -sync_threshold) {//视频比音频慢了10-40ms。
			new_pts_diff = 0;//立刻刷新渲染当前视频帧。
		}
		else if (diff >= sync_threshold) {//视频比音频快了10-40ms。
			new_pts_diff = 2 * pts_diff;//多延时10-40ms。等待音频。
		}
	}
	return new_pts_diff;
}

void 
Mplayer::stopd() 
{
	if (mInit())
	{
		m_mdemuxer->stopd();
		m_vdecoder->stopd();
		m_adecoder->stopd();
		m_signal_quit = true;
		if(m_vrefesh_worker.joinable())
			m_vrefesh_worker.join();
		if (m_arefesh_worker.joinable())
			m_arefesh_worker.join();
		m_vmrender->stopd();
		m_amrender->stopd();
	}
}

void Mplayer::pause(bool pauseflag)
{
	if (mInit())
	{
		m_amrender->pause(pauseflag);
		m_vmrender->pause(pauseflag);
	}
}

void Mplayer::seekp(int64_t seektp)
{
	if (mInit())
	{
		return m_mdemuxer->seekp(seektp);
	}
}

int64_t Mplayer::durats(void)
{
	if (mInit())
	{
		return m_mdemuxer->durats();
	}
	return -1;
}


// Demuxer->Decoder
void
Mplayer::onPacket(std::shared_ptr<MPacket> av_pkt)  
{
	m_mdemuxer->pause((m_adecoder->Q_size() >= MAX_AUDIO_Q_SIZE)
				   || (m_vdecoder->Q_size() >= MAX_VIDEO_Q_SIZE));

	if (av_pkt->type == AVMEDIA_TYPE_AUDIO)
	{		
		if (!CHK_PROPERTY(av_pkt->prop, P_PAUS))
			m_adecoder->onMPkt(av_pkt);
		dbg("audio: :Q_size()=%d prop=%lld, type=%d upts=%g s\n", m_adecoder->Q_size(), av_pkt->prop, av_pkt->type, av_pkt->upts);
	}

	if (av_pkt->type == AVMEDIA_TYPE_VIDEO)
	{	
		if (!CHK_PROPERTY(av_pkt->prop, P_PAUS))
			m_vdecoder->onMPkt(av_pkt);
		dbg("video:Q_size()=%d  prop=%lld, type=%d upts=%g s\n", m_vdecoder->Q_size(), av_pkt->prop, av_pkt->type, av_pkt->upts);
	}
}

// Decoder->Mrender
void 
Mplayer::onMFrame(std::shared_ptr<MRframe> av_frm)  
{
	m_adecoder->pause((m_arender_Q.size() >= MAX_AUDIO_Q_SIZE));
	m_vdecoder->pause((m_vrender_Q.size() >= MAX_VIDEO_Q_SIZE));

	if ( av_frm->type == AVMEDIA_TYPE_AUDIO )
	{
		if (!CHK_PROPERTY(av_frm->prop, P_PAUS)) 
			m_arender_Q.push(av_frm);
		dbg("audio: :Q_size()=%d prop=%lld, type=%d upts=%g s\n", m_adecoder->Q_size(), av_frm->prop, av_frm->type, av_frm->upts);
	}

	if (av_frm->type == AVMEDIA_TYPE_VIDEO)
	{
		if (!CHK_PROPERTY(av_frm->prop, P_PAUS))
			m_vrender_Q.push(av_frm);
		dbg("video: :Q_size()=%d prop=%lld, type=%d upts=%g s\n", m_adecoder->Q_size(), av_frm->prop, av_frm->type, av_frm->upts);
	}

}

// Mrender->Maneger.
void
Mplayer::onMPoint(int32_t type, double upts)  
{
	if (type == AVMEDIA_TYPE_AUDIO)
	{
		dbg("-->apts=%g\n", upts);
		//if (upts > 10) m_vmrender->pause(true); hide video.
	}
	if (type == AVMEDIA_TYPE_VIDEO)
	{
		dbg("-->vpts=%g\n", upts);
		//if (upts > 10) m_vmrender->pause(true); hide audio.
	}
}

#include <QApplication>
#include <QMainWindow>

int32_t main(int32_t argc, char *argv[])
{
	QApplication app(argc, argv);
	QWidget window1, window2;
	window1.resize(640, 360);
	window2.resize(640, 360);
	window1.show();
	window2.show();
#if 0// 1 chinese test
	const char* speakr1 = "耳机 (Bluedio Stereo)";
	const char* speakr2 = "扬声器 (Realtek High Definition Audio)";
#else
	const char* speakr1 = "";
	const char* speakr2 = "";

#endif
	std::thread([&]() 
	{
#if 1// 2 window test
		std::shared_ptr<Mplayer> mp1 = 
			std::make_shared<Mplayer>("E:\\av-test\\海绵宝宝.mp3", speakr1, (void*)window1.winId());
		std::shared_ptr<Mplayer> mp2 = 
			std::make_shared<Mplayer>("E:\\av-test\\8.mp4", speakr2, (void*)window2.winId());
// 		std::shared_ptr<Mplayer> mp2 =
// 			std::make_shared<Mplayer>("rtmp://live.hkstv.hk.lxdns.com/live/hks", speakr2, (void*)window2.winId());	
#else
		std::shared_ptr<Mplayer> mp1 =
			std::make_shared<Mplayer>("E:\\av-test\\8.mp4");
		std::shared_ptr<Mplayer> mp2 =
			std::make_shared<Mplayer>("rtmp://live.hkstv.hk.lxdns.com/live/hks");
#endif
		mp1->start();
		mp2->start();
		std::this_thread::sleep_for(std::chrono::seconds(300));
#if 1// 3 reopen test
		mp1->stopd();
		mp2->stopd();
		for (int i=0;i <1;++i)
		{
			mp1->start();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			mp1->stopd();			
			mp2->start();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			mp2->stopd();
		}
		mp1->start();
		mp2->start();
#endif

		std::this_thread::sleep_for(std::chrono::seconds(1000));
	}).detach();

	msg(" main loog exec..............................\n");
	app.exec();
	return 0;
}
