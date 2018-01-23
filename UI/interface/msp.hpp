
#pragma once

#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <stdint.h>
#include <stddef.h>
#ifdef WIN32
#include <windows.h>
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

#define  FUNC_ENTER qDebug() << __FUNCTION__ << __LINE__ <<":";

class MSPApplication : public QApplication
{
	Q_OBJECT
public:
	explicit MSPApplication(int &argc, char *argv[]) : QApplication(argc, argv) {
		load_settings();
	};
	~MSPApplication() {};
private:
	void load_settings(const char* appQss=":/msp.qss",const char* appQm="/msp_ch.qm");
private:
	QString		m_appPath;
	QString		m_appQss;
	QTranslator m_appTran;
};
