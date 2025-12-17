#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QDockWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QToolButton>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QWidget>

#include "nightbot-dock.h"
#include "nightbot-api.h"
#include "SettingsManager.h"
#include "nightbot-auth.h"
#include "plugin-support.h"

NightbotDock::NightbotDock() : QWidget(nullptr)
{
	QVBoxLayout *mainLayout = new QVBoxLayout();

	QHBoxLayout *controlsLayout = new QHBoxLayout();
	controlsLayout->setContentsMargins(0, 0, 0, 0);

	auto createControlButton = [&](QIcon icon, const QString &tooltip) {
		QPushButton *button = new QPushButton();
		button->setIcon(icon);
		button->setFixedSize(32, 32);
		button->setIconSize(QSize(24, 24));
		button->setToolTip(tooltip);
		return button;
	};

	QPushButton *playButton = createControlButton(style()->standardIcon(QStyle::SP_MediaPlay), get_obs_text("Nightbot.Controls.Play"));
	QPushButton *pauseButton = createControlButton(style()->standardIcon(QStyle::SP_MediaPause), get_obs_text("Nightbot.Controls.Pause"));
	QPushButton *skipButton = createControlButton(style()->standardIcon(QStyle::SP_MediaSkipForward), get_obs_text("Nightbot.Controls.Skip"));

	controlsLayout->addWidget(playButton);
	controlsLayout->addWidget(pauseButton);
	controlsLayout->addWidget(skipButton);

	controlsLayout->addStretch();

	srToggleButton = new QToolButton();
	srToggleButton->setFixedSize(32, 32);
	srToggleButton->setIconSize(QSize(24, 24));
	srToggleButton->setCheckable(true);
	updateSRStatusButton(false);
	srToggleButton->setEnabled(NightbotAuth::get().IsAuthenticated());
	controlsLayout->addWidget(srToggleButton);


	QPushButton *refreshButton = createControlButton(
		style()->standardIcon(QStyle::SP_BrowserReload), get_obs_text("Nightbot.Dock.Refresh"));
	controlsLayout->addWidget(refreshButton);

	mainLayout->addLayout(controlsLayout);

	songQueueTable = new QTableWidget();
	songQueueTable->setColumnCount(4);

	QStringList headers;
	headers << get_obs_text("Nightbot.Queue.Position")
		<< get_obs_text("Nightbot.Queue.Title")
		<< get_obs_text("Nightbot.Queue.User")
		<< get_obs_text("Nightbot.Queue.Actions");
	songQueueTable->setHorizontalHeaderLabels(headers);

	QHeaderView *header = songQueueTable->horizontalHeader();
	header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
	header->setSectionResizeMode(1, QHeaderView::Stretch);
	header->setSectionResizeMode(2, QHeaderView::ResizeToContents);

	header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
	songQueueTable->verticalHeader()->hide();
	songQueueTable->setEditTriggers(QAbstractItemView::NoEditTriggers);

	mainLayout->addWidget(songQueueTable);

	setLayout(mainLayout);

	connect(refreshButton, &QPushButton::clicked, this, &NightbotDock::onRefreshClicked);

	connect(playButton, &QPushButton::clicked, this, &NightbotDock::onPlayClicked);
	connect(pauseButton, &QPushButton::clicked, this, &NightbotDock::onPauseClicked);
	connect(skipButton, &QPushButton::clicked, this, &NightbotDock::onSkipClicked);

	connect(srToggleButton, &QToolButton::clicked, this, &NightbotDock::onToggleSRClicked);

	connect(&NightbotAPI::get(), &NightbotAPI::songQueueFetched, this,
		&NightbotDock::UpdateSongQueue);

	connect(&NightbotAPI::get(), &NightbotAPI::srStatusFetched, this,
		&NightbotDock::updateSRStatusButton);

	refreshTimer = new QTimer(this);
	connect(refreshTimer, &QTimer::timeout, this, &NightbotDock::onRefreshClicked);
	UpdateRefreshTimer();
};

void NightbotDock::UpdateRefreshTimer()
{
	if (NightbotAuth::get().GetAccessToken().empty()) {
		if (refreshTimer->isActive()) {
			refreshTimer->stop();
			obs_log_info("[Nightbot SR/Dock] Not authenticated. Auto-refresh timer stopped.");
		}
		updateSRStatusButton(false);
		return;
	}

	bool enabled = SettingsManager::get().GetAutoRefreshEnabled();
	if (enabled) {
		int interval_s = SettingsManager::get().GetAutoRefreshInterval();
		if (interval_s > 0) {
			int interval_ms = interval_s * 1000;
			refreshTimer->start(interval_ms);
			obs_log_info("[Nightbot SR/Dock] Auto-refresh timer started with %dms interval.", interval_ms);
		} else {
			refreshTimer->stop();
			obs_log_warning("[Nightbot SR/Dock] Auto-refresh is enabled but interval is invalid (%d seconds). Timer stopped to prevent spam.", interval_s);
		}
	} else {
		refreshTimer->stop();
		obs_log_info("[Nightbot SR/Dock] Auto-refresh timer stopped.");
	}
}

