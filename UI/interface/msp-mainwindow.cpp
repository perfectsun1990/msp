
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

std::shared_ptr<std::string>
getFmtTime(int64_t sec)
{
	char buf[512] = {0};
	sprintf(buf,"%02lld:%02lld:%02lld", sec / 3600, (sec / 60) % 60, sec % 60);
	std::shared_ptr<std::string> p = std::make_shared<std::string>(buf);
	return p;
}

MSPPlayer::MSPPlayer(QWidget *parent) :
	QMainWindow(parent),
    ui(std::make_shared<Ui::MSPPlayer>())
{
	FUNC_ENTER
	ui->setupUi(this);
	ui->seekSlider->setMaximum(POS_RESOLUTION);
	ui->volmSlider->setMaximum(100);

	/* 1.Create mediacore instances .*/
	const char * const vlc_args[] = {
		"--verbose=0", //be much more verbose then normal for debugging purpose
	};
	m_vInst = libvlc_new(sizeof(vlc_args) / sizeof(vlc_args[0]), vlc_args);

	// 2.Addon sub widgets.
	m_helpWindow = std::make_shared<MSP_Help>();

	// 3.Qtimer function and connect slots.
	connect(ui->actionUpdate,	SIGNAL(triggered(bool)), this, SLOT(on_actionHelp_triggered()));
	m_poller = std::make_shared<QTimer>(this);
	connect(m_poller.get(), &QTimer::timeout, this, [&]() 
	{
		if (!m_isPlaying)
			return;
		// It's possible that the vlc doesn't play anything so check before
		libvlc_media_t *curMedia = libvlc_media_player_get_media(m_mediaPlayer);
		if (curMedia == NULL)
			return;
		ui->seekSlider->setValue((int)(libvlc_media_player_get_position(m_mediaPlayer)*(float)(POS_RESOLUTION)));
		ui->volmSlider->setValue(libvlc_audio_get_volume(m_mediaPlayer));
		m_cur_time = libvlc_media_player_get_time(m_mediaPlayer);
		m_duration = libvlc_media_player_get_length(m_mediaPlayer);
		if ( abs(m_cur_time - m_duration) <= 200 ){
			on_stopButton_clicked();
			ui->timeLable->setText(getFmtTime(0)->c_str());
		}else {
			ui->timeLable->setText(getFmtTime(m_cur_time / 1000)->c_str());
		}
	});
	m_poller->start(100);//start timer to trigger every 100 ms.
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
	if (nullptr != m_vInst)
	{
		libvlc_release(m_vInst);
		m_vInst = nullptr;
	}
}

/************************************************************************/
/*                  	   	  buttons									*/
/************************************************************************/
void
MSPPlayer::on_playButton_clicked()
{
	FUNC_ENTER

	if (m_curPlayingFile.isEmpty())
		on_actionOpen_triggered();

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
	m_media = libvlc_media_new_path(m_vInst, m_curPlayingFile.toUtf8().data());
	m_mediaPlayer = libvlc_media_player_new(m_vInst);
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
MSPPlayer::on_stopButton_clicked()
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
MSPPlayer::on_lastButton_clicked()
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
		on_stopButton_clicked();
		on_playButton_clicked();
	}
}

void 
MSPPlayer::on_nextButton_clicked()
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
		on_stopButton_clicked();
		on_playButton_clicked();
	}
}

/************************************************************************/
/*                  	   	  actions									*/
/************************************************************************/
void
MSPPlayer::on_actionOpen_triggered()
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
MSPPlayer::on_volmSlider_sliderMoved(int vol)
{
	if (nullptr != m_mediaPlayer)
	{
		libvlc_audio_set_volume(m_mediaPlayer, vol);
	}	
}

void
MSPPlayer::on_seekSlider_sliderMoved(int pos)
{
	if (nullptr != m_mediaPlayer)
	{
		libvlc_media_t *curMedia = libvlc_media_player_get_media(m_mediaPlayer);
		if (curMedia == NULL)
			return;
		float new_pos = (float)(pos) / (float)POS_RESOLUTION;
		libvlc_media_player_set_position(m_mediaPlayer, new_pos);
	}
}

void
MSPPlayer::on_actionOriginalpp_triggered(bool flag)
{
	if (m_mediaPlayer)
	{
		libvlc_video_set_scale(m_mediaPlayer, flag);	
	}
}

void
MSPPlayer::on_actionFullScreen_triggered(bool flag)
{
	if (m_mediaPlayer)
	{
		libvlc_set_fullscreen(m_mediaPlayer, flag);
	}
}

void
MSPPlayer::on_actionHelp_triggered()
{
	m_helpWindow->show();
}

void 
MSPPlayer::on_actionExit_triggered()
{
	this->close();
}