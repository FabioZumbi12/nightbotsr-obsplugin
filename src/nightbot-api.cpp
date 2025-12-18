#include <curl/curl.h>

#include "nightbot-api.h"
#include "nightbot-auth.h"
#include "plugin-support.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
#include <QString>
#include <QtConcurrent/QtConcurrent>
#include <QThreadPool>

static size_t auth_curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	((std::string *)userp)->append((char *)contents, realsize);
	return realsize;
}

struct HttpRequest {
	std::string url;
	std::string method = "GET";
	std::string body;
	std::vector<std::string> headers;
};

struct HttpResponse {
	long http_code = 0;
	std::string body;
	bool curl_error = false;
	std::string error_message;
};

static HttpResponse PerformRequest(const HttpRequest &request, bool is_retry = false)
{
	const std::string &access_token = NightbotAuth::get().GetAccessToken();
	if (access_token.empty()) {
		obs_log_warning("[Nightbot SR/API] "
				  "Attempt to make %s request without an access token.", request.method.c_str());
		return {-1, "", true, "No access token"};
	}

	CURL *curl = curl_easy_init();
	if (!curl) {
		obs_log_error("[Nightbot SR/API] Failed to initialize libcurl.");
		return {-1, "", true, "cURL init failed"};
	}

	HttpResponse response;
	std::string auth_header = "Authorization: Bearer " + access_token;
	struct curl_slist *headers_list = NULL;
	headers_list = curl_slist_append(headers_list, auth_header.c_str());
	for(const auto& header : request.headers) {
		headers_list = curl_slist_append(headers_list, header.c_str());
	}

	curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers_list);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, auth_curl_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request.method.c_str());

	if (request.method == "POST" || request.method == "PUT") {
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
	}

	CURLcode res = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.http_code);

	curl_slist_free_all(headers_list);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK) {
		response.curl_error = true;
		response.error_message = curl_easy_strerror(res);
		response.http_code = -1; // Internal error code for cURL failure
		obs_log_error(
			 "[Nightbot SR/API] %s request to '%s' failed: %s",
			 request.method.c_str(), request.url.c_str(), response.error_message.c_str());
		QMetaObject::invokeMethod(&NightbotAPI::get(), "apiErrorOccurred", Qt::QueuedConnection,
					  Q_ARG(QString, response.error_message.c_str()));
	}

	if (response.http_code == 401 && !is_retry) {
		obs_log_info("[Nightbot SR/API] Received 401 Unauthorized. Attempting to refresh token...");
		if (NightbotAuth::get().RefreshToken()) {
			obs_log_info(
				"[Nightbot SR/API] Token refreshed. Retrying original request...");
			return PerformRequest(request, true); // Tenta novamente, apenas uma vez
		} else {
			obs_log_warning("[Nightbot SR/API] Token refresh failed. Aborting request.");
		}
	} else if (response.http_code >= 400 && response.http_code != 401) {
		obs_log_warning(
			"[Nightbot SR/API] Request to '%s' failed with HTTP status %ld.",
			request.url.c_str(), response.http_code);
		QMetaObject::invokeMethod(&NightbotAPI::get(), "apiErrorOccurred", Qt::QueuedConnection,
					  Q_ARG(QString, "API Error: " + QString::number(response.http_code)));
	}

	return response;
}

NightbotAPI &NightbotAPI::get()
{
	static NightbotAPI instance;
	return instance;
}

NightbotAPI::NightbotAPI() {}

void NightbotAPI::FetchUserInfo()
{
	obs_log_info("[Nightbot SR/API] Fetching user info...");

	HttpRequest request = { "https://api.nightbot.tv/1/me" };
	auto response = PerformRequest(request);

	if (response.http_code == 200) {
		QThreadPool::globalInstance()->start([this, body = response.body]() {
			QJsonParseError parseError;
			QByteArray response_data = QString::fromStdString(body).toUtf8();
			QJsonDocument doc = QJsonDocument::fromJson(response_data, &parseError);

			if (doc.isNull()) {
				obs_log_error(
					 "[Nightbot SR/API] Failed to parse user info response: %s",
					 parseError.errorString().toUtf8().constData());
				emit userInfoFetched("");
				return;
			}

			QJsonObject rootObj = doc.object();
			if (rootObj.contains("user") && rootObj.value("user").isObject()) {
				QJsonObject userObj = rootObj.value("user").toObject();
				QString display_name = userObj["displayName"].toString();

				obs_log_info("[Nightbot SR/API] Fetched user: %s",
					 display_name.toUtf8().constData());
				emit userInfoFetched(display_name);
			} else {
				emit userInfoFetched("");
			}
		});
	} else {
		obs_log_warning(
		     "[Nightbot SR/API] User info fetch failed with HTTP status %ld.",
		     response.http_code);
		emit userInfoFetched("");
	}
}

