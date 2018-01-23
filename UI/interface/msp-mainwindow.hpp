#pragma  once

#include <QMainWindow>
#include <memory>

#include "vlc/vlc.h"
#include "vlc/libvlc.h"

namespace Ui {class MSPPlayer;}
class MSP_Help;

#define POSITION_RESOLUTION 10000
#define MAX_URL_LENGTH 500

typedef enum{
	LOOPING_NONE=0,
	LOOPING_SINGLE,
	LOOPING_XORDER,
	LOOPING_RANDOM,
}PLAY_MODE;

enum PlayerState {
	STATE_PREPARE,
	STATE_PLAY,
	STATE_PAUSE
};

class MSPPlayer : public QMainWindow
{
    Q_OBJECT
public:
    explicit MSPPlayer(QWidget *parent = 0);
    ~MSPPlayer();

public slots:
	/* actions. */
	void on_actionOpenFile();
	void on_actionExit();
	void on_actionHelp();	
	void on_actionFullScreen(bool flag);
	void on_actionScale(bool flag);
	/* buttons. */
	void on_playButtonClicked();
	void on_stopButtonClicked();
	void on_lastButtonClicked();
	void on_nextButtonClicked();
	/* sliders. */
	void on_seekVolume(int vol);
	void on_seekPosition(int pos);
	/* timer.   */
	void updateInterface();

	void test_demo(char* pszMultiByte);
private:
	Ui::MSPPlayer			  *ui;
	QTimer					  *m_poller;

	/* sub windows */
	std::shared_ptr<MSP_Help> m_helpWindow;
	/* play files. */
	QString					  m_curPlayingFile;
	QStringList				  m_curPlayingList;
	/* play status */		  
	bool					  m_isPlaying;
	bool					  m_isPause;
	/* libvlc hdws */		  
	libvlc_instance_t		  *m_vlcInst;
	libvlc_media_player_t	  *m_mediaPlayer;
	libvlc_media_t			  *m_media;
};


