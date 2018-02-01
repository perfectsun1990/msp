#pragma  once

#include <QMainWindow>
#include <memory>

#include "vlc/vlc.h"
#include "vlc/libvlc.h"

namespace Ui {class MSPPlayer;}
class MSP_Help;

#define POS_RESOLUTION		10000
#define MAX_URL_LENGTH		512
typedef enum {
	LOOPING_NOLOOP=0,
	LOOPING_SINGLE,
	LOOPING_XORDER,
	LOOPING_RANDOM,
}PLAY_MODE;

class MSPPlayer : public QMainWindow
{
    Q_OBJECT
public:
    explicit MSPPlayer(QWidget *parent = 0);
    ~MSPPlayer();

public slots:
	/* actions. */
	void on_actionOpen_triggered();
	void on_actionExit_triggered();
	void on_actionHelp_triggered();
	void on_actionFullScreen_triggered(bool flag);
	void on_actionOriginalpp_triggered(bool flag);
	/* buttons. */
	void on_playButton_clicked();
	void on_stopButton_clicked();
	void on_lastButton_clicked();
	void on_nextButton_clicked();
	/* sliders. */
	void on_volmSlider_sliderMoved(int vol);
	void on_seekSlider_sliderMoved(int pos);

	void test_demo(char* pszMultiByte);
private:
	std::shared_ptr<Ui::MSPPlayer> ui		 = nullptr;
	std::shared_ptr<QTimer>	  m_poller;
	/* vlc handle */
	libvlc_instance_t		  *m_vInst		 = nullptr;
	libvlc_media_player_t	  *m_mediaPlayer = nullptr;
	libvlc_media_t			  *m_media		 = nullptr;
	/* sub widget */
	std::shared_ptr<MSP_Help> m_helpWindow;
	/* play medias. */
	QString					  m_curPlayingFile;
	QStringList				  m_curPlayingList;
	/* play status */		  
	bool					  m_isPlaying	 = false;
	bool					  m_isPause      = false;
	int64_t					  m_duration	 = 0;
	int64_t					  m_cur_time	 = 0;
	int64_t					  m_pre_time	 = 0;
};
