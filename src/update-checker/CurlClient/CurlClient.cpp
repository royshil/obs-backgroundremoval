/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Kaito Udagawa umireon@kaito.tokyo
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * OBS Plugin: Portrait Background Removal / Virtual Green-screen and Low-Light Enhancement
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
