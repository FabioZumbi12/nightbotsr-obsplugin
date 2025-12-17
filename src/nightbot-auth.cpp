#include "nightbot-auth.h"
#include <obs.h>
#include <curl/curl.h>

#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>

#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "SettingsManager.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

static const char *BACKEND_BASE_URL = "https://nightbot-plugin.areaz12server.net.br";

static void LoadState(std::string &access, std::string &refresh)
{
	access = SettingsManager::get().GetAccessToken();
	refresh = SettingsManager::get().GetRefreshToken();
}

NightbotAuth::NightbotAuth(QObject *parent) : QObject(parent)
{
	LoadState(access_token, refresh_token);

	http_server = new QTcpServer(this);
	connect(http_server, &QTcpServer::newConnection, this,
		&NightbotAuth::onNewConnection);

	auth_timeout_timer = new QTimer(this);
	auth_timeout_timer->setSingleShot(true);
	connect(auth_timeout_timer, &QTimer::timeout, this,
		&NightbotAuth::onAuthTimeout);

	countdown_timer = new QTimer(this);
	connect(countdown_timer, &QTimer::timeout, this,
		&NightbotAuth::onSecondElapsed);
}

NightbotAuth::~NightbotAuth()
{
    if (http_server->isListening())
		http_server->close();
}

void NightbotAuth::Authenticate()
{
	if (http_server->isListening()) {
		blog(LOG_INFO,
		     "[Nightbot SR/Auth] Authentication process is already in progress.");
		return;
	}

	if (!http_server->listen(QHostAddress::LocalHost, 8921)) {
		blog(LOG_ERROR, "[Nightbot SR/Auth] Failed to start local server.");
		return;
	}

	blog(LOG_INFO, "[Nightbot SR/Auth] Starting local server for 30 seconds...");
	auth_timeout_timer->start(30000);

	auth_countdown = 30;
	emit authTimerUpdate(auth_countdown);
	countdown_timer->start(1000);


	const char *redirect_uri = BACKEND_BASE_URL;
	const char *scopes = "song_requests_queue song_requests";

	QUrl url("https://nightbot.tv/oauth2/authorize");
	QUrlQuery query;

	query.addQueryItem("response_type", "code");
	query.addQueryItem("client_id", client_id.c_str());
	query.addQueryItem("redirect_uri", redirect_uri);
	query.addQueryItem("scope", scopes);

	url.setQuery(query);

	QDesktopServices::openUrl(url);
}

bool NightbotAuth::RefreshToken()
{
	if (refresh_token.empty()) {
		blog(LOG_WARNING,
		     "[Nightbot SR/Auth] No refresh token available to renew.");
		return false;
	}

	blog(LOG_INFO, "[Nightbot SR/Auth] Refreshing token...");

	CURL *curl = curl_easy_init();
	if (!curl) {
		blog(LOG_ERROR,
		     "[Nightbot SR/Auth] Failed to initialize libcurl for token refresh.");
		return false;
	}

	std::string readBuffer;
	auto write_callback = [](void *contents, size_t size, size_t nmemb,
				 void *userp) -> size_t {
		((std::string *)userp)->append((char *)contents, size * nmemb);
		return size * nmemb;
	};

	std::string refresh_url_str = std::string(BACKEND_BASE_URL) + "/api/refresh-token";
	const char *refresh_url = refresh_url_str.c_str();

	QJsonObject request_body;
	request_body["refresh_token"] = QString::fromStdString(refresh_token);
	QJsonDocument doc(request_body);
	std::string postFields =
		doc.toJson(QJsonDocument::Compact).toStdString();

	curl_easy_setopt(curl, CURLOPT_URL, refresh_url);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	CURLcode res = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	bool success = false;

	if (res != CURLE_OK) {
		blog(LOG_ERROR, "[Nightbot SR/Auth] Token refresh request failed: %s",
		     curl_easy_strerror(res));
	} else if (http_code >= 200 && http_code < 300) {
		QJsonParseError parseError;
		QJsonDocument doc = QJsonDocument::fromJson(
			QByteArray::fromStdString(readBuffer), &parseError);

		if (doc.isNull()) {
			blog(LOG_ERROR,
			     "[Nightbot SR/Auth] Failed to parse token refresh response: %s",
			     parseError.errorString().toUtf8().constData());
		} else {
			QJsonObject data = doc.object();
			if (data.contains("access_token") &&
			    data["access_token"].isString()) {
				access_token =
					data["access_token"].toString().toStdString();
				SettingsManager::get().SetAccessToken(access_token);

				if (data.contains("refresh_token") &&
				    data["refresh_token"].isString()) {
					refresh_token = data["refresh_token"]
							      .toString()
							      .toStdString();
					SettingsManager::get().SetRefreshToken(
						refresh_token);
				}
				SettingsManager::get().Save();

				blog(LOG_INFO,
				     "[Nightbot SR/Auth] Token refreshed successfully.");
				success = true;
			} else {
				blog(LOG_WARNING,
				     "[Nightbot SR/Auth] Token refresh response is missing 'access_token'.");
			}
		}
	} else {
		blog(LOG_WARNING,
		     "[Nightbot SR/Auth] Token refresh failed with HTTP status %ld. Response: %s",
		     http_code, readBuffer.c_str());
	}

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);
	return success;
}

