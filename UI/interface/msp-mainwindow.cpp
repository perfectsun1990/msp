
#include "msp.hpp"
#include "msp-help.hpp"
#include "ui_msp-mainwindow.h"
#include "msp-mainwindow.hpp"

const char *mediaFilter =
" (*.mp4 *.ts *.mov *.flv *.mkv *.avi *.mp3 *.ogg *.aac *.wav *.gif *.webm *.wmv);;";
const char *videoFilter =
" (*.mp4 *.ts *.mov *.flv *.mkv *.avi *.gif *.webm *.wmv);;";
const char *audioFilter =
" (*.mp3 *.aac *.ogg *.wav *.wma);;";

const QString openFilter = QObject::tr("All media Files") + mediaFilter
+ QString(QObject::tr("Video Files") + videoFilter)
+ QString(QObject::tr("Audio Files") + audioFilter) + QString(QObject::tr("All Files") + ("(*.*)"));

void MSPPlayer::test_demo(char* name_utf8)
{
	libvlc_instance_t * inst;
	libvlc_media_player_t *mp;
	libvlc_media_t *m;

	libvlc_time_t length;
	int width;
	int height;
	int wait_time = 5000;

	//libvlc_time_t length;

	/* Load the VLC engine */
	inst = libvlc_new(0, NULL);

	//Create a new item
	//Method 1:
	//m = libvlc_media_new_location (inst, "file:///F:\\movie\\cuc_ieschool.flv");
	//Screen Capture
	//m = libvlc_media_new_location (inst, "screen://");
	//Method 2:

	//char* pszMultiByte = "Holle";  //strlen(pwsUnicode)=5

	m = libvlc_media_new_path(inst, name_utf8);
	if (NULL==m)
		return;
	/* Create a media player playing environement */
	mp = libvlc_media_player_new_from_media(m);

#if defined(Q_OS_MAC)
	libvlc_media_player_set_nsobject(vlcPlayer, videoWidget->winId());
#elif defined(Q_OS_UNIX)
	libvlc_media_player_set_xwindow(vlcPlayer, videoWidget->winId());
#elif defined(Q_OS_WIN)
	//HWND* screen_hwnd = (HWND*)(ui->openGLWidget->winId());
	HWND* screen_hwnd = (HWND*)(m_helpWindow.get()->winId());
	libvlc_media_player_set_hwnd(mp, screen_hwnd/*::GetDesktopWindow()*/);
#endif
	/* No need to keep the media now */
	libvlc_media_release(m);

	// play the media_player
	libvlc_media_player_play(mp);

	//wait until the tracks are created
	Sleep(wait_time);
	length = libvlc_media_player_get_length(mp);
	width = libvlc_video_get_width(mp);
	height = libvlc_video_get_height(mp);
	printf("Stream Duration: %lld\n", length / 1000);
	printf("Resolution: %d x %d\n", width, height);
	//Let it play 
	Sleep(length - wait_time);

	// Stop playing
	libvlc_media_player_stop(mp);

	// Free the media_player
	libvlc_media_player_release(mp);

	libvlc_release(inst);
}

const char * const vlc_args[] = {
	"--verbose=0", //be much more verbose then normal for debugging purpose
	"--plugin-path=./plugins"
};

