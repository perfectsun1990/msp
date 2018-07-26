
#include "pubcore.hpp"
#include "mplayer.hpp"
Mplayer::Mplayer(const char* url, const char* speakr, const void* window)
	: m_window(window)
{
	SET_STATUS(m_status, E_INVALID);
	char tmp_uf8[512] = { 0 };
	Ansi2Utf8(url,		tmp_uf8, sizeof(tmp_uf8));
	m_inputf = tmp_uf8;
	Ansi2Utf8(speakr,	tmp_uf8, sizeof(tmp_uf8));
	m_speakr = tmp_uf8;
	SET_STATUS(m_status, E_INITRES);
}

Mplayer::~Mplayer() 
{
	stopd();
}

STATUS Mplayer::status(void)
{
	return STATUS(m_status);
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
		m_mdemuxer = IDemuxer::create(m_inputf.c_str(), shared_from_this());
		m_msynchro = ISynchro::create(shared_from_this());
		m_vdecoder = IDecoderFactory::createVideoDecoder(shared_from_this());
		m_adecoder = IDecoderFactory::createAudioDecoder(shared_from_this());
		m_vmrender = IMrenderFactory::createVideoMrender(m_window,			shared_from_this());
		m_amrender = IMrenderFactory::createAudioMrender(m_speakr.c_str(),	shared_from_this());
		m_signal_init = (E_INVALID != m_mdemuxer->status() 
			&& E_INVALID != m_msynchro->status()
			&& E_INVALID != m_vdecoder->status()
			&& E_INVALID != m_adecoder->status() 
			&& E_INVALID != m_vmrender->status()
			&& E_INVALID != m_amrender->status());
		assert(m_signal_init);
	}
	return m_signal_init;
}

void
Mplayer::start() 
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);
	if (mInit())
	{
		m_vmrender->start();		
		m_amrender->start();
		m_msynchro->start();
		m_vdecoder->start();
		m_adecoder->start();
		m_mdemuxer->start();
	}
	SET_STATUS(m_status, E_STARTED);
}

void 
Mplayer::stopd() 
{
	CHK_RETURN(E_STARTED != status());
	SET_STATUS(m_status, E_STOPING);
	if (mInit())
	{
		m_mdemuxer->stopd();
		m_vdecoder->stopd();
		m_adecoder->stopd();
		m_msynchro->stopd();
		m_vmrender->stopd();
		m_amrender->stopd();
	}
	SET_STATUS(m_status, E_STOPPED);
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
Mplayer::onMPkt(std::shared_ptr<MPacket> av_pkt)
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

// Decoder->Synchro
void 
Mplayer::onMFrm(std::shared_ptr<MRframe> av_frm)
{
	m_adecoder->pause((m_msynchro->Q_size() >= MAX_AUDIO_Q_SIZE)
	|| (m_msynchro->Q_size() >= MAX_VIDEO_Q_SIZE));
	if (!CHK_PROPERTY(av_frm->prop, P_PAUS))
		m_msynchro->onMFrm(av_frm);
}

// Synchro->Mrender
void
Mplayer::onSync(std::shared_ptr<MRframe> av_frm)
{
	if (av_frm->type == AVMEDIA_TYPE_AUDIO)
	{
		if (!CHK_PROPERTY(av_frm->prop, P_PAUS))
			m_amrender->onMFrm(av_frm);
		dbg("audio: :Q_size()=%d prop=%lld, type=%d upts=%g s\n", m_amrender->Q_size(), av_frm->prop, av_frm->type, av_frm->upts);
	}

	if (av_frm->type == AVMEDIA_TYPE_VIDEO)
	{
		if (!CHK_PROPERTY(av_frm->prop, P_PAUS))
			m_vmrender->onMFrm(av_frm);
		dbg("video: :Q_size()=%d prop=%lld, type=%d upts=%g s\n", m_amrender->Q_size(), av_frm->prop, av_frm->type, av_frm->upts);
	}
}

// Mrender->Maneger.
void
Mplayer::onMPts(int32_t type, double upts)
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
			std::make_shared<Mplayer>("rtmp://live.hkstv.hk.lxdns.com/live/hks", speakr1, (void*)window1.winId());
		std::shared_ptr<Mplayer> mp2 = 
			std::make_shared<Mplayer>("E:\\av-test\\6构件夹的设置.avi", speakr2, (void*)window2.winId());
#else
		std::shared_ptr<Mplayer> mp1 =
			std::make_shared<Mplayer>("E:\\av-test\\8.mp4");
		std::shared_ptr<Mplayer> mp2 =
			std::make_shared<Mplayer>("rtmp://live.hkstv.hk.lxdns.com/live/hks");
#endif
		//mp1->start();
		mp2->start();
#if 0// 3 reopen thread_safe test
		for (int32_t i=0 ;i<5; ++i)
		{
			mp1->stopd();
			std::this_thread::sleep_for(std::chrono::seconds(3));
			mp1->start();
			mp2->stopd();
			mp2->start();
		}
#endif
		std::this_thread::sleep_for(std::chrono::seconds(1000));
	}).detach();

	msg(" main loog exec..............................\n");
	app.exec();
	return 0;
}