void NightbotAuth::ClearTokens()
{
	access_token.clear();
	refresh_token.clear();

	SettingsManager::get().SetAccessToken("");
	SettingsManager::get().SetRefreshToken("");
	SettingsManager::get().SetUserName("");
	SettingsManager::get().Save();
}

bool NightbotAuth::IsAuthenticated()
{
	return !access_token.empty();
}

const std::string &NightbotAuth::GetAccessToken()
{
	return access_token;
}

void NightbotAuth::onNewConnection()
{
	auth_timeout_timer->stop();
	countdown_timer->stop();

	QTcpSocket *clientSocket = http_server->nextPendingConnection();
	if (!clientSocket)
		return;

	connect(clientSocket, &QTcpSocket::readyRead, this, [this, clientSocket]() {
		if (!clientSocket || !clientSocket->isValid()){
            blog(LOG_WARNING, "[Nightbot SR/Auth] Invalid client socket.");
            return;
        }

		QString request = clientSocket->readAll();
		QStringList reqLines = request.split("\r\n");
		if (reqLines.isEmpty()) {
			clientSocket->disconnectFromHost();
            blog(LOG_WARNING, "[Nightbot SR/Auth] Empty HTTP request received.");
			return;
		}

		QString firstLine = reqLines.first();
		QStringList requestParts = firstLine.split(" ");
		if (requestParts.size() < 2) {
			clientSocket->disconnectFromHost();
			return;
		}
		QString method = requestParts[0];
		QString path = requestParts[1];

		if (method == "OPTIONS") {
			QString httpResponse =
				"HTTP/1.1 204 No Content\r\n"
				"Access-Control-Allow-Origin: *\r\n"
				"Access-Control-Allow-Methods: POST, OPTIONS\r\n"
				"Access-Control-Allow-Headers: Content-Type\r\n"
				"\r\n";
			clientSocket->write(httpResponse.toUtf8());
			clientSocket->disconnectFromHost();
			return;
		}

		if (method == "POST" && path == "/token") {
			int bodyIndex = request.indexOf("\r\n\r\n");
			if (bodyIndex == -1) {
				QString httpResponse = "HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
				clientSocket->write(httpResponse.toUtf8());
				clientSocket->disconnectFromHost();
				http_server->close();
				return;
			}
			QString body = request.mid(bodyIndex + 4);

			QJsonParseError parseError;
			QJsonDocument doc = QJsonDocument::fromJson(
				body.toUtf8(), &parseError);

			if (doc.isNull()) {
				blog(LOG_WARNING,
				     "[Nightbot SR/Auth] Failed to parse tokens from POST body: %s",
				     parseError.errorString().toUtf8().constData());
				emit authenticationFinished(false);
				QString httpResponse = "HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
				clientSocket->write(httpResponse.toUtf8());
			} else {
				QJsonObject data = doc.object();
				if (data.contains("access_token") && data["access_token"].isString() &&
				    data.contains("refresh_token") && data["refresh_token"].isString()) {
					access_token = data["access_token"].toString().toStdString();
					refresh_token = data["refresh_token"].toString().toStdString();

					SettingsManager::get().SetAccessToken(access_token);
					SettingsManager::get().SetRefreshToken(refresh_token);
					SettingsManager::get().Save();

					blog(LOG_INFO, "[Nightbot SR/Auth] Tokens received and saved successfully.");
					emit authenticationFinished(true);

					QString httpResponse = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
					clientSocket->write(httpResponse.toUtf8());
				} else {
					blog(LOG_WARNING, "[Nightbot SR/Auth] Missing or invalid tokens in JSON body.");
					emit authenticationFinished(false);
					QString httpResponse = "HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
					clientSocket->write(httpResponse.toUtf8());
				}
			}
		} else {
			blog(LOG_WARNING, "[Nightbot SR/Auth] Received unexpected request: %s %s",
			     method.toUtf8().constData(), path.toUtf8().constData());
			QString httpResponse = "HTTP/1.1 404 Not Found\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
			clientSocket->write(httpResponse.toUtf8());
		}
		clientSocket->disconnectFromHost();
		http_server->close();
	});

	connect(clientSocket, &QTcpSocket::disconnected, clientSocket,
		&QTcpSocket::deleteLater);
}

void NightbotAuth::onAuthTimeout()
{
	if (http_server->isListening()) {
		blog(LOG_WARNING,
		     "[Nightbot SR/Auth] Authentication timed out after 30 seconds. Server is shutting down.");
		countdown_timer->stop();
		http_server->close();
		emit authenticationFinished(false);
	}
}

void NightbotAuth::onSecondElapsed()
{
	auth_countdown--;
	emit authTimerUpdate(auth_countdown);

	if (auth_countdown <= 0) {
		countdown_timer->stop();
	}
}
