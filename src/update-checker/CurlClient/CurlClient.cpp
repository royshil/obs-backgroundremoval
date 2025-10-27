#include <string>
#include <functional>

#include <curl/curl.h>

#include <obs.h>
#include <plugin-support.h>

const std::string userAgent = std::string(PLUGIN_NAME) + "/" + PLUGIN_VERSION;

static std::size_t writeFunc(void *ptr, std::size_t size, size_t nmemb, std::string *data)
{
	data->append(static_cast<char *>(ptr), size * nmemb);
	return size * nmemb;
}

void fetchStringFromUrl(const char *urlString, std::function<void(std::string, int)> callback)
{
	CURL *curl = curl_easy_init();
	if (!curl) {
		obs_log(LOG_INFO, "Failed to initialize curl");
		callback("", CURL_LAST);
		return;
	}

	std::string responseBody;
	curl_easy_setopt(curl, CURLOPT_URL, urlString);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFunc);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);

	CURLcode code = curl_easy_perform(curl);
	curl_easy_cleanup(curl);

	if (code == CURLE_OK) {
		callback(responseBody, 0);
	} else {
		obs_log(LOG_INFO, "Failed to get latest release info");
		callback("", code);
	}
}
