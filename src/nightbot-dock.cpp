#include <obs-frontend-api.h>
#include <util/platform.h>
#include <QDockWidget>
#include <QMainWindow>
#include <QPushButton>
#include <QToolButton>
#include <QStyle>
#include <QTableWidget>
#include <QTimer>
#include <QSlider>
#include <QLabel>
#include <QToolTip>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QVBoxLayout>
#include <QWidget>
#include <QFile>

#include "nightbot-dock.h"
#include "nightbot-api.h"
#include "SettingsManager.h"
#include "nightbot-auth.h"
#include "plugin-support.h"
#include "song-request-dialog.h"
#include "nightbot-settings.h"

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

	QPushButton *playPauseButton = createControlButton(style()->standardIcon(QStyle::SP_MediaPlay), get_obs_text("Nightbot.Controls.Play"));
	playPauseButton->setProperty("isPlaying", false);
	QPushButton *skipButton = createControlButton(style()->standardIcon(QStyle::SP_MediaSkipForward), get_obs_text("Nightbot.Controls.Skip"));

	controlsLayout->addWidget(playPauseButton);
	controlsLayout->addWidget(skipButton);

	QPushButton *addButton = createControlButton(
		style()->standardIcon(QStyle::SP_FileDialogNewFolder),
		get_obs_text("Nightbot.SongRequest.Add"));
	controlsLayout->addWidget(addButton);

	controlsLayout->addStretch();

	alertButton = new QPushButton();
	alertButton->setIcon(
		style()->standardIcon(QStyle::SP_MessageBoxWarning)
			.pixmap(24, 24));
	alertButton->setToolTip(get_obs_text("Nightbot.Error.Tooltip"));
	alertButton->setFlat(true);
	alertButton->hide();
	controlsLayout->addWidget(alertButton);

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

	QHBoxLayout *volumeLayout = new QHBoxLayout();
	volumeLayout->setContentsMargins(0, 0, 0, 0);
	QLabel *volumeLabel = new QLabel(get_obs_text("Nightbot.Controls.Volume"));
	volumeLayout->addWidget(volumeLabel);

	volumeSlider = new QSlider(Qt::Horizontal);
	volumeSlider->setRange(0, 100);
	volumeSlider->setTickPosition(QSlider::TicksBelow);
	volumeSlider->setTickInterval(10);
	volumeSlider->setEnabled(NightbotAuth::get().IsAuthenticated());
	volumeLayout->addWidget(volumeSlider);
	mainLayout->addLayout(volumeLayout);

	setLayout(mainLayout);

	connect(refreshButton, &QPushButton::clicked, this, &NightbotDock::onRefreshClicked);

	connect(playPauseButton, &QPushButton::clicked, this, [this, playPauseButton]() {
		bool isPlaying = playPauseButton->property("isPlaying").toBool();
		if (isPlaying) {
			NightbotAPI::get().ControlPause();
			playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
			playPauseButton->setToolTip(get_obs_text("Nightbot.Controls.Play"));
			playPauseButton->setProperty("isPlaying", false);
		} else {
			NightbotAPI::get().ControlPlay();
			playPauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
			playPauseButton->setToolTip(get_obs_text("Nightbot.Controls.Pause"));
			playPauseButton->setProperty("isPlaying", true);
		}
		NightbotAPI::get().FetchSongQueue(get_obs_text("Nightbot.Queue.PlaylistUser"));
	});
	connect(skipButton, &QPushButton::clicked, this, &NightbotDock::onSkipClicked);

	connect(addButton, &QPushButton::clicked, this, &NightbotDock::onAddSongClicked);
	connect(srToggleButton, &QToolButton::clicked, this, &NightbotDock::onToggleSRClicked);

	connect(&NightbotAPI::get(), &NightbotAPI::songQueueFetched, this,
		&NightbotDock::UpdateSongQueue);

	connect(&NightbotAPI::get(), &NightbotAPI::srStatusFetched, this,
		&NightbotDock::updateSRStatusButton);

	connect(&NightbotAPI::get(), &NightbotAPI::volumeFetched, this,
		&NightbotDock::updateVolumeSlider);

	connect(volumeSlider, &QSlider::sliderReleased, this, [this]() {
		onVolumeChanged(volumeSlider->value());
	});

	connect(volumeSlider, &QSlider::valueChanged, this,
		&NightbotDock::onVolumeSliderMoved);
	connect(volumeSlider, &QSlider::sliderPressed, this, [this]() {
		onVolumeSliderMoved(volumeSlider->value());
	});

	connect(&NightbotAuth::get(), &NightbotAuth::authenticationFinished,
		this, &NightbotDock::onAuthStatusChanged);

	connect(&NightbotAPI::get(), &NightbotAPI::apiErrorOccurred, this,
		[this](const QString &) { alertButton->show(); });

	connect(alertButton, &QPushButton::clicked, this,
		&NightbotDock::onAlertClicked);

	refreshTimer = new QTimer(this);
	connect(refreshTimer, &QTimer::timeout, this, &NightbotDock::onRefreshClicked);
	UpdateRefreshTimer();

	if (NightbotAuth::get().IsAuthenticated()) {
		onRefreshClicked();
	}
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

	// 1. Prepara o texto "Tocando Agora" independentemente de qualquer saída.
	QString nowPlayingText = "";
	if (!queue.isEmpty() && queue.at(0).position == 0) {
		const SongItem &currentSong = queue.at(0);
		int minutes = currentSong.duration / 60;
		int seconds = currentSong.duration % 60;
		QString durationStr = QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));

		QString format = QString::fromStdString(SettingsManager::get().GetNowPlayingFormat());
		format.replace("{music}", currentSong.title);
		format.replace("{artist}", currentSong.artist);
		format.replace("{user}", currentSong.user);
		format.replace("{time}", durationStr);

		nowPlayingText = format;
	}

	// 2. Atualiza a fonte de texto, se uma estiver selecionada.
	std::string sourceName = SettingsManager::get().GetNowPlayingSource();
	if (!sourceName.empty() && !nowPlayingText.isEmpty()) {
		obs_source_t *textSource = obs_get_source_by_name(sourceName.c_str());
		if (textSource) {
			obs_data_t *settings = obs_data_create();
			obs_data_set_string(settings, "text", nowPlayingText.toUtf8().constData());
			obs_source_update(textSource, settings);
			obs_data_release(settings);
			obs_source_release(textSource);
		} else {
			obs_log_warning("[Nightbot SR/Dock] Now playing source '%s' not found.", sourceName.c_str());
			SettingsManager::get().SetNowPlayingSource("");
		}
	}

	// 3. Salva para o arquivo, se a opção estiver habilitada.
	if (SettingsManager::get().GetNowPlayingToFileEnabled() && !nowPlayingText.isEmpty()) {
		std::string filePathStr = SettingsManager::get().GetNowPlayingToFilePath();
		if (!filePathStr.empty()) {
			QFile file(QString::fromStdString(filePathStr));
			if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
				QTextStream out(&file);
				out << nowPlayingText;
				file.close();
			} else {
				obs_log_warning("[Nightbot SR/Dock] Failed to write to file '%s'. Error: %s",
						filePathStr.c_str(),
						file.errorString().toUtf8().constData());
				SettingsManager::get().SetNowPlayingToFilePath("");
			}
		}
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
				NightbotAPI::get().DeleteSong(id);
				QTimer::singleShot(500, this, &NightbotDock::onRefreshClicked);
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
	NightbotAPI::get().FetchSRSettings();
}

