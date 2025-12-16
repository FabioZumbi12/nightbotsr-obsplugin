#include <obs-frontend-api.h>
#include <obs-module.h>
#include <util/platform.h>

#include <QAction>
#include <QDockWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QWidget>

#include "nightbot-dock.h"

NightbotDock::NightbotDock(QWidget *parent) : QWidget(parent)
{
	// Layout principal vertical
	QVBoxLayout *mainLayout = new QVBoxLayout();

	// Layout horizontal para os botões de controle
	QHBoxLayout *controlsLayout = new QHBoxLayout();
	QPushButton *prevButton = new QPushButton();
	prevButton->setIcon(style()->standardIcon(QStyle::SP_MediaSkipBackward));
	QPushButton *playButton = new QPushButton();
	playButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
	QPushButton *pauseButton = new QPushButton();
	pauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
	QPushButton *stopButton = new QPushButton();
	stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
	QPushButton *nextButton = new QPushButton();
	nextButton->setIcon(style()->standardIcon(QStyle::SP_MediaSkipForward));

	controlsLayout->addWidget(prevButton);
	controlsLayout->addWidget(playButton);
	controlsLayout->addWidget(pauseButton);
	controlsLayout->addWidget(stopButton);
	controlsLayout->addWidget(nextButton);

	// Adiciona o layout dos controles ao layout principal
	mainLayout->addLayout(controlsLayout);

	QTableWidget *songQueueTable = new QTableWidget();
	songQueueTable->setColumnCount(3);

	QStringList headers;
	headers << obs_module_text("Nightbot.Queue.Position")
		<< obs_module_text("Nightbot.Queue.Title")
		<< obs_module_text("Nightbot.Queue.User");
	songQueueTable->setHorizontalHeaderLabels(headers);

	QHeaderView *header = songQueueTable->horizontalHeader();
	header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(1, QHeaderView::Stretch);
	header->setSectionResizeMode(2, QHeaderView::ResizeToContents);

	mainLayout->addWidget(songQueueTable);

	setLayout(mainLayout);
};
