#include "nightbot-api.h"
#include "nightbot-auth.h"

#include <obs-module.h>
#include <curl/curl.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonParseError>
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

static HttpResponse PerformRequest(const HttpRequest &request)
{
	const std::string &access_token = NightbotAuth::get().GetAccessToken();
	if (access_token.empty()) {
		blog(LOG_WARNING, "[Nightbot SR/API] "
				  "Attempt to make %s request without an access token.", request.method.c_str());
		return {-1, "", true, "No access token"};
	}

	CURL *curl = curl_easy_init();
	if (!curl) {
		blog(LOG_ERROR, "[Nightbot SR/API] Failed to initialize libcurl.");
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
		blog(LOG_ERROR,
		     "[Nightbot SR/API] %s request to '%s' failed: %s",
		     request.method.c_str(), request.url.c_str(), response.error_message.c_str());
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
	blog(LOG_INFO, "[Nightbot SR/API] Fetching user info...");

	HttpRequest request = { "https://api.nightbot.tv/1/me" };
	auto response = PerformRequest(request);

	if (response.http_code == 200) {
		QThreadPool::globalInstance()->start([this, body = response.body]() {
			QJsonParseError parseError;
			QByteArray response_data = QString::fromStdString(body).toUtf8();
			QJsonDocument doc = QJsonDocument::fromJson(response_data, &parseError);

			if (doc.isNull()) {
				blog(LOG_ERROR,
					 "[Nightbot SR/API] Failed to parse user info response: %s",
					 parseError.errorString().toUtf8().constData());
				emit userInfoFetched("");
				return;
			}

			QJsonObject rootObj = doc.object();
			if (rootObj.contains("user") && rootObj.value("user").isObject()) {
				QJsonObject userObj = rootObj.value("user").toObject();
				QString display_name = userObj["displayName"].toString();

				blog(LOG_INFO, "[Nightbot SR/API] Fetched user: %s",
					 display_name.toUtf8().constData());
				emit userInfoFetched(display_name);
			} else {
				emit userInfoFetched("");
			}
		});
	} else {
		blog(LOG_WARNING,
		     "[Nightbot SR/API] User info fetch failed with HTTP status %ld.",
		     response.http_code);
		emit userInfoFetched("");
	}
}

void NightbotAPI::FetchSongQueue()
{
	QThreadPool::globalInstance()->start([this]() {
		QList<SongItem> song_queue;

		HttpRequest request = { "https://api.nightbot.tv/1/song_requests/queue" };
		auto response = PerformRequest(request);

		if (response.http_code == 200) {
			QJsonParseError parseError;
			QJsonDocument doc = QJsonDocument::fromJson(
				response.body.c_str(), &parseError);

			if (doc.isNull() || !doc.isObject()) {
				blog(LOG_WARNING, "[Nightbot SR/API] Failed to parse song queue response.");
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
						item.user = obs_module_text("Nightbot.Queue.PlaylistUser");
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
			blog(LOG_WARNING,
			     "[Nightbot SR/API] User song queue fetch failed with HTTP status %ld.",
			     response.http_code);
		}

		emit songQueueFetched(song_queue);
	});
}

void NightbotAPI::ControlPlay()
{
	QThreadPool::globalInstance()->start([this]() {
		blog(LOG_INFO, "[Nightbot SR/API] Sending PLAY command...");
		const std::string url =
			"https://api.nightbot.tv/1/song_requests/queue/play";
		HttpRequest request = { url, "POST" };
		auto response = PerformRequest(request);

		if (response.http_code >= 200 && response.http_code < 300) {
			blog(LOG_INFO, "[Nightbot SR/API] PLAY command successful.");
		} else {
			blog(LOG_WARNING,
			     "[Nightbot SR/API] PLAY command failed with HTTP status %ld.",
			     response.http_code);
		}
	});
}

void NightbotAPI::ControlPause()
{
	QThreadPool::globalInstance()->start([this]() {
		blog(LOG_INFO, "[Nightbot SR/API] Sending PAUSE command...");
		const std::string url =
			"https://api.nightbot.tv/1/song_requests/queue/pause";
		HttpRequest request = { url, "POST" };
		auto response = PerformRequest(request);

		if (response.http_code >= 200 && response.http_code < 300) {
			blog(LOG_INFO, "[Nightbot SR/API] PAUSE command successful.");
		} else {
			blog(LOG_WARNING,
			     "[Nightbot SR/API] PAUSE command failed with HTTP status %ld.",
			     response.http_code);
		}
	});
}

void NightbotAPI::ControlSkip()
{
	QThreadPool::globalInstance()->start([this]() {
		blog(LOG_INFO, "[Nightbot SR/API] Sending SKIP command...");
		const std::string url = "https://api.nightbot.tv/1/song_requests/queue/skip";
		HttpRequest request = { url, "POST" };
		std::ignore = PerformRequest(request);
	});
}

void NightbotAPI::DeleteSong(const QString &songId)
{
	if (songId.isEmpty())
		return;

	QThreadPool::globalInstance()->start([songId]() {
		blog(LOG_INFO, "[Nightbot SR/API] Deleting song with ID: %s", songId.toUtf8().constData());
		std::string url = "https://api.nightbot.tv/1/song_requests/queue/" + songId.toStdString();
		HttpRequest request = { url, "DELETE" };
		std::ignore = PerformRequest(request);
	});
}

void NightbotAPI::SetSREnabled(bool enabled)
{
	QThreadPool::globalInstance()->start([this, enabled]() {
		blog(LOG_INFO, "[Nightbot SR/API] Setting Song Requests to %s...",
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
		blog(LOG_INFO, "[Nightbot SR/API] Promoting song with ID: %s", songId.toUtf8().constData());
		std::string url = "https://api.nightbot.tv/1/song_requests/queue/" + songId.toStdString() + "/promote";
		HttpRequest request = { url, "POST" };
		std::ignore = PerformRequest(request);
	});
}
