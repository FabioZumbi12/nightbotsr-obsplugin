#ifndef NIGHTBOT_SETTINGS_H
#define NIGHTBOT_SETTINGS_H

#include <QDialog>

class QLabel;
class QPushButton;
class QCheckBox;
class QSpinBox;
class QComboBox;
class QLineEdit;

class NightbotSettingsDialog : public QDialog {
	Q_OBJECT

public:
	explicit NightbotSettingsDialog(QWidget *parent = nullptr);

private slots:
	void OnConnectClicked();
	void OnDisconnectClicked();
	void onAuthTimerUpdate(int remainingSeconds);
	void onUserInfoFetched(const QString &userName);
	void onAutoRefreshToggled(bool checked);
	void onRefreshIntervalChanged(int value);
	void onNowPlayingSourceChanged(const QString &sourceName);
	void onNowPlayingFormatChanged(const QString &format);
	void onApiError(const QString &error);
	void onSaveToFileToggled(bool checked);
	void onBrowseFileClicked();
	void onClearPathClicked();
	void onFilePathChanged();

private:
	void UpdateUI(bool just_authenticated = false);
	void PopulateTextSources();
	void CheckFilePath();

	QLabel *statusLabel;
	QLabel *authErrorLabel;
	QPushButton *connectButton;
	QPushButton *disconnectButton;
	QCheckBox *autoRefreshCheckBox;
	QSpinBox *refreshIntervalSpinBox;
	QComboBox *nowPlayingSourceComboBox;
	QLineEdit *nowPlayingFormatLineEdit;
	QCheckBox *saveToFileCheckBox;
	QLineEdit *filePathLineEdit;
	QPushButton *browseButton;
	QPushButton *clearPathButton;
	QLabel *fileErrorLabel;
};

#endif // NIGHTBOT_SETTINGS_H
