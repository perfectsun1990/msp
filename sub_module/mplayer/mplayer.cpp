
#include "pubcore.hpp"
#include "mplayer.hpp"

static int32_t g_mplayer_ssidNo = 0;

std::shared_ptr<IMplayer> IMplayer::create(const char * inputf, const char* speakr, const void* window,
	std::shared_ptr<IMplayerObserver> observer) {
	return std::make_shared<Mplayer>(inputf, speakr, window, observer);
}

Mplayer::Mplayer(const char* inputf, const char* speakr, const void* window,
	std::shared_ptr<IMplayerObserver> observer):m_observer(observer)
{
	SET_STATUS(m_status, E_INVALID);

	m_config = std::make_shared<MplyerCfg>();
	m_config->rd_window = window;
	m_ssidNo = g_mplayer_ssidNo++;
	char tmp_uf8[512] = { 0 };
	Ansi2Utf8(inputf,		tmp_uf8, sizeof(tmp_uf8));
	m_config->inputpath = tmp_uf8;
	Ansi2Utf8(speakr,	tmp_uf8, sizeof(tmp_uf8));
	m_config->rd_speakr = tmp_uf8;
	
	SET_STATUS(m_status, E_INITRES);
}

Mplayer::~Mplayer() 
{
	stopd();
}

STATUS 
Mplayer::status(void)
{
	return STATUS(m_status);
}

void
Mplayer::config(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config)
		*((MplyerCfg*)config) = *m_config;
}

int32_t 
Mplayer::ssidNo(void)
{
	return m_ssidNo;
}

void
Mplayer::update(void * config)
{
	std::lock_guard<std::mutex> locker(m_cmutex);
	if (nullptr != config) {
		*m_config = *((MplyerCfg*)config);
		updateAttributes();
	}
}

bool
Mplayer::mInit(void)
{
	if (!m_signal_init)
	{
		m_vdecoder1 = IDecoderFactory::createVideoDecoder(shared_from_this());
		m_adecoder1 = IDecoderFactory::createAudioDecoder(shared_from_this());
		m_menmuxer = IEnmuxer::create("E:\\test.flv",shared_from_this());

		m_mdemuxer = IDemuxer::create(m_config->inputpath.c_str(), shared_from_this());
		m_msynchro = ISynchro::create(SYNC_AUDIO_MASTER, shared_from_this());
		m_vdecoder = IDecoderFactory::createVideoDecoder(shared_from_this());
		m_adecoder = IDecoderFactory::createAudioDecoder(shared_from_this());
		m_vencoder = IEncoderFactory::createVideoEncoder(shared_from_this());
		m_aencoder = IEncoderFactory::createAudioEncoder(shared_from_this());
		m_vmrender = IMrenderFactory::createVideoMrender(m_config->rd_window,		 shared_from_this());
		m_amrender = IMrenderFactory::createAudioMrender(m_config->rd_speakr.c_str(),shared_from_this());
		m_signal_init = (E_INVALID != m_mdemuxer->status() 
			&& E_INVALID != m_msynchro->status()
			&& E_INVALID != m_vdecoder->status()
			&& E_INVALID != m_adecoder->status()
			&& E_INVALID != m_vencoder->status()
			&& E_INVALID != m_aencoder->status()
			&& E_INVALID != m_vmrender->status()
			&& E_INVALID != m_amrender->status());
		assert(m_signal_init);
	}
	return m_signal_init;
}

