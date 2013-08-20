#ifndef INFORMATIONWIDGET_H
#define INFORMATIONWIDGET_H

#include <QDockWidget>
#include <QModelIndex>
#include <stdint.h>
#include "EventLog.h"

namespace Ui {
class InformationWidget;
}

class InformationWidget : public QDockWidget
{
	Q_OBJECT
	
public:
	explicit InformationWidget(QWidget *parent = 0);
	~InformationWidget();


public slots:
	void addMessage(rpl::Event *newMsg);

signals:
	void setCurrentVersion(int version);

protected slots:
	void onMessageLogDoubleClicked(QModelIndex index);
	void onFilterTextChanged(QString newText);
	
private:
	Ui::InformationWidget *ui;
	EventLog *messageLog;
};

#endif // INFORMATIONWIDGET_H