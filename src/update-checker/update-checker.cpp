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

#include "update-checker.h"
#include "github-utils.h"
#include "obs-utils/obs-config-utils.h"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <plugin-support.h>

#include <mutex>

extern "C" const char *PLUGIN_VERSION;

static std::string latestVersionForUpdate;
static std::mutex latestVersionMutex;

void check_update(void)
{
	bool shouldCheckForUpdates = false;
	if (getFlagFromConfig("check_for_updates", &shouldCheckForUpdates, true) != OBS_BGREMOVAL_CONFIG_SUCCESS) {
		// Failed to get the config value, assume it's enabled
		shouldCheckForUpdates = true;
		// store the default value
		setFlagInConfig("check_for_updates", shouldCheckForUpdates);
	}

	if (!shouldCheckForUpdates) {
		// Update checks are disabled
		return;
	}

	const auto callback = [](github_utils_release_information info) {
		if (info.responseCode != OBS_BGREMOVAL_GITHUB_UTILS_SUCCESS) {
			obs_log(LOG_INFO, "failed to get latest release information");
			return;
		}
		obs_log(LOG_INFO, "Latest release is %s", info.version.c_str());

		if (info.version == PLUGIN_VERSION) {
			// No update available, latest version is the same as the current version
			std::lock_guard<std::mutex> lock(latestVersionMutex);
			latestVersionForUpdate.clear();
			return;
		}

		std::lock_guard<std::mutex> lock(latestVersionMutex);
		latestVersionForUpdate = info.version;
	};

	github_utils_get_release_information(callback);
}

const char *get_latest_version(void)
{
	std::lock_guard<std::mutex> lock(latestVersionMutex);
	obs_log(LOG_INFO, "get_latest_version: %s", latestVersionForUpdate.c_str());
	if (latestVersionForUpdate.empty()) {
		return nullptr;
	}
	return latestVersionForUpdate.c_str();
}