void NightbotDock::UpdateSongQueue(const QList<SongItem> &queue)
{
	songQueueTable->clearContents();
	songQueueTable->setRowCount(static_cast<int>(queue.size()));

	std::string sourceName = SettingsManager::get().GetNowPlayingSource();
	obs_source_t *textSource = nullptr;
	if (!sourceName.empty()) {
		textSource = obs_get_source_by_name(sourceName.c_str());
	}

	if (textSource) {
		obs_data_t *settings = obs_data_create();
		if (queue.isEmpty() || queue.at(0).position != 0) {
			obs_data_set_string(settings, "text", "");
		} else {
			const SongItem &currentSong = queue.at(0);
			int minutes = currentSong.duration / 60;
			int seconds = currentSong.duration % 60;
			QString durationStr = QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));

			QString format = QString::fromStdString(SettingsManager::get().GetNowPlayingFormat());
			format.replace("{music}", currentSong.title);
			format.replace("{artist}", currentSong.artist);
			format.replace("{user}", currentSong.user);
			format.replace("{time}", durationStr);

			obs_data_set_string(settings, "text", format.toUtf8().constData());
		}
		obs_source_update(textSource, settings);
		obs_data_release(settings);
		obs_source_release(textSource);
	} else if (!sourceName.empty()) {
		obs_log_warning("[Nightbot SR/Dock] Now playing source '%s' not found.", sourceName.c_str());
	}

	for (qsizetype i = 0; i < queue.size(); ++i) {
		const SongItem &item = queue.at(static_cast<int>(i));

		int minutes = item.duration / 60;
		int seconds = item.duration % 60;
		QString durationStr = QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
		QString titleWithDuration = QStringLiteral("%1 (%2)").arg(item.title, durationStr);

		QTableWidgetItem *posItem;
		QTableWidgetItem *titleItem = new QTableWidgetItem(titleWithDuration);
		QTableWidgetItem *userItem = new QTableWidgetItem(item.user);

		if (i == 0 && !queue.isEmpty()) {
			posItem = new QTableWidgetItem();
			posItem->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));

			QFont boldFont;
			boldFont.setBold(true);
			posItem->setFont(boldFont);
			titleItem->setFont(boldFont);
			userItem->setFont(boldFont);
		} else {
			posItem = new QTableWidgetItem(QString::number(item.position));

			QWidget *actionsWidget = new QWidget();
			QHBoxLayout *actionsLayout = new QHBoxLayout(actionsWidget);
			actionsLayout->setContentsMargins(5, 0, 5, 0);
			actionsLayout->setSpacing(5);

			if (item.position > 1) {
				QPushButton *promoteButton = new QPushButton();
				promoteButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
				promoteButton->setFixedSize(24, 24); 
				promoteButton->setToolTip(get_obs_text("Nightbot.Queue.Promote"));
				connect(promoteButton, &QPushButton::clicked, this, [this, id = item.id]() {
					onPromoteSongClicked(id);
				});
				actionsLayout->addWidget(promoteButton);
			}

			QPushButton *deleteButton = new QPushButton();
			deleteButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
			deleteButton->setFixedSize(24, 24);
			deleteButton->setToolTip(get_obs_text("Nightbot.Queue.Delete"));
			connect(deleteButton, &QPushButton::clicked, this, [this, id = item.id]() {
				onDeleteSongClicked(id);
			});

			actionsLayout->addWidget(deleteButton);
			actionsWidget->setLayout(actionsLayout);

			songQueueTable->setCellWidget(static_cast<int>(i), 3, actionsWidget);
		}

		posItem->setTextAlignment(Qt::AlignCenter);
		userItem->setTextAlignment(Qt::AlignCenter);
		songQueueTable->setItem(static_cast<int>(i), 0, posItem);
		songQueueTable->setItem(static_cast<int>(i), 1, titleItem);
		songQueueTable->setItem(static_cast<int>(i), 2, userItem);

	}
}

void NightbotDock::onRefreshClicked()
{
	NightbotAPI::get().FetchSongQueue(get_obs_text("Nightbot.Queue.PlaylistUser"));
}

void NightbotDock::onPlayClicked()
{
	NightbotAPI::get().ControlPlay();	
	NightbotAPI::get().FetchSongQueue(get_obs_text("Nightbot.Queue.PlaylistUser"));
}

void NightbotDock::onPauseClicked()
{
	NightbotAPI::get().ControlPause();	
	NightbotAPI::get().FetchSongQueue(get_obs_text("Nightbot.Queue.PlaylistUser"));
}

void NightbotDock::onSkipClicked()
{
	NightbotAPI::get().ControlSkip();
	QTimer::singleShot(500, this, &NightbotDock::onRefreshClicked);

	NightbotAPI::get().FetchSongQueue(get_obs_text(
		"Nightbot.Queue.PlaylistUser"));
}

void NightbotDock::onToggleSRClicked()
{
	bool isChecked = srToggleButton->isChecked();
	NightbotAPI::get().SetSREnabled(isChecked);
	updateSRStatusButton(isChecked);
	QTimer::singleShot(1000, this, &NightbotDock::onRefreshClicked);
}

void NightbotDock::updateSRStatusButton(bool isEnabled)
{
	srToggleButton->setEnabled(NightbotAuth::get().IsAuthenticated());
	srToggleButton->setChecked(isEnabled);
	if (isEnabled) {
		srToggleButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
		srToggleButton->setToolTip(get_obs_text("Nightbot.Dock.SR_Enabled"));
	} else {
		srToggleButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
		srToggleButton->setToolTip(get_obs_text("Nightbot.Dock.SR_Disabled"));
	}
}

void NightbotDock::onDeleteSongClicked(const QString &songId)
{
	NightbotAPI::get().DeleteSong(songId);
	QTimer::singleShot(500, this, &NightbotDock::onRefreshClicked);

	NightbotAPI::get().FetchSongQueue(get_obs_text("Nightbot.Queue.PlaylistUser"));
}

void NightbotDock::onPromoteSongClicked(const QString &songId)
{
	NightbotAPI::get().PromoteSong(songId);
	QTimer::singleShot(500, this, &NightbotDock::onRefreshClicked);

	NightbotAPI::get().FetchSongQueue(get_obs_text("Nightbot.Queue.PlaylistUser"));
}
