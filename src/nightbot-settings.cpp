#include <obs-frontend-api.h>

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
#include <QFileDialog>
#include <QFile>
#include <QFrame>

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
	authErrorLabel = new QLabel();
	authErrorLabel->setStyleSheet("color: #ffcc00;"); // Amarelo/Laranja
	authErrorLabel->setWordWrap(true);
	authErrorLabel->hide();


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
	authLayout->addWidget(authErrorLabel);
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
	nowPlayingSourceLabel->setWordWrap(true);

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

	nowPlayingLayout->addLayout(formatLabelLayout);
	nowPlayingLayout->addWidget(nowPlayingFormatLineEdit);

	QHBoxLayout *outputLayout = new QHBoxLayout();

	// --- Seção Fonte de Texto ---
	QGroupBox *textSourceGroup = new QGroupBox(get_obs_text("Nightbot.Settings.NowPlayingSource"));
	QVBoxLayout *textSourceLayout = new QVBoxLayout();
	textSourceGroup->setLayout(textSourceLayout);
	textSourceLayout->addWidget(nowPlayingSourceLabel);
	textSourceLayout->addWidget(nowPlayingSourceComboBox);
	textSourceLayout->addStretch();
	outputLayout->addWidget(textSourceGroup);

	// --- Seção Salvar para Arquivo ---
	QGroupBox *saveToFileGroup = new QGroupBox(get_obs_text("Nightbot.Settings.SaveToFile.Title"));
	QVBoxLayout *saveToFileLayout = new QVBoxLayout();
	saveToFileGroup->setLayout(saveToFileLayout);
	saveToFileCheckBox = new QCheckBox(get_obs_text("Nightbot.Settings.SaveToFile.Enable"));
	saveToFileLayout->addWidget(saveToFileCheckBox);

	QHBoxLayout *filePathLayout = new QHBoxLayout();
	filePathLineEdit = new QLineEdit();
	filePathLineEdit->setPlaceholderText(get_obs_text("Nightbot.Settings.SaveToFile.PathPlaceholder"));
	browseButton = new QPushButton();
	browseButton->setIcon(style()->standardIcon(QStyle::SP_DirOpenIcon));
	browseButton->setToolTip(get_obs_text("Nightbot.Settings.SaveToFile.SelectFile"));
	browseButton->setFixedSize(32, filePathLineEdit->sizeHint().height());
	clearPathButton = new QPushButton();
	clearPathButton->setIcon(style()->standardIcon(QStyle::SP_DialogResetButton));
	clearPathButton->setFixedSize(32, filePathLineEdit->sizeHint().height());
	clearPathButton->setToolTip(get_obs_text("Nightbot.Settings.SaveToFile.Clear"));

	filePathLayout->addWidget(filePathLineEdit);
	filePathLayout->addWidget(browseButton);
	filePathLayout->addWidget(clearPathButton);
	saveToFileLayout->addLayout(filePathLayout);

	fileErrorLabel = new QLabel();
	fileErrorLabel->setStyleSheet("color: red;");
	fileErrorLabel->setWordWrap(true);
	fileErrorLabel->hide();
	saveToFileLayout->addWidget(fileErrorLabel);
	saveToFileLayout->addStretch();
	outputLayout->addWidget(saveToFileGroup);

	connect(saveToFileCheckBox, &QCheckBox::toggled, this, &NightbotSettingsDialog::onSaveToFileToggled);
	connect(browseButton, &QPushButton::clicked, this, &NightbotSettingsDialog::onBrowseFileClicked);
	connect(clearPathButton, &QPushButton::clicked, this, &NightbotSettingsDialog::onClearPathClicked);
	connect(filePathLineEdit, &QLineEdit::editingFinished, this, &NightbotSettingsDialog::onFilePathChanged);

	mainLayout->addWidget(authGroup);
	mainLayout->addWidget(queueGroup);
	mainLayout->addWidget(nowPlayingGroup);
	mainLayout->addLayout(outputLayout);

	mainLayout->addStretch();

	QFrame *line = new QFrame();
	line->setFrameShape(QFrame::HLine);
	line->setFrameShadow(QFrame::Sunken);
	mainLayout->addWidget(line);

	QLabel *footerLabel = new QLabel(get_obs_text("Nightbot.Settings.Footer"));
	footerLabel->setOpenExternalLinks(true);
	footerLabel->setAlignment(Qt::AlignCenter);
	mainLayout->addWidget(footerLabel);

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

	connect(&NightbotAPI::get(), &NightbotAPI::apiErrorOccurred, this,
		&NightbotSettingsDialog::onApiError);

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
void NightbotSettingsDialog::onSaveToFileToggled(bool checked)
{
	SettingsManager::get().SetNowPlayingToFileEnabled(checked);
	filePathLineEdit->setEnabled(checked);
	browseButton->setEnabled(checked);
	clearPathButton->setEnabled(checked);
	CheckFilePath();
}

void NightbotSettingsDialog::onBrowseFileClicked()
{
	QString filePath = QFileDialog::getSaveFileName(this, get_obs_text("Nightbot.Settings.SaveToFile.SelectFile"), "", "Text Files (*.txt);;All Files (*)");
	if (!filePath.isEmpty()) {
		filePathLineEdit->setText(filePath);
		onFilePathChanged();
	}
}

void NightbotSettingsDialog::onClearPathClicked()
{
	filePathLineEdit->clear();
	onFilePathChanged();
}

void NightbotSettingsDialog::onFilePathChanged()
{
	SettingsManager::get().SetNowPlayingToFilePath(filePathLineEdit->text().toStdString());
	CheckFilePath();
}

void NightbotSettingsDialog::onApiError(const QString &error)
{
	Q_UNUSED(error);
	authErrorLabel->setText(get_obs_text("Nightbot.Error.AuthenticationFailed"));
	authErrorLabel->show();
}

void NightbotSettingsDialog::CheckFilePath()
{
	QString path = filePathLineEdit->text();
	if (!saveToFileCheckBox->isChecked() || path.isEmpty()) {
		fileErrorLabel->hide();
		return;
	}

	QFile file(path);
	fileErrorLabel->setVisible(!file.open(QIODevice::WriteOnly));
	fileErrorLabel->setText(get_obs_text("Nightbot.Settings.SaveToFile.ErrorWritable"));
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
	if (just_authenticated) {
		authErrorLabel->hide();
	} else if (!auth.IsAuthenticated()) {
		authErrorLabel->setText(get_obs_text("Nightbot.Error.AuthenticationFailed"));
		authErrorLabel->show();
	}
	bool authenticated = auth.IsAuthenticated();

	if (authenticated) {
		std::string user_name = SettingsManager::get().GetNightUserName();
		obs_log_info("[Nightbot SR/Settings] Authenticated user: %s",
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

	bool saveToFile = SettingsManager::get().GetNowPlayingToFileEnabled();
	saveToFileCheckBox->setChecked(saveToFile);
	filePathLineEdit->setText(QString::fromStdString(SettingsManager::get().GetNowPlayingToFilePath()));
	filePathLineEdit->setEnabled(saveToFile);
	browseButton->setEnabled(saveToFile);
	clearPathButton->setEnabled(saveToFile);
	CheckFilePath();
}
