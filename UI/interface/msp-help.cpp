
#include "msp.hpp"
#include "msp-help.hpp"
#include "ui_msp-help.h"

MSP_Help::MSP_Help(QWidget *parent):
	QMainWindow(parent),
	ui_help(new Ui::HelpWindow)
{
	ui_help->setupUi(this);
	connect(ui_help->linkButton,   SIGNAL(clicked()), this, SLOT(on_linkUsClicked()));
	connect(ui_help->updateButton, SIGNAL(clicked()), this, SLOT(on_updateClicked()));
}

MSP_Help::~MSP_Help()
{
	delete ui_help;
}

void
MSP_Help::on_linkUsClicked(void)
{
	QMessageBox::information(NULL, "Information", 
		QWidget::tr("Email:perfectsun1990@163.com"));
}

void
MSP_Help::on_updateClicked(void)
{
	QMessageBox::information(NULL, "Information",
		QWidget::tr("Current is latest version!"));
}
