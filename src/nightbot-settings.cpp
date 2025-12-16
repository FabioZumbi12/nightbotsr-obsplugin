#include <obs-frontend-api.h>
#include <obs-module.h>

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>

#include "nightbot-settings.h"
#include "nightbot-api.h"
#include "nightbot-auth.h"
#include "SettingsManager.h"

// Use the singleton accessor
#define auth NightbotAuth::get()

NightbotSettingsDialog::NightbotSettingsDialog(QWidget *parent)
	: QDialog(parent)
{
	setWindowTitle(obs_module_text("Nightbot.Settings"));

	QVBoxLayout *mainLayout = new QVBoxLayout();
	setLayout(mainLayout);

	// Instruções
	QLabel *instructions = new QLabel(
		obs_module_text("Nightbot.Settings.Instructions"));
	instructions->setWordWrap(true);

	// Status da Conexão
	statusLabel = new QLabel();

	// Botões
	connectButton =
		new QPushButton(obs_module_text("Nightbot.Settings.Connect"));
	disconnectButton = new QPushButton(
		obs_module_text("Nightbot.Settings.Disconnect"));

	QHBoxLayout *buttonLayout = new QHBoxLayout();
	buttonLayout->addWidget(connectButton);
	buttonLayout->addWidget(disconnectButton);

	mainLayout->addWidget(instructions);
	mainLayout->addSpacing(15);
	mainLayout->addWidget(statusLabel);
	mainLayout->addLayout(buttonLayout);

	// Conecta os sinais dos botões aos slots
	connect(connectButton, &QPushButton::clicked, this,
		&NightbotSettingsDialog::OnConnectClicked);
	connect(disconnectButton, &QPushButton::clicked, this,
		&NightbotSettingsDialog::OnDisconnectClicked);

	// Connect to the auth finished signal to update the UI automatically
	connect(&auth, &NightbotAuth::authenticationFinished, this, [this](bool success){
		UpdateUI(success); });
	connect(&auth, &NightbotAuth::authTimerUpdate, this,
		&NightbotSettingsDialog::onAuthTimerUpdate);

	UpdateUI();
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
			.arg(obs_module_text("Nightbot.Settings.Status"))
			.arg(obs_module_text(
				"Nightbot.Settings.Status.Authenticating"))
			.arg(remainingSeconds));
}

void NightbotSettingsDialog::UpdateUI(bool just_authenticated)
{
	bool authenticated = auth.IsAuthenticated();

	if (authenticated) {
		std::string user_name = SettingsManager::get().GetNightUserName();
		blog(LOG_INFO, "[Nightbot SR/Settings] Authenticated user: %s",
				user_name.c_str());

		// Se acabamos de autenticar ou não temos o nome salvo, buscamos na API
		if (just_authenticated || user_name.empty()) {
			std::string fetched_name = NightbotAPI::get().FetchUserInfo();
			if (!fetched_name.empty()) {
				user_name = fetched_name;
				SettingsManager::get().SetUserName(user_name);
			}
		}

		if (!user_name.empty()) {
			QString connected_as_format = obs_module_text(
				"Nightbot.Settings.Status.ConnectedAs");
			QString connected_as_text =
				connected_as_format.arg(user_name.c_str());
			statusLabel->setText(QString("<b>%1</b> %2")
						 .arg(obs_module_text("Nightbot.Settings.Status"))
						 .arg(connected_as_text));
		}

	} else {
		statusLabel->setText(QString("<b>%1</b> %2")
					 .arg(obs_module_text(
						 "Nightbot.Settings.Status"))
					 .arg(obs_module_text(
						 "Nightbot.Settings.Status.Disconnected")));
	}

	connectButton->setEnabled(!authenticated);
	disconnectButton->setEnabled(authenticated);
}