void 
Mplayer::seekp(int64_t seektp)
{
	if (mInit()) {
		return m_mdemuxer->seekp(seektp);
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////

void 
Mplayer::updateAttributes(void)
{
}

double 
Mplayer::durats(void)
{
	if (mInit()) {
		return m_mdemuxer->durats();
	}
	return -1.0;
}

void
Mplayer::pause(bool pauseflag)
{
	if (mInit()) {
		m_amrender->pause(pauseflag);
		m_vmrender->pause(pauseflag);
	}
}

void
Mplayer::start() 
{
	CHK_RETURN(E_INITRES != status() && E_STOPPED != status());
	SET_STATUS(m_status, E_STRTING);
	if (mInit()) {
		m_vmrender->start();		
		m_amrender->start();
		m_msynchro->start();
		m_vdecoder->start();
		m_adecoder->start();
		m_aencoder->start();
		m_vencoder->start();
		m_mdemuxer->start();

		m_vdecoder1->start();
		m_adecoder1->start();
		m_menmuxer->start();
	}
	SET_STATUS(m_status, E_STARTED);
}

void 
Mplayer::stopd() 
{
	CHK_RETURN(E_STOPPED == status() || E_STOPING == status());
	SET_STATUS(m_status, E_STOPING);
	if (mInit()) {
		m_mdemuxer->stopd();
		m_vencoder->stopd();
		m_aencoder->stopd();
		m_vdecoder->stopd();
		m_adecoder->stopd();
		m_msynchro->stopd();
		m_vmrender->stopd();
		m_amrender->stopd();

		m_vdecoder1->stopd();
		m_adecoder1->stopd();
		m_menmuxer->stopd();
	}
	SET_STATUS(m_status, E_STOPPED);
}


// Demuxer->Decoder
void
Mplayer::onDemuxerPackt(std::shared_ptr<MPacket> av_pkt)
{
	m_mdemuxer->pause((m_adecoder->Q_size() >= MAX_AUDIO_Q_SIZE)
				   || (m_vdecoder->Q_size() >= MAX_VIDEO_Q_SIZE));
	if (av_pkt->type == AVMEDIA_TYPE_AUDIO){
		if (m_mdemuxer->ssidNo() == av_pkt->ssid) {
			if (!CHK_PROPERTY(av_pkt->prop, P_PAUS))
				m_adecoder->pushPackt(av_pkt);
		}
		dbg("audio: :Q_size()=%d prop=%lld, type=%d upts=%g s\n", m_adecoder->Q_size(), av_pkt->prop, av_pkt->type, av_pkt->upts);
	}

	if (av_pkt->type == AVMEDIA_TYPE_VIDEO){	
		if (m_mdemuxer->ssidNo() == av_pkt->ssid) {
			if (!CHK_PROPERTY(av_pkt->prop, P_PAUS))
				m_vdecoder->pushPackt(av_pkt);
		}
		dbg("video:Q_size()=%d  prop=%lld, type=%d upts=%g s\n", m_vdecoder->Q_size(), av_pkt->prop, av_pkt->type, av_pkt->upts);
	}
	//remuxer.
	//m_menmuxer->pushPackt(av_pkt->clone());
}

// Decoder->Synchro
void 
Mplayer::onDecoderFrame(std::shared_ptr<MRframe> av_frm)
{
	if (av_frm->type == AVMEDIA_TYPE_AUDIO) {
		if (m_adecoder->ssidNo() == av_frm->ssid) {
			m_adecoder->pause((m_msynchro->Q_size(AVMEDIA_TYPE_AUDIO) >= MAX_AUDIO_Q_SIZE));
			m_msynchro->pushFrame(av_frm);
		}
		dbg("audio: :Q_size()=%d prop=%lld, type=%d upts=%g s\n", m_amrender->Q_size(), av_frm->prop, av_frm->type, av_frm->upts);
	}
	if (av_frm->type == AVMEDIA_TYPE_VIDEO) {
		if (m_vdecoder->ssidNo() == av_frm->ssid) {
			m_vdecoder->pause((m_msynchro->Q_size(AVMEDIA_TYPE_VIDEO) >= MAX_VIDEO_Q_SIZE));
			m_msynchro->pushFrame(av_frm);
		}
		dbg("video: :Q_size()=%d prop=%lld, type=%d upts=%g s\n", m_vmrender->Q_size(), av_frm->prop, av_frm->type, av_frm->upts);
	}
}

// Synchro->Mrender
void
Mplayer::onSynchroFrame(std::shared_ptr<MRframe> av_frm)
{
	m_msynchro->pause((m_amrender->Q_size() >= MAX_AUDIO_Q_SIZE)
		|| (m_vmrender->Q_size() >= MAX_VIDEO_Q_SIZE));

	if (av_frm->type == AVMEDIA_TYPE_AUDIO) {
		if (m_msynchro->ssidNo() == av_frm->ssid) {
			m_amrender->pushFrame(av_frm);
		}
		dbg("audio: :Q_size()=%d prop=%lld, type=%d upts=%g s\n", m_amrender->Q_size(), av_frm->prop, av_frm->type, av_frm->upts);
	}
	if (av_frm->type == AVMEDIA_TYPE_VIDEO) {
		if (m_msynchro->ssidNo() == av_frm->ssid) {
			m_vmrender->pushFrame(av_frm);
		}
		dbg("video: :Q_size()=%d prop=%lld, type=%d upts=%g s\n", m_vmrender->Q_size(), av_frm->prop, av_frm->type, av_frm->upts);
	}
}

// Mrender->Manager.
void
Mplayer::onMRenderFrame(std::shared_ptr<MRframe> av_frm)
{
	if (av_frm->type == AVMEDIA_TYPE_AUDIO){
		if (m_amrender->ssidNo() == av_frm->ssid) {
			//NOTE: for smoth play,we don't pause render.
			m_aencoder->pushFrame(av_frm);
		}
		dbg("-->apts=%g\n", av_frm->upts);
	}
	if (av_frm->type == AVMEDIA_TYPE_VIDEO){
		if (m_vmrender->ssidNo() == av_frm->ssid) {
			//NOTE: for smoth play,we don't pause render.
			m_vencoder->pushFrame(av_frm);
		}
		dbg("-->vpts=%g\n", av_frm->upts);
	}
	if (!CHK_PROPERTY(av_frm->prop, P_PAUS) && !m_observer.expired())
			m_observer.lock()->onCurRenderPts(m_ssidNo, av_frm->type, av_frm->upts);
}

// Encoder->Enmuxer.
void Mplayer::onEncoderPackt(std::shared_ptr<MPacket> av_pkt)
{
	if (av_pkt->type == AVMEDIA_TYPE_AUDIO) {
		if (m_aencoder->ssidNo() == av_pkt->ssid) {
			m_menmuxer->pushPackt(av_pkt);
		}
		dbg("audio: :Q_size()=%d prop=%lld, type=%d upts=%g s\n", m_aencoder->Q_size(), av_pkt->prop, av_pkt->type, av_pkt->upts);
	}

	if (av_pkt->type == AVMEDIA_TYPE_VIDEO) {
		if (m_vencoder->ssidNo() == av_pkt->ssid) {
			m_menmuxer->pushPackt(av_pkt);
		}
		dbg("video:Q_size()=%d  prop=%lld, type=%d upts=%g s\n", m_vencoder->Q_size(), av_pkt->prop, av_pkt->type, av_pkt->upts);
	}
}

// Encoder->Enmuxer.
void Mplayer::onEnmuxerPackt(std::shared_ptr<MPacket> av_pkt)
{

}
//////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef UNIT_TEST

#define  TEST_WINDOW	1
#define  TEST_REOPEN	0
#define  TEST_CHINESE	0
#define  TEST_SEEK_PAUSE 0
#if TEST_WINDOW
#include <QApplication>
#include <QMainWindow>
#endif

int32_t main(int32_t argc, char *argv[])
{
#if TEST_WINDOW
	QApplication app(argc, argv);
	QWidget window1, window2;
	window1.resize(640, 360);
	window2.resize(640, 360);
	window1.show();
	window2.show();
#endif

#if TEST_CHINESE// 1 chinese test
	const char* speakr1 = "耳机 (Bluedio Stereo)";
	const char* speakr2 = "扬声器 (Realtek High Definition Audio)";
#else
	const char* speakr1 = "";
	const char* speakr2 = "";
#endif
	std::thread([&]() 
	{
#if TEST_WINDOW// 2 window test
		std::shared_ptr<Mplayer> mp1 =
			std::make_shared<Mplayer>("rtmp://202.69.69.180:443/webcast/bshdlive-pc", speakr1, (void*)window1.winId());
		std::shared_ptr<Mplayer> mp2 = 
			std::make_shared<Mplayer>("E:\\av-test\\8.mp4", speakr2, (void*)window2.winId());
#else
		std::shared_ptr<Mplayer> mp1 =
			std::make_shared<Mplayer>("E:\\av-test\\8.mp4");
		std::shared_ptr<Mplayer> mp2 =
			std::make_shared<Mplayer>("E:\\av-test\\8.mp4");
#endif
		mp1->start();
		//mp2->start();
		//std::this_thread::sleep_for(std::chrono::seconds(30));
		//mp1->stopd();
#if		TEST_SEEK_PAUSE
		for (int i = 1; ; i++) {
			std::this_thread::sleep_for(std::chrono::milliseconds(1000));
			msg("set %d\n", i % 2);
			//mp2->pause(i%2);
			mp2->seekp((i % 10) * 20 * 1000000);// t:us
		}
#endif
#if TEST_REOPEN// 3 reopen thread_safe test
		std::this_thread::sleep_for(std::chrono::seconds(5));
		std::thread([&]() {
			for (int32_t i = 0; i < 3; ++i)
			{
				mp1->stopd();
				mp1->start();
				mp2->stopd();
				mp2->start();
			}
		}).detach();
#endif
		std::this_thread::sleep_for(std::chrono::seconds(1000));
	}).detach();

	msg(" main loog exec..............................\n");
#if TEST_WINDOW
	app.exec();
#else
	std::this_thread::sleep_for(std::chrono::seconds(1000));
#endif
	return 0;
}

#endif


