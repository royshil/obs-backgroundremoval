// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "update-checker.h"
#include "github-utils.hpp"
#include "../obs-utils/obs-config-utils.hpp"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include "../plugin-support.h"

#include <mutex>
#include <sstream>
#include <vector>
#include <algorithm>

extern "C" const char *PLUGIN_VERSION;

static std::string latestVersionForUpdate;
static std::mutex latestVersionMutex;

static bool is_newer_version(const std::string& remote, const std::string& local)
{
	std::vector<int> remote_parts;
	std::vector<int> local_parts;
	std::string part;

	std::stringstream ss_remote(remote);
	while (std::getline(ss_remote, part, '.')) {
		try {
			remote_parts.push_back(std::stoi(part));
		} catch (...) {
			remote_parts.push_back(0);
		}
	}

	std::stringstream ss_local(local);
	while (std::getline(ss_local, part, '.')) {
		try {
			local_parts.push_back(std::stoi(part));
		} catch (...) {
			local_parts.push_back(0);
		}
	}

	size_t max_parts = std::max(remote_parts.size(), local_parts.size());
	for (size_t i = 0; i < max_parts; ++i) {
		int r = i < remote_parts.size() ? remote_parts[i] : 0;
		int l = i < local_parts.size() ? local_parts[i] : 0;
		if (r > l) return true;
		if (r < l) return false;
	}
	return false;
}

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

		if (!is_newer_version(info.version, PLUGIN_VERSION)) {
			// No update available, latest version is not newer than the current version
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
