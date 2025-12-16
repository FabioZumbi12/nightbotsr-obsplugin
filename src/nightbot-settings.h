#ifndef NIGHTBOT_SETTINGS_H
#define NIGHTBOT_SETTINGS_H

#include <QDialog>

class QLabel;
class QPushButton;

class NightbotSettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit NightbotSettingsDialog(QWidget *parent = nullptr);

private slots:
	void OnConnectClicked();
	void OnDisconnectClicked();
	void onAuthTimerUpdate(int remainingSeconds);


private:
	void UpdateUI(bool just_authenticated = false);

	QLabel *statusLabel;
	QPushButton *connectButton;
	QPushButton *disconnectButton;
};

#endif // NIGHTBOT_SETTINGS_H