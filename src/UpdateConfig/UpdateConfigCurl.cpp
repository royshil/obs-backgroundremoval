// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UpdateConfig.hpp"

#include <limits>
#include <mutex>
#include <vector>

#include <curl/curl.h>

#include <obs-module.h>

#ifdef __APPLE__
#include <jthread.hpp>
using jthread = josuttis::jthread;
#else
#include <thread>
using jthread = std::jthread;
#endif

#ifndef OBS_LOG_HEADER
#define OBS_LOG_HEADER ""
#endif

namespace UpdateConfig {
namespace {

std::size_t writeCallback(void *contents, std::size_t size, std::size_t nmemb, void *userp) noexcept
{
	if (size != 0 && nmemb > std::numeric_limits<std::size_t>::max() / size) {
		return CURL_WRITEFUNC_ERROR;
	}

	const char *start = static_cast<const char *>(contents);
	auto *response = static_cast<std::vector<char> *>(userp);
	try {
		response->insert(response->end(), start, start + size * nmemb);
		return size * nmemb;
	} catch (...) {
		return CURL_WRITEFUNC_ERROR;
	}
}

class CurlLatestVersionClient final : public LatestVersionClient {
public:
	struct CurlGlobal final {
		const CURLcode result{curl_global_init(CURL_GLOBAL_DEFAULT)};

		CurlGlobal() noexcept
		{
			if (result != CURLE_OK) {
				blog(LOG_ERROR, OBS_LOG_HEADER "Failed to initialize libcurl: %s",
				     curl_easy_strerror(result));
			}
		}

		~CurlGlobal() noexcept
		{
			if (result == CURLE_OK) {
				curl_global_cleanup();
			}
		}
	};

	CurlGlobal curlGlobal_;
	std::mutex responseMutex_;
	std::vector<char> response_;
	bool responseReady_{false};
	std::mutex fetchThreadMutex_;
	std::optional<jthread> fetchThread_;

	~CurlLatestVersionClient() override = default;

	void fetchAsync(std::string url, std::string userAgent) override
	{
		std::lock_guard lock(fetchThreadMutex_);
		if (!fetchThread_ || !fetchThread_->joinable()) {
			fetchThread_.emplace(&CurlLatestVersionClient::fetch, this, std::move(url),
					     std::move(userAgent));
		}
	}

	std::optional<std::string> getLatestVersion() override
	{
		std::lock_guard lock(responseMutex_);
		if (!responseReady_) {
			return std::nullopt;
		}

		auto end = response_.end();
		while (end != response_.begin()) {
			switch (end[-1]) {
			case ' ':
			case '\t':
			case '\n':
			case '\r':
				--end;
				continue;
			}
			break;
		}
		return std::string{response_.begin(), end};
	}

	void reset() noexcept override
	{
		std::lock_guard fetchLock(fetchThreadMutex_);
		fetchThread_.reset();

		std::lock_guard responseLock(responseMutex_);
		responseReady_ = false;
		response_.clear();
	}

private:
	void fetch(std::string url, std::string userAgent) noexcept
	{
		if (curlGlobal_.result != CURLE_OK) {
			return;
		}

		CURL *curl = curl_easy_init();
		if (!curl) {
			blog(LOG_WARNING, OBS_LOG_HEADER "Failed to create cURL easy handle");
			return;
		}
		std::unique_ptr<CURL, void (*)(CURL *)> curl_(curl, &curl_easy_cleanup);

		std::vector<char> fetchedResponse;
		curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
		curl_easy_setopt(curl, CURLOPT_USERAGENT, userAgent.c_str());
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &writeCallback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &fetchedResponse);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
		curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 60L);
		curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);
		curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

		const CURLcode performResult = curl_easy_perform(curl);
		if (performResult != CURLE_OK) {
			blog(LOG_WARNING, OBS_LOG_HEADER "Failed to fetch latest-version.txt: %s",
			     curl_easy_strerror(performResult));
			return;
		}

		long statusCode = 0;
		const CURLcode infoResult = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);
		if (infoResult != CURLE_OK) {
			blog(LOG_WARNING, OBS_LOG_HEADER "Failed to get update-check HTTP status: %s",
			     curl_easy_strerror(infoResult));
		} else if (statusCode / 100 != 2) {
			blog(LOG_WARNING, OBS_LOG_HEADER "latest-version.txt request returned HTTP status %ld",
			     statusCode);
		} else {
			std::lock_guard lock(responseMutex_);
			response_.swap(fetchedResponse);
			responseReady_ = true;
		}
	}
};

} // namespace

std::unique_ptr<LatestVersionClient> createLatestVersionClient()
{
	return std::make_unique<CurlLatestVersionClient>();
}

} // namespace UpdateConfig