MSPPlayer::MSPPlayer(QWidget *parent) :
	QMainWindow(parent),
    ui(new Ui::MSPPlayer),
	m_vlcInst(nullptr),
	m_media(nullptr),
	m_mediaPlayer(nullptr),
	m_poller(new QTimer(this)),
	m_isPlaying(false),
	m_isPause(false)
{
	FUNC_ENTER

	ui->setupUi(this);
	ui->seekSlider->setMaximum(POSITION_RESOLUTION);
	ui->volumeSlider->setMaximum(100);

	// 1.sub windows  of mainwindow.
	m_helpWindow = std::make_shared<MSP_Help>();

	/*Create a new LibVLC instance.*/
	m_vlcInst = libvlc_new(sizeof(vlc_args)/sizeof(vlc_args[0]),vlc_args);

	// 2.sig and slot of mainwindow.
	connect(ui->playButton,		SIGNAL(clicked()), this, SLOT(on_playButtonClicked()));
	connect(ui->stopButton,		SIGNAL(clicked()), this, SLOT(on_stopButtonClicked()));
	connect(ui->lastButton,		SIGNAL(clicked()), this, SLOT(on_lastButtonClicked()));
	connect(ui->nextButton,		SIGNAL(clicked()), this, SLOT(on_nextButtonClicked()));
	connect(ui->actionOpenFile, SIGNAL(triggered(bool)), this, SLOT(on_actionOpenFile()));
	connect(ui->actionUpdate,	SIGNAL(triggered(bool)), this, SLOT(on_actionHelp()));
	connect(ui->actionAbout,	SIGNAL(triggered(bool)), this, SLOT(on_actionHelp()));
	connect(ui->actionExit,		SIGNAL(triggered(bool)), this, SLOT(on_actionExit()));
	connect(ui->actionFullScreen, SIGNAL(triggered(bool)), this, SLOT(on_actionFullScreen(bool)));	
	connect(ui->actionOriginal, SIGNAL(triggered(bool)), this, SLOT(on_actionScale(bool)));
	connect(ui->seekSlider,		SIGNAL(sliderMoved(int)), this, SLOT(on_seekPosition(int)));
	connect(ui->seekSlider,		SIGNAL(clicked()), this, SLOT(on_seekPosition(int)));
	connect(ui->volumeSlider,   SIGNAL(sliderMoved(int)), this, SLOT(on_seekVolume(int)));
	connect(ui->volumeSlider,   SIGNAL(clicked()), this, SLOT(on_seekVolume(int)));	
	connect(m_poller, SIGNAL(timeout()), this, SLOT(updateInterface()));
	m_poller->start(100);//start timer to trigger every 100 ms the updateInterface slot 
}

MSPPlayer::~MSPPlayer()
{
	FUNC_ENTER
	/* Stop playing */
	if (nullptr != m_mediaPlayer)
	{
		libvlc_media_player_stop(m_mediaPlayer);
		/* Free the media_player */
		libvlc_media_player_release(m_mediaPlayer);
		m_mediaPlayer = nullptr;
	}
	if (nullptr != m_media)
	{
		libvlc_media_release(m_media);
		m_media = nullptr;
	}
	if (nullptr != m_vlcInst)
	{
		libvlc_release(m_vlcInst);
		m_vlcInst = nullptr;
	}
	delete m_poller;
    delete ui;
}

/************************************************************************/
/*                  	   	  buttons									*/
/************************************************************************/
void
MSPPlayer::on_playButtonClicked()
{
	FUNC_ENTER

	if (m_curPlayingFile.isEmpty())
		on_actionOpenFile();

	if (m_isPlaying)
	{
		libvlc_media_player_set_pause(m_mediaPlayer, m_isPause = !m_isPause);
		if (m_isPause) {
			ui->playButton->setText(QObject::tr("play"));
		}
		else {
			ui->playButton->setText(QObject::tr("pause"));
		}
		return;
	}	

	/* Create a new LibVLC media descriptor */
	m_media = libvlc_media_new_path(m_vlcInst, m_curPlayingFile.toStdString().data());
	m_mediaPlayer = libvlc_media_player_new(m_vlcInst);
	libvlc_media_player_set_media(m_mediaPlayer, m_media);
	/* Get our media instance to use our window */
#if		defined(Q_OS_WIN)
	libvlc_media_player_set_hwnd(m_mediaPlayer, reinterpret_cast<void*>(ui->openGLWidget->winId()));
#elif	defined(Q_OS_MAC)
	libvlc_media_player_set_nsobject(m_mediaPlayer, reinterpret_cast<void*>(ui->openGLWidget->winId()));
#endif
	/* Play */
	libvlc_media_player_play(m_mediaPlayer);
	ui->playButton->setText(QObject::tr("pause"));
	m_isPlaying = true;
}

void
MSPPlayer::on_stopButtonClicked()
{
	FUNC_ENTER
	/* Stop playing */
	if (nullptr != m_mediaPlayer)
	{
		libvlc_media_player_stop(m_mediaPlayer);
		/* Free the media_player */
		libvlc_media_player_release(m_mediaPlayer);
		m_mediaPlayer = nullptr;
	}	
	m_isPlaying = false;

	ui->playButton->setText(QObject::tr("play"));
	ui->seekSlider->setValue(0);
}

