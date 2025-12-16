#include "nightbot-auth.h"

#include <obs-module.h>
#include <curl/curl.h>

#include <QUrl>
#include <QUrlQuery>
#include <QDesktopServices>

#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>

#include "SettingsManager.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;

static const char *BACKEND_BASE_URL =
	"https://6000-firebase-studio-1765863731028.cluster-r7kbxfo3fnev2vskbkhhphetq6.cloudworkstations.dev";

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
	// Não é single-shot, vai disparar a cada segundo
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
	auth_timeout_timer->start(30000); // 30 segundos de timeout

	auth_countdown = 30;
	emit authTimerUpdate(auth_countdown); // Emite o valor inicial
	countdown_timer->start(1000);	      // Inicia o timer de 1 segundo


	const char *redirect_uri = BACKEND_BASE_URL;

	const char *scopes = "song_requests_queue";

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

	// URL do endpoint de refresh no backend
	std::string refresh_url_str = std::string(BACKEND_BASE_URL) + "/api/refresh-token";
	const char *refresh_url = refresh_url_str.c_str();

	json request_body;
	request_body["refresh_token"] = refresh_token;
	std::string postFields = request_body.dump();

	curl_easy_setopt(curl, CURLOPT_URL, refresh_url);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postFields.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
	// Adicionar header de content-type para JSON
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
		try {
			json data = json::parse(readBuffer);
			if (data.contains("access_token")) {
				access_token = data["access_token"];
				SettingsManager::get().SetAccessToken(access_token);

				// Opcional: O servidor pode retornar um novo refresh_token
				if (data.contains("refresh_token")) {
					refresh_token = data["refresh_token"];
					SettingsManager::get().SetRefreshToken(
						refresh_token);
				}

				blog(LOG_INFO,
				     "[Nightbot SR/Auth] Token refreshed successfully.");

				success = true;
			} else {
				blog(LOG_WARNING,
				     "[Nightbot SR/Auth] Token refresh response is missing 'access_token'.");
			}
		} catch (const json::parse_error &e) {
			blog(LOG_ERROR,
			     "[Nightbot SR/Auth] Failed to parse token refresh response: %s",
			     e.what());
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

		// Decodifica a URL: substitui '+' por espaço e depois decodifica o resto.
		QString tempPath = path;
		tempPath.replace('+', ' ');
		QString decodedPath = QUrl::fromPercentEncoding(tempPath.toUtf8());
		QUrl url(decodedPath);
		QUrlQuery query(url);
		auto build_response_page = [](const QString &title,
						const QString &body,
						int close_timeout_ms = 5000) {
			return QString(
				"<!DOCTYPE html><html><head><title>%1</title></head><body style='font-family: sans-serif; background-color: #2d2d2d; color: #f0f0f0; text-align: center; padding-top: 50px;'>"
				"%2"
				"<script>setTimeout(function() { window.close(); }, %3);</script></body></html>")
				.arg(title, body, QString::number(close_timeout_ms));
		};

		// Lida com a requisição POST do backend com os tokens
		if (method == "POST" && path == "/token") {
			int bodyIndex = request.indexOf("\r\n\r\n");
			if (bodyIndex == -1) {
				clientSocket->disconnectFromHost();
				return;
			}
			QString body = request.mid(bodyIndex + 4);

			try {
				json data = json::parse(body.toStdString());
				if (data.contains("access_token") &&
				    data.contains("refresh_token")) {
					access_token = data["access_token"];
					refresh_token = data["refresh_token"];

					SettingsManager::get().SetAccessToken(access_token);
					SettingsManager::get().SetRefreshToken(refresh_token);

					blog(LOG_INFO, "[Nightbot SR/Auth] Tokens received and saved successfully.");
					emit authenticationFinished(true);
					QString httpResponse = "HTTP/1.1 200 OK\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
					clientSocket->write(httpResponse.toUtf8());

				} else {
					throw std::runtime_error("Missing tokens in JSON body");
				}
			} catch (const std::exception &e) {
				blog(LOG_WARNING, "[Nightbot SR/Auth] Failed to parse tokens from POST body: %s", e.what());
				emit authenticationFinished(false);
				QString httpResponse = "HTTP/1.1 400 Bad Request\r\nAccess-Control-Allow-Origin: *\r\n\r\n";
				clientSocket->write(httpResponse.toUtf8());
			}
		}
		// Lida com a requisição GET de erro vinda do backend
		else if (method == "GET" && query.hasQueryItem("error")) {
			QString error_reason = query.queryItemValue("error_description");
			if (error_reason.isEmpty())
				error_reason = query.queryItemValue("error");

			blog(LOG_WARNING, "[Nightbot SR/Auth] Authentication failed with error: %s", error_reason.toUtf8().constData());

			QString error_body =
				QString(obs_module_text("Auth.Page.Error.Message"))
					.arg(error_reason);
			QString errorPage = build_response_page(
				obs_module_text("Auth.Page.Title"),
				QString(obs_module_text("Auth.Page.Error.Title")) + error_body,
				10000); // Fecha a janela em 10 segundos

			QString httpResponse = "HTTP/1.1 400 Bad Request\r\nContent-Type: text/html; charset=utf-8\r\n\r\n" + errorPage;
			clientSocket->write(httpResponse.toUtf8());
			emit authenticationFinished(false);
		}
		// Lida com requisições OPTIONS (pre-flight) para CORS
		else if (method == "OPTIONS") {
			QString httpResponse = "HTTP/1.1 204 No Content\r\n"
					       "Access-Control-Allow-Origin: *\r\n"
					       "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"
					       "Access-Control-Allow-Headers: Content-Type\r\n"
					       "Access-Control-Max-Age: 86400\r\n"
					       "\r\n";
			clientSocket->write(httpResponse.toUtf8());
		}
		else {
			// Responde a qualquer outra requisição (ex: /favicon.ico)
			QString httpResponse = "HTTP/1.1 404 Not Found\r\n\r\n";
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