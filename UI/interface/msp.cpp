
/*
 * MSP-Studio demo based on libVLC/Qt/FFmpeg.
 * Just for fun!
 * Copyright © 2029 Sun Pengfei <perfectsun1990@163.com>
 */

#include "msp.hpp"
#include "msp-mainwindow.hpp"

void
MSPApplication::load_settings(const char* appQss, const char* appQm)
{
	// 1.Create %appdata%/msp/ directory.
	// purpose:store config and log files.
	QStringList paths = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
	if (!paths.isEmpty())
	{
		m_appPath = paths.at(0).toUtf8();
		QDir dir(m_appPath);
		if (!dir.exists())
			dir.mkdir(dir.absoluteFilePath(m_appPath));
	}
	// 2.Load ui style file(msp.qss).
	// general:edit style & rename it to msp.qss.
	m_appQss = appQss;
	QFile qss(appQss);
	qss.open(QFile::ReadOnly);
	setStyleSheet(qss.readAll());
	qss.close();
	// 3.Load language file(msp_ch.qm).
	// general:"lupdate ../../UI -ts  msp_ch.ts".	
	QString str;
	if (QLocale::system().language() == QLocale::China
		|| QLocale::system().language() == QLocale::Chinese)
		str = QDir::currentPath() + appQm;
	else
		str = QDir::currentPath() + appQm;
	m_appTran.load(str);
	if (m_appTran.isEmpty())
		qDebug() << str << "is empty!";
	installTranslator(&m_appTran);
	return;
}

int32_t
main(int32_t argc, char *argv[])
{
	MSPApplication msp_app(argc,argv);	
	MSPPlayer p;
	p.show();
	return msp_app.exec();
}
