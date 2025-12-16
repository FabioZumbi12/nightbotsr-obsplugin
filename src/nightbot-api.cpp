#include "nightbot-api.h"
#include "nightbot-auth.h"

#include <obs-module.h>
#include <curl/curl.h>

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>

static size_t auth_curl_write_callback(void *contents, size_t size, size_t nmemb, void *userp)
{
	size_t realsize = size * nmemb;
	((std::string *)userp)->append((char *)contents, realsize);
	return realsize;
}

// Função auxiliar para realizar uma requisição GET genérica
static std::pair<long, std::string>
PerformGetRequest(const std::string &url_str)
{
	const std::string &access_token = NightbotAuth::get().GetAccessToken();
	if (access_token.empty()) {
		blog(LOG_WARNING,
		     "[Nightbot SR/API] Tentativa de fazer requisição GET sem um token de acesso.");
		return {-1, ""};
	}

	CURL *curl = curl_easy_init();
	if (!curl) {
		blog(LOG_ERROR, "[Nightbot SR/API] Failed to initialize libcurl.");
		return {-1, ""};
	}

	std::string readBuffer;
	std::string auth_header = "Authorization: Bearer " + access_token;
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, auth_header.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, auth_curl_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

	CURLcode res = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (res != CURLE_OK) {
		blog(LOG_ERROR,
		     "[Nightbot SR/API] GET request to '%s' failed: %s",
		     url_str.c_str(), curl_easy_strerror(res));
		http_code = -1; // Indicate a curl error
	}

	return {http_code, readBuffer};
}

// Função auxiliar para realizar uma requisição POST genérica
static std::pair<long, std::string>
PerformPostRequest(const std::string &url_str, const std::string &post_body)
{
	const std::string &access_token = NightbotAuth::get().GetAccessToken();
	if (access_token.empty()) {
		blog(LOG_WARNING,
		     "[Nightbot SR/API] Tentativa de fazer requisição POST sem um token de acesso.");
		return {-1, ""};
	}

	CURL *curl = curl_easy_init();
	if (!curl) {
		blog(LOG_ERROR, "[Nightbot SR/API] Failed to initialize libcurl.");
		return {-1, ""};
	}

	std::string readBuffer;
	std::string auth_header = "Authorization: Bearer " + access_token;
	struct curl_slist *headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, auth_header.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, url_str.c_str());
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_body.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, auth_curl_write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

	CURLcode res = curl_easy_perform(curl);
	long http_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

	if (res != CURLE_OK) {
		blog(LOG_ERROR,
		     "[Nightbot SR/API] POST request to '%s' failed: %s",
		     url_str.c_str(), curl_easy_strerror(res));
		http_code = -1; // Indica um erro do cURL
	}

	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	return {http_code, readBuffer};
}

NightbotAPI &NightbotAPI::get()
{
	static NightbotAPI instance;
	return instance;
}

NightbotAPI::NightbotAPI() {}

std::string NightbotAPI::FetchUserInfo()
{
	blog(LOG_INFO, "[Nightbot SR/API] Fetching user info...");

	const std::string url = "https://api.nightbot.tv/1/me";
	auto [http_code, response_body] = PerformGetRequest(url);

	if (http_code == 200) {
		QJsonParseError parseError;
		// Converte a resposta para QByteArray usando um método que trata UTF-8 explicitamente
		QByteArray response_data =
			QString::fromUtf8(response_body.c_str()).toUtf8();
		QJsonDocument doc = QJsonDocument::fromJson(response_data, &parseError);

		if (doc.isNull()) {
			blog(LOG_ERROR,
			     "[Nightbot SR/API] Failed to parse user info response: %s",
			     parseError.errorString().toUtf8().constData());
		} else if (doc.isObject()) {
			QJsonObject userObj = doc.object()["user"].toObject();
			std::string display_name =
				userObj["displayName"].toString().toStdString();

			blog(LOG_INFO, "[Nightbot SR/API] Fetched user: %s",
			     display_name.c_str());
			return display_name;
		}
	} else {
		blog(LOG_WARNING,
		     "[Nightbot SR/API] User info fetch failed with HTTP status %ld.",
		     http_code);
	}
	return "";
}