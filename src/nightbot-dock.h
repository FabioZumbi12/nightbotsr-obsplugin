#ifndef NIGHTBOT_DOCK_H
#define NIGHTBOT_DOCK_H

#include <QList>
#include <QWidget>

class QPushButton;
class QToolButton;
class QTableWidget;
class QTimer;
class QSlider;
struct SongItem;

class NightbotDock : public QWidget {
	Q_OBJECT

public:
	explicit NightbotDock();
	void UpdateRefreshTimer();

private slots:
	void UpdateSongQueue(const QList<SongItem> &queue);
	void onRefreshClicked();
	void onPlayClicked();
	void onPauseClicked();
	void onSkipClicked();
	void onPromoteSongClicked(const QString &songId);
	void onAddSongClicked();
	void onToggleSRClicked();
	void onAlertClicked();
	void updateSRStatusButton(bool isEnabled);
	void onVolumeChanged(int volume);
	void onAuthStatusChanged(bool success);
	void onVolumeSliderMoved(int value);
	void updateVolumeSlider(int volume);

private:
	QTableWidget *songQueueTable;
	QTimer *refreshTimer;
	QPushButton *alertButton;
	QToolButton *srToggleButton;
	QSlider *volumeSlider;
};

#endif // NIGHTBOT_DOCK_H
