#ifndef NIGHTBOT_AUTH_H
#define NIGHTBOT_AUTH_H

#include <QObject>
#include <string>

class QTcpServer;
class QTcpSocket;
class QTimer;

class NightbotAuth : public QObject {
	Q_OBJECT
public:
	static NightbotAuth &get()
    {
        static NightbotAuth instance;
        return instance;
    }

	enum class RefreshStatus {
		WAITING,
		DONE,
		FAILED
	};

	void Authenticate();
	RefreshStatus RefreshToken();
	void ClearTokens();
	bool IsAuthenticated();

	const std::string &GetAccessToken();

signals:
	void authenticationFinished(bool success);
	void authTimerUpdate(int remainingSeconds);

private slots:
	void onNewConnection();
	void onAuthTimeout();
	void onSecondElapsed();

private:
	NightbotAuth(QObject *parent = nullptr);
	~NightbotAuth();

	std::string client_id = "148baef93cc409a221dfe21a820efbab";
	std::string access_token;
	std::string refresh_token;

	QTcpServer *http_server = nullptr;
	QTimer *auth_timeout_timer = nullptr;
	QTimer *countdown_timer = nullptr;
	int auth_countdown;
};

#endif // NIGHTBOT_AUTH_H
