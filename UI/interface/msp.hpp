
#pragma once
#include <iostream>
#include <chrono>
#include <future>
#include <atomic>
#include <memory>
#include <cstdio>
#include <thread>
#include <mutex>
#include <functional>
#include <cassert>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <ctime>
#ifdef WIN32
#include <windows.h>
#include <direct.h>  
#include <io.h>
#include<shellapi.h>
#else
#endif
#include <QApplication>
#include <QStandardPaths>
#include <QTranslator>
#include <QFile>
#include <QDir>
#include <QTimer>
#include <QEventLoop>
#include <QDesktopWidget>
#include <QDateTime>
#include <QFileDialog>
#include <QFileInfo>
#include <QUrl>
#include <QFontMetrics>
#include <QScrollBar>
#include <QMessageBox>
#include <QDebug>
#ifdef Q_WS_X11
#include <QX11EmbedContainer>
#endif

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#define  MSG(fmt, ...)				printf( fmt, ##__VA_ARGS__ )
#define  FUNC_ENTER MSG("Enter [%s:%d]\n",__FUNCTION__,__LINE__);

class MSPApp : public QApplication
{
	Q_OBJECT
public:
	explicit MSPApp(int &argc, char *argv[]) : QApplication(argc, argv) {
		load_settings();
	};
	~MSPApp() {};
private:
	void load_settings(const char* appQss=":/msp.qss",const char* appQm="/msp_ch.qm");
private:
	QString		m_appPath;
	QString		m_appQss;
	QTranslator m_appTran;
};