void
MSPPlayer::on_lastButtonClicked()
{
	FUNC_ENTER

	if (!m_curPlayingList.isEmpty())
	{	
		qDebug() << "size=" << m_curPlayingList.size();
		for (int32_t i = 0; i < m_curPlayingList.size(); ++i)
		{
			if (!m_curPlayingList[i].compare(m_curPlayingFile))
			{				
				i = (0 == i)?m_curPlayingList.size():i;				
				m_curPlayingFile = m_curPlayingList[(i-1) % m_curPlayingList.size()];
				break;
			}
		}
		on_stopButtonClicked();
		on_playButtonClicked();
	}
}

void 
MSPPlayer::on_nextButtonClicked()
{
	FUNC_ENTER

	if (!m_curPlayingList.isEmpty())
	{
		for (int32_t i=0; i < m_curPlayingList.size(); ++i)
		{
			qDebug() <<"size=" <<m_curPlayingList.size();
			if (!m_curPlayingList[i].compare(m_curPlayingFile))
			{
				m_curPlayingFile = m_curPlayingList[(i+1) % m_curPlayingList.length()];
				break;
			}
		}
		on_stopButtonClicked();
		on_playButtonClicked();
	}
}

/************************************************************************/
/*                  	   	  actions									*/
/************************************************************************/
void
MSPPlayer::on_actionOpenFile()
{
	QStringList openFileNames = QFileDialog::getOpenFileNames(
		nullptr, QObject::tr("Open Files"), ".", openFilter, nullptr, 0);

	if (!openFileNames.isEmpty())
	{
		for each (QString new_item in openFileNames)
		{
#ifdef WIN32
			new_item.replace("/", "\\");
#endif
			bool addToPlayList = true;
			for each (QString old_item in m_curPlayingList)
			{
				if (!old_item.compare(new_item))
				{
					addToPlayList = false;
					break;
				}				
			}
			if (addToPlayList)
				m_curPlayingList.append(new_item);
		}	
		m_curPlayingFile = m_curPlayingList[0];
	}
	
	qDebug() << "@@@m_curPlayingList=" << m_curPlayingList << "@@@m_curPlayingFile" << m_curPlayingFile ;

}

void
MSPPlayer::on_seekVolume(int vol)
{
	if (nullptr != m_mediaPlayer)
	{
		libvlc_audio_set_volume(m_mediaPlayer, vol);
	}	
}

void
MSPPlayer::on_seekPosition(int pos)
{
	if (nullptr != m_mediaPlayer)
	{
		libvlc_media_t *curMedia = libvlc_media_player_get_media(m_mediaPlayer);
		if (curMedia == NULL)
			return;
		float new_pos = (float)(pos) / (float)POSITION_RESOLUTION;
		libvlc_media_player_set_position(m_mediaPlayer, new_pos);
	}
}

void
MSPPlayer::on_actionScale(bool flag)
{
	if (m_mediaPlayer)
	{
		libvlc_video_set_scale(m_mediaPlayer, flag);	
	}
}

void
MSPPlayer::on_actionFullScreen(bool flag)
{
	if (m_mediaPlayer)
	{
		//libvlc_video_set_scale(m_mediaPlayer, flag);
		libvlc_set_fullscreen(m_mediaPlayer, flag);
	}
}
void
MSPPlayer::on_actionHelp()
{
	m_helpWindow->show();
}

void 
MSPPlayer::on_actionExit()
{
	this->close();
}


void MSPPlayer::updateInterface()
{
	if (!m_isPlaying)
		return;

	// It's possible that the vlc doesn't play anything
	// so check before
	libvlc_media_t *curMedia = libvlc_media_player_get_media(m_mediaPlayer);
	if (curMedia == NULL)
		return;
	float pos = libvlc_media_player_get_position(m_mediaPlayer);
	int siderPos = (int)(pos*(float)(POSITION_RESOLUTION));	
	int volume = libvlc_audio_get_volume(m_mediaPlayer);
	ui->seekSlider->setValue(siderPos);
	ui->volumeSlider->setValue(volume);
	ui->timeLable->setText("");
}

