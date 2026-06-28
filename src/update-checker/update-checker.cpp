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
#include <regex>
#include <algorithm>

extern "C" const char *PLUGIN_VERSION;

static std::string latestVersionForUpdate;
static std::mutex latestVersionMutex;

static bool is_newer_version(const std::string& remote, const std::string& local)
{
	std::regex version_regex(R"(^(\d+)\.(\d+)\.(\d+)(?:-([a-zA-Z]+)(?:\.?(\d+))?)?$)");
	std::smatch remote_match;
	std::smatch local_match;

	bool remote_valid = std::regex_match(remote, remote_match, version_regex);
	bool local_valid = std::regex_match(local, local_match, version_regex);

	if (!remote_valid || !local_valid) {
		// Fallback to basic string comparison if regex fails
		return remote > local;
	}

	int remote_major = std::stoi(remote_match[1].str());
	int remote_minor = std::stoi(remote_match[2].str());
	int remote_patch = std::stoi(remote_match[3].str());

	int local_major = std::stoi(local_match[1].str());
	int local_minor = std::stoi(local_match[2].str());
	int local_patch = std::stoi(local_match[3].str());

	if (remote_major != local_major) {
		return remote_major > local_major;
	}
	if (remote_minor != local_minor) {
		return remote_minor > local_minor;
	}
	return remote_patch > local_patch;
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
