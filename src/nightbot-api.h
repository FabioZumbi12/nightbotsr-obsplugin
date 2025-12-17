#ifndef NIGHTBOT_API_H
#define NIGHTBOT_API_H

#include <QObject>
#include <string>
#include <QList>
#include <QString>

struct SongItem {
	QString id;
	QString title;
	QString artist;
	QString user;
	int position;
	int duration;
};

class NightbotAPI : public QObject {
	Q_OBJECT

public:
	static NightbotAPI &get();

	void FetchUserInfo();
	void FetchSongQueue(const QString &playlistUserText);

	void ControlPlay();
	void ControlPause();
	void ControlSkip();
	void DeleteSong(const QString &songId);
	void PromoteSong(const QString &songId);
	void SetSREnabled(bool enabled);

signals:
	void userInfoFetched(const QString &userName);
	void songQueueFetched(const QList<SongItem> &queue);
	void srStatusFetched(bool isEnabled);

private:
	NightbotAPI();
};

#endif // NIGHTBOT_API_H
