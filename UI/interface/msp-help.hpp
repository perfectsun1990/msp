#pragma once

#include <QMainWindow>

namespace Ui {class HelpWindow;}

class MSP_Help :public QMainWindow
{
	Q_OBJECT

public:
	explicit MSP_Help(QWidget *parent = 0);
	~MSP_Help();
public slots :
	void on_linkUsClicked(void);
	void on_updateClicked(void);
private:
	Ui::HelpWindow *ui_help;
};