void NightbotAPI::FetchSongQueue(const QString &playlistUserText)
{
	QThreadPool::globalInstance()->start([this, playlistUserText]() {
		QList<SongItem> song_queue;

		HttpRequest request = { "https://api.nightbot.tv/1/song_requests/queue" };
		auto response = PerformRequest(request);

		if (response.http_code == 200) {
			QJsonParseError parseError; 
			QJsonDocument doc = QJsonDocument::fromJson(
				response.body.c_str(), &parseError);

			if (doc.isNull() || !doc.isObject()) {
				obs_log_warning("[Nightbot SR/API] Failed to parse song queue response.");
			} else {
				QJsonObject rootObj = doc.object();

				if (rootObj.contains("_requestsEnabled")) {
					bool isEnabled = rootObj["_requestsEnabled"].toBool();
					emit srStatusFetched(isEnabled);
				}

				if (rootObj.contains("_currentSong") && rootObj["_currentSong"].isObject()) {
					QJsonObject songObj = rootObj["_currentSong"].toObject();
					SongItem item;
					item.id = songObj["_id"].toString();
					item.position = 0;
					QJsonObject trackObj = songObj["track"].toObject();
					item.title = trackObj["title"].toString();
					item.artist = trackObj["artist"].toString();
					item.duration = trackObj["duration"].toInt();
					if (songObj.contains("user") && songObj["user"].isObject()) {
						item.user = songObj["user"].toObject()["displayName"].toString();
					} else {
						item.user = playlistUserText;
					}
					song_queue.append(item);
				}

				const auto queueArray = rootObj["queue"].toArray();
				for (const QJsonValue value : queueArray) {
					QJsonObject songObj = value.toObject();
					SongItem item;
					item.id = songObj["_id"].toString();
					item.position = songObj["_position"].toInt();
					QJsonObject trackObj = songObj["track"].toObject();
					item.title = trackObj["title"].toString();
					item.artist = trackObj["artist"].toString();
					item.duration = trackObj["duration"].toInt();
					item.user = songObj["user"].toObject()["displayName"].toString();
					song_queue.append(item);
				}
			}

			std::sort(song_queue.begin(), song_queue.end(),
				  [](const SongItem &a, const SongItem &b) {
					  return a.position < b.position;
				  });
		} else {
			obs_log_warning(
			     "[Nightbot SR/API] User song queue fetch failed with HTTP status %ld.",
			     response.http_code);
			QMetaObject::invokeMethod(this, "apiErrorOccurred", Qt::QueuedConnection,
						  Q_ARG(QString, "Failed to fetch song queue: " + QString::number(response.http_code)));
		}

		emit songQueueFetched(song_queue);
	});
}

void NightbotAPI::FetchSRSettings()
{
	QThreadPool::globalInstance()->start([this]() {
		HttpRequest request = { "https://api.nightbot.tv/1/song_requests" };
		auto response = PerformRequest(request);

		if (response.http_code == 200) {
			QJsonParseError parseError;
			QJsonDocument doc = QJsonDocument::fromJson(
				response.body.c_str(), &parseError);

			if (doc.isNull() || !doc.isObject()) {
				obs_log_warning("[Nightbot SR/API] Failed to parse SR settings response.");
				return;
			}

			QJsonObject rootObj = doc.object();
			if (rootObj.contains("settings") && rootObj["settings"].isObject()) {
				QJsonObject settingsObj = rootObj["settings"].toObject();
				if (settingsObj.contains("volume") && settingsObj["volume"].isDouble()) {
					int volume = settingsObj["volume"].toInt();
					emit volumeFetched(volume);
				}
			}
		} else {
			obs_log_warning(
				"[Nightbot SR/API] SR settings fetch failed with HTTP status %ld.",
				response.http_code);
		}
	});
}

void NightbotAPI::ControlPlay()
{
	QThreadPool::globalInstance()->start([this]() {
		obs_log_info("[Nightbot SR/API] Sending PLAY command...");
		const std::string url =
			"https://api.nightbot.tv/1/song_requests/queue/play";
		HttpRequest request = { url, "POST" };
		auto response = PerformRequest(request);

		if (response.http_code >= 200 && response.http_code < 300) {
			obs_log_info("[Nightbot SR/API] PLAY command successful.");
		} else {
			obs_log_warning(
			     "[Nightbot SR/API] PLAY command failed with HTTP status %ld.",
			     response.http_code);
		}
	});
}

