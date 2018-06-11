
#include "pubcore.hpp"
#include "synchro.hpp"

std::shared_ptr<ISynchro> ISynchro::create(
	std::shared_ptr<ISynchroObserver> observer)
{
	return std::make_shared<MediaSynchro>(observer);
}

MediaSynchro::MediaSynchro(
	std::shared_ptr<ISynchroObserver> observer)
	: m_observer(observer)
{
	SET_STATUS(m_status,E_INVALID);
	
	m_config = std::make_shared<MSynConfig>();
	
	m_acache = std::make_shared<MRframe>();
	m_acache->type = AVMEDIA_TYPE_AUDIO;
	SET_PROPERTY(m_acache->prop, P_PAUS);
	m_vcache = std::make_shared<MRframe>();
	m_vcache->type = AVMEDIA_TYPE_VIDEO;
	SET_PROPERTY(m_vcache->prop, P_PAUS);

	SET_STATUS(m_status, E_INITRES);
}

MediaSynchro::~MediaSynchro()
{
	stopd(true);
}

void 
MediaSynchro::start(void)
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);

#if 1
	m_vsychro_worker = std::thread([&]()
	{
		m_current_ptsv = 0;
		m_current_pts_timev = 0;
		m_previous_ptsv = 0;
		m_previous_pts_diffv = 40e-3;
		m_start_ptsv = 0;
		m_predicted_ptsv = 0.0;
		m_first_framev = true;
		m_next_wakev = 0.0;
		m_vsychro_Q.clear();
		m_signalVquit = false;
		while (!m_signalVquit)
		{
			std::shared_ptr<MRframe> av_frm = nullptr;
			if (!m_vsychro_Q.try_peek(av_frm))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			m_vsychro_Q.popd(av_frm);

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
			if (m_config->sync_type != 1)
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
			if (!m_observer.expired())
				m_observer.lock()->onSync(av_frm);
			int offset = 0;
			std::this_thread::sleep_for(AT::micros_t((int)(delay_until_next_wake * 1000000 - offset)));
#endif	
		}
		msg("video sync thread....exit....\n");
	});


	m_asychro_worker = std::thread([&]()
	{
		m_current_pts = 0;
		m_current_pts_time = 0;
		m_previous_pts = 0;
		m_previous_pts_diff = 40e-3;
		m_start_pts = 0;
		m_predicted_pts = 0.0;
		m_first_frame = true;
		m_next_wake = 0.0;
		m_asychro_Q.clear();
		m_signalAquit = false;
		while (!m_signalAquit)
		{
			//pop..
			std::shared_ptr<MRframe> av_frm = nullptr;
			if (!m_asychro_Q.try_peek(av_frm))
			{
				std::this_thread::sleep_for(std::chrono::milliseconds(10));
				continue;
			}
			m_asychro_Q.popd(av_frm);

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
			if (m_config->sync_type != 0)
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

			if (!m_observer.expired())
				m_observer.lock()->onSync(av_frm);
			int offset = 0;
			std::this_thread::sleep_for(AT::micros_t((int)(delay_until_next_wake * 1000000 - offset)));
		}
		msg("audio sync thread....exit....\n");
	});
#endif

	SET_STATUS(m_status, E_STARTED);
}

void
MediaSynchro::stopd(bool stop_quik)
{
	CHK_RETURN(E_STARTED != status());
	SET_STATUS(m_status, E_STOPING);

	m_signalAquit = true;
	if (m_asychro_worker.joinable()) m_asychro_worker.join();
	m_signalVquit = true;
	if (m_vsychro_worker.joinable()) m_vsychro_worker.join();

	SET_STATUS(m_status, E_STOPPED);
}

void
MediaSynchro::pause(bool pauseflag)
{
	MSynConfig cfg;
	config(&cfg);
	cfg.pauseflag = pauseflag;
	update(&cfg);
}

void
MediaSynchro::config(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config)
		*((MSynConfig*)config) = *m_config;
}

void
MediaSynchro::update(void* config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	MSynConfig* cfg = (MSynConfig*)config;
	if (nullptr != config)
	{
		*m_config = *cfg;
	}
}

STATUS 
MediaSynchro::status(void)
{
	return m_status;
}

int32_t MediaSynchro::Q_size(void)
{
	return m_asychro_Q.size();
}

void MediaSynchro::updateSyncType(int32_t sync_type)
{
	MSynConfig cfg;
	config(&cfg);
	cfg.sync_type = sync_type;
	update(&cfg);
}


void
MediaSynchro::onMFrm(std::shared_ptr<MRframe> av_frm)
{
	if (av_frm->type == AVMEDIA_TYPE_AUDIO)
	{
		if (CHK_PROPERTY(av_frm->prop, P_SEEK)) {
			m_asychro_Q.clear();
			msg("sychro: clear Que...\n");
		}
		if (CHK_PROPERTY(av_frm->prop, P_PAUS)) {
			dbg("sychro: pause Prm...\n");
			return;
		}
		if (!m_signalAquit)
			m_asychro_Q.push(av_frm);
	}
	if (av_frm->type == AVMEDIA_TYPE_VIDEO)
	{
		if (CHK_PROPERTY(av_frm->prop, P_SEEK)) {
			m_vsychro_Q.clear();
			msg("sychro: clear Que...\n");
		}
		if (CHK_PROPERTY(av_frm->prop, P_PAUS)) {
			dbg("sychro: pause Prm...\n");
			return;
		}
		if (!m_signalVquit)
			m_vsychro_Q.push(av_frm);
	}
}

double MediaSynchro::decodeClock()
{
	double delta = (av_gettime() - m_current_pts_time) / 1000000.0;//s
	return m_current_pts + delta;//ts+调用该函数时增量.
}

double MediaSynchro::getSyncAdjustedPtsDiff(double pts, double pts_diff)
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
