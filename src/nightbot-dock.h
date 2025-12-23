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

public slots:
	void SetPlayPauseState(bool isPlaying);

private slots:
	void UpdateSongQueue(const QList<SongItem> &queue);
	void onRefreshClicked();
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
	QPushButton *playPauseButton;
	QTableWidget *songQueueTable;
	QTimer *refreshTimer;
	QPushButton *alertButton;
	QToolButton *srToggleButton;
	QSlider *volumeSlider;
};

#endif // NIGHTBOT_DOCK_H