void NightbotAPI::ControlPause()
{
	QThreadPool::globalInstance()->start([this]() {
		obs_log_info("[Nightbot SR/API] Sending PAUSE command...");
		const std::string url =
			"https://api.nightbot.tv/1/song_requests/queue/pause";
		HttpRequest request = { url, "POST" };
		auto response = PerformRequest(request);

		if (response.http_code >= 200 && response.http_code < 300) {
			obs_log_info("[Nightbot SR/API] PAUSE command successful.");
		} else {
			obs_log_warning(
			     "[Nightbot SR/API] PAUSE command failed with HTTP status %ld.",
			     response.http_code);
		}
	});
}

void NightbotAPI::ControlSkip()
{
	QThreadPool::globalInstance()->start([this]() {
		obs_log_info("[Nightbot SR/API] Sending SKIP command...");
		const std::string url = "https://api.nightbot.tv/1/song_requests/queue/skip";
		HttpRequest request = { url, "POST" };
		std::ignore = PerformRequest(request);
	});
}

void NightbotAPI::SetVolume(int volume)
{
	QThreadPool::globalInstance()->start([this, volume]() {
		obs_log_info("[Nightbot SR/API] Setting volume to %d...", volume);
		const std::string url = "https://api.nightbot.tv/1/song_requests";

		QJsonObject body;
		body["volume"] = volume;
		QJsonDocument doc(body);
		std::string put_body =
			doc.toJson(QJsonDocument::Compact).toStdString();

		HttpRequest request = {url, "PUT", put_body};
		request.headers.push_back("Content-Type: application/json");

		std::ignore = PerformRequest(request);
	});
}

void NightbotAPI::DeleteSong(const QString &songId)
{
	if (songId.isEmpty())
		return;

	QThreadPool::globalInstance()->start([songId]() {
		obs_log_info("[Nightbot SR/API] Deleting song with ID: %s", songId.toUtf8().constData());
		std::string url = "https://api.nightbot.tv/1/song_requests/queue/" + songId.toStdString();
		HttpRequest request = { url, "DELETE" };
		std::ignore = PerformRequest(request);
	});
}

void NightbotAPI::AddSong(const QString &query)
{
	QThreadPool::globalInstance()->start([this, query]() {
		obs_log_info("[Nightbot SR/API] Adding song with query: %s",
			     query.toUtf8().constData());

		QJsonObject body;
		body["q"] = query;
		QJsonDocument doc(body);
		std::string post_body =
			doc.toJson(QJsonDocument::Compact).toStdString();

		HttpRequest request = {
			"https://api.nightbot.tv/1/song_requests/queue", "POST",
			post_body};
		request.headers.push_back("Content-Type: application/json");

		auto response = PerformRequest(request);

		if (response.http_code == 200) {
			emit songAdded(true, "");
		} else {
			QJsonParseError parseError;
			QJsonDocument errorDoc = QJsonDocument::fromJson(
				QByteArray::fromStdString(response.body),
				&parseError);
			QString finalErrorMsg;

			if (!errorDoc.isNull() && errorDoc.isObject() &&
			    errorDoc.object().contains("message")) {
				finalErrorMsg = errorDoc.object()["message"].toString();
			} else {
				finalErrorMsg = QString::fromStdString(response.body);
			}
			emit songAdded(false, "Error: " + finalErrorMsg);
		}
	});
}

void NightbotAPI::SetSREnabled(bool enabled)
{
	QThreadPool::globalInstance()->start([this, enabled]() {
		obs_log_info("[Nightbot SR/API] Setting Song Requests to %s...",
		     enabled ? "Enabled" : "Disabled");
		const std::string url = "https://api.nightbot.tv/1/song_requests";

		QJsonObject body;
		body["enabled"] = enabled;
		QJsonDocument doc(body);
		std::string put_body = doc.toJson(QJsonDocument::Compact).toStdString();

		HttpRequest request = { url, "PUT", put_body };
		request.headers.push_back("Content-Type: application/json");

		std::ignore = PerformRequest(request);
	});
}

void NightbotAPI::PromoteSong(const QString &songId)
{
	if (songId.isEmpty())
		return;

	QThreadPool::globalInstance()->start([songId]() {
		obs_log_info("[Nightbot SR/API] Promoting song with ID: %s", songId.toUtf8().constData());
		std::string url = "https://api.nightbot.tv/1/song_requests/queue/" + songId.toStdString() + "/promote";
		HttpRequest request = { url, "POST" };
		std::ignore = PerformRequest(request);
	});
}
