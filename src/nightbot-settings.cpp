#include <obs-frontend-api.h>
#include <obs.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QCheckBox>
#include <QSpinBox>
#include <QComboBox>
#include <QStyle>
#include <QGroupBox>
#include <QLineEdit>

#include "nightbot-settings.h"
#include "nightbot-api.h"
#include "nightbot-auth.h"
#include "SettingsManager.h"
#include "nightbot-dock.h"
#include "plugin-support.h"

extern NightbotDock *g_dock_widget;

#define auth NightbotAuth::get()

NightbotSettingsDialog::NightbotSettingsDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(get_obs_text("Nightbot.Settings"));

	QVBoxLayout *mainLayout = new QVBoxLayout();
	setLayout(mainLayout);

	// --- Seção de Autenticação ---
	QGroupBox *authGroup = new QGroupBox(get_obs_text("Nightbot.Settings.Authentication"));
	QVBoxLayout *authLayout = new QVBoxLayout();
	authGroup->setLayout(authLayout);

	QLabel *instructions = new QLabel(
		get_obs_text("Nightbot.Settings.Instructions"));
	instructions->setWordWrap(true);

	statusLabel = new QLabel();

	connectButton =
		new QPushButton(get_obs_text("Nightbot.Settings.Connect"));
	disconnectButton = new QPushButton(
		get_obs_text("Nightbot.Settings.Disconnect"));

	QHBoxLayout *buttonLayout = new QHBoxLayout();
	buttonLayout->addWidget(connectButton);
	buttonLayout->addWidget(disconnectButton);

	authLayout->addWidget(instructions);
	authLayout->addSpacing(10);
	authLayout->addWidget(statusLabel);
	authLayout->addLayout(buttonLayout);

	// --- Seção da Fila de Músicas ---
	QGroupBox *queueGroup = new QGroupBox(get_obs_text("Nightbot.Settings.Queue"));
	QVBoxLayout *queueLayout = new QVBoxLayout();
	queueGroup->setLayout(queueLayout);
	autoRefreshCheckBox = new QCheckBox(get_obs_text("Nightbot.Settings.AutoRefresh.Enable"));
	refreshIntervalSpinBox = new QSpinBox();
	refreshIntervalSpinBox->setMinimum(5);
	refreshIntervalSpinBox->setMaximum(300);
	refreshIntervalSpinBox->setSuffix("s");

	QHBoxLayout *refreshLayout = new QHBoxLayout();
	refreshLayout->addWidget(autoRefreshCheckBox);
	refreshLayout->addWidget(refreshIntervalSpinBox);
	refreshLayout->addStretch();

	queueLayout->addLayout(refreshLayout);

	// --- Seção "Tocando Agora" ---
	QGroupBox *nowPlayingGroup = new QGroupBox(get_obs_text("Nightbot.Settings.NowPlaying"));
	QVBoxLayout *nowPlayingLayout = new QVBoxLayout();
	nowPlayingGroup->setLayout(nowPlayingLayout);

	QLabel *nowPlayingSourceLabel = new QLabel(
		get_obs_text("Nightbot.Settings.NowPlayingSource.Label"));

	nowPlayingSourceComboBox = new QComboBox();

	QHBoxLayout *formatLabelLayout = new QHBoxLayout();
	formatLabelLayout->setContentsMargins(0, 0, 0, 0);
	QLabel *nowPlayingFormatLabel = new QLabel(get_obs_text("Nightbot.Settings.NowPlayingFormat"));
	formatLabelLayout->addWidget(nowPlayingFormatLabel);

	QLabel *helpIconLabel = new QLabel();
	QIcon helpIcon = style()->standardIcon(QStyle::SP_MessageBoxInformation);
	helpIconLabel->setPixmap(helpIcon.pixmap(16, 16));
	helpIconLabel->setToolTip(get_obs_text("Nightbot.Settings.NowPlayingFormat.Tooltip"));
	formatLabelLayout->addWidget(helpIconLabel);
	formatLabelLayout->addStretch();

	nowPlayingFormatLineEdit = new QLineEdit();
	nowPlayingFormatLineEdit->setPlaceholderText("{music}, {artist}, {user}, {time}");

	nowPlayingLayout->addWidget(nowPlayingSourceLabel);
	nowPlayingLayout->addWidget(nowPlayingSourceComboBox);
	nowPlayingLayout->addLayout(formatLabelLayout);
	nowPlayingLayout->addWidget(nowPlayingFormatLineEdit);

	mainLayout->addWidget(authGroup);
	mainLayout->addWidget(queueGroup);
	mainLayout->addWidget(nowPlayingGroup);

	PopulateTextSources();

	connect(nowPlayingSourceComboBox, &QComboBox::currentTextChanged, this,
		&NightbotSettingsDialog::onNowPlayingSourceChanged);
	connect(nowPlayingFormatLineEdit, &QLineEdit::textChanged, this,
		&NightbotSettingsDialog::onNowPlayingFormatChanged);
	connect(autoRefreshCheckBox, &QCheckBox::toggled, this, &NightbotSettingsDialog::onAutoRefreshToggled);
	connect(refreshIntervalSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), this, &NightbotSettingsDialog::onRefreshIntervalChanged);

	connect(connectButton, &QPushButton::clicked, this,
		&NightbotSettingsDialog::OnConnectClicked);
	connect(disconnectButton, &QPushButton::clicked, this,
		&NightbotSettingsDialog::OnDisconnectClicked);

	connect(&auth, &NightbotAuth::authenticationFinished, this, [this](bool success){
		UpdateUI(success); });
	connect(&auth, &NightbotAuth::authTimerUpdate, this,
		&NightbotSettingsDialog::onAuthTimerUpdate);

	UpdateUI();

	connect(&NightbotAPI::get(), &NightbotAPI::userInfoFetched, this,
		&NightbotSettingsDialog::onUserInfoFetched);
}

