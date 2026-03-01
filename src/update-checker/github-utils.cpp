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

#include <cstddef>
#include <string>

#include <obs.h>

#include "Client.hpp"
#include "github-utils.h"
#include "plugin-support.h"

static const std::string GITHUB_LATEST_RELEASE_URL =
	"https://api.github.com/repos/royshil/obs-backgroundremoval/releases/latest";

void github_utils_get_release_information(std::function<void(github_utils_release_information)> callback)
{
	fetchStringFromUrl(GITHUB_LATEST_RELEASE_URL.c_str(), [callback](std::string responseBody, int code) {
		if (code != 0)
			return;
		// Parse the JSON response
		obs_data_t *data = obs_data_create_from_json(responseBody.c_str());
		if (!data) {
			obs_log(LOG_INFO, "Failed to parse latest release info");
			callback({OBS_BGREMOVAL_GITHUB_UTILS_ERROR, "", ""});
			return;
		}

		// The version is in the "tag_name" property
		std::string version = obs_data_get_string(data, "tag_name");
		std::string body = obs_data_get_string(data, "body");
		obs_data_release(data);

		// remove the "v" prefix in version, if it exists
		if (version[0] == 'v') {
			version = version.substr(1);
		}

		callback({OBS_BGREMOVAL_GITHUB_UTILS_SUCCESS, body, version});
	});
}