void NightbotDock::onSkipClicked()
{
	NightbotAPI::get().ControlSkip();
	QTimer::singleShot(500, this, &NightbotDock::onRefreshClicked);

	NightbotAPI::get().FetchSongQueue(get_obs_text(
		"Nightbot.Queue.PlaylistUser"));
}

void NightbotDock::onAddSongClicked()
{
	SongRequestDialog dialog(this);
	dialog.exec();
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

void NightbotDock::onVolumeChanged(int volume)
{
	NightbotAPI::get().SetVolume(volume);
}

void NightbotDock::onAuthStatusChanged(bool success)
{
	alertButton->setVisible(!success);
}

void NightbotDock::onAlertClicked()
{
	NightbotSettingsDialog dialog(
		static_cast<QWidget *>(obs_frontend_get_main_window()));
	dialog.exec();
}

void NightbotDock::onVolumeSliderMoved(int value)
{
	QToolTip::showText(QCursor::pos(), QString::number(value) + "%",
			   volumeSlider);
}

void NightbotDock::updateVolumeSlider(int volume)
{
	// Só atualiza o slider se o usuário não estiver interagindo com ele
	// e se o valor recebido da API for diferente do valor atual.
	if (volumeSlider->isSliderDown() || volumeSlider->value() == volume) {
		return;
	}

	volumeSlider->setEnabled(NightbotAuth::get().IsAuthenticated());
	volumeSlider->blockSignals(true);
	volumeSlider->setValue(volume);
	volumeSlider->blockSignals(false);
	volumeSlider->setToolTip(QString::number(volume) + "%");
}

void NightbotDock::onPromoteSongClicked(const QString &songId)
{
	NightbotAPI::get().PromoteSong(songId);
	QTimer::singleShot(500, this, &NightbotDock::onRefreshClicked);

	NightbotAPI::get().FetchSongQueue(get_obs_text("Nightbot.Queue.PlaylistUser"));
}