void NightbotSettingsDialog::OnConnectClicked()
{
	auth.Authenticate();
}

void NightbotSettingsDialog::OnDisconnectClicked()
{
	auth.ClearTokens();
	UpdateUI(false);
}

void NightbotSettingsDialog::onAuthTimerUpdate(int remainingSeconds)
{
	statusLabel->setText(
		QString("<b>%1</b> <span style='color: #ffcc00;'>%2 (%3s)</span>")
			.arg(get_obs_text("Nightbot.Settings.Status"))
			.arg(get_obs_text(
				"Nightbot.Settings.Status.Authenticating"))
			.arg(remainingSeconds));
}

void NightbotSettingsDialog::onUserInfoFetched(const QString &userName)
{
	if (!userName.isEmpty()) {
		SettingsManager::get().SetUserName(userName.toStdString());

		if (g_dock_widget) {
			NightbotAPI::get().FetchSongQueue(get_obs_text("Nightbot.Queue.PlaylistUser"));
		}

		UpdateUI();
	}
}

void NightbotSettingsDialog::onAutoRefreshToggled(bool checked)
{
	SettingsManager::get().SetAutoRefreshEnabled(checked);
	refreshIntervalSpinBox->setEnabled(checked);
	if (g_dock_widget)
		g_dock_widget->UpdateRefreshTimer();
}

void NightbotSettingsDialog::onRefreshIntervalChanged(int value)
{
	SettingsManager::get().SetAutoRefreshInterval(value);
	if (g_dock_widget)
		g_dock_widget->UpdateRefreshTimer();
}

void NightbotSettingsDialog::PopulateTextSources()
{
	nowPlayingSourceComboBox->blockSignals(true);
	nowPlayingSourceComboBox->clear();
	nowPlayingSourceComboBox->addItem("", ""); // Add an empty option

	obs_enum_sources([](void *data, obs_source_t *source) {
		QComboBox *combo = static_cast<QComboBox *>(data);
		const char *unversioned_id = obs_source_get_unversioned_id(source);
		if (strcmp(unversioned_id, "text_gdiplus") == 0 ||
		    strcmp(unversioned_id, "text_ft2_source") == 0 ||
		    strcmp(unversioned_id, "text_source") == 0) {
			const char *name = obs_source_get_name(source);
			combo->addItem(name, name);
		}
		return true;
	}, nowPlayingSourceComboBox);

	std::string currentSource = SettingsManager::get().GetNowPlayingSource();
	int index = nowPlayingSourceComboBox->findData(currentSource.c_str());
	if (index != -1) {
		nowPlayingSourceComboBox->setCurrentIndex(index);
	}

	nowPlayingSourceComboBox->blockSignals(false);
}

void NightbotSettingsDialog::onNowPlayingSourceChanged(const QString &sourceName)
{
	SettingsManager::get().SetNowPlayingSource(sourceName.toStdString());
	SettingsManager::get().Save();
}

void NightbotSettingsDialog::onNowPlayingFormatChanged(const QString &format)
{
	SettingsManager::get().SetNowPlayingFormat(format.toStdString());
	SettingsManager::get().Save();
}

void NightbotSettingsDialog::UpdateUI(bool just_authenticated)
{
	bool authenticated = auth.IsAuthenticated();

	if (authenticated) {
		std::string user_name = SettingsManager::get().GetNightUserName();
		blog(LOG_INFO, "[Nightbot SR/Settings] Authenticated user: %s",
				user_name.c_str());

		if (just_authenticated || user_name.empty()) {
			NightbotAPI::get().FetchUserInfo();
		}

		if (!user_name.empty()) {
			QString connected_as_format = get_obs_text(
				"Nightbot.Settings.Status.ConnectedAs");
			QString connected_as_text =
				connected_as_format.arg(user_name.c_str());
			statusLabel->setText(QString("<b>%1</b> %2")
						 .arg(get_obs_text("Nightbot.Settings.Status"))
						 .arg(connected_as_text));
		}

	} else {
		statusLabel->setText(QString("<b>%1</b> %2")
					 .arg(get_obs_text(
						 "Nightbot.Settings.Status"))
					 .arg(get_obs_text(
						 "Nightbot.Settings.Status.Disconnected")));
	}

	connectButton->setEnabled(!authenticated);
	disconnectButton->setEnabled(authenticated);

	bool autoRefreshEnabled = SettingsManager::get().GetAutoRefreshEnabled();
	autoRefreshCheckBox->setChecked(autoRefreshEnabled);
	refreshIntervalSpinBox->setEnabled(autoRefreshEnabled);
	refreshIntervalSpinBox->setValue(SettingsManager::get().GetAutoRefreshInterval());

	PopulateTextSources();
	nowPlayingFormatLineEdit->setText(
		QString::fromStdString(SettingsManager::get().GetNowPlayingFormat()));
}
