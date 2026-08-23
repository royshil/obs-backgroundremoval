// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UpdateConfig.hpp"

#include <memory>
#include <mutex>

#include <obs-module.h>
#include <util/base.h>
#include <util/config-file.h>
#include <util/platform.h>

#ifndef OBS_LOG_HEADER
#define OBS_LOG_HEADER ""
#endif

namespace UpdateConfig {

bool doRequest(Request &request)
{
	static std::mutex mutex;

	std::unique_ptr<char, void (*)(void *)> configDirectory(obs_module_config_path(nullptr), &bfree);
	if (!configDirectory || configDirectory.get()[0] == '\0') {
		blog(LOG_ERROR, OBS_LOG_HEADER "Invalid config directory path (empty)");
		return false;
	}

	std::unique_ptr<char, void (*)(void *)> updateConfigPath(obs_module_config_path("update.ini"), &bfree);
	if (!updateConfigPath || updateConfigPath.get()[0] == '\0') {
		blog(LOG_ERROR, OBS_LOG_HEADER "Invalid update config path (empty)");
		return false;
	}

	if (os_mkdirs(configDirectory.get()) == MKDIR_ERROR) {
		blog(LOG_WARNING, OBS_LOG_HEADER "Failed to create config directory");
		return false;
	}

	auto *firstRunRequest = std::get_if<HandleFirstRunRequest>(&request);
	if (firstRunRequest && firstRunRequest->pluginVersion.empty()) {
		blog(LOG_ERROR, OBS_LOG_HEADER "Invalid plugin version (empty)");
		return false;
	}

	const std::lock_guard lock(mutex);

	config_t *config = nullptr;
	if (config_open(&config, updateConfigPath.get(), CONFIG_OPEN_ALWAYS) != CONFIG_SUCCESS) {
		blog(LOG_WARNING, OBS_LOG_HEADER "Failed to open update config");
		return false;
	}
	std::unique_ptr<config_t, void (*)(config_t *)> config_(config, &config_close);

	config_set_default_bool(config, "update", "check_for_updates", true);

	if (firstRunRequest) {
		const char *storedVersion = config_get_string(config, "update", "version");
		if (!storedVersion || firstRunRequest->pluginVersion != storedVersion) {
			if (!config_has_user_value(config, "update", "check_for_updates")) {
				config_set_bool(config, "update", "check_for_updates", true);
			}
			config_set_string(config, "update", "version", firstRunRequest->pluginVersion.c_str());
		} else {
			return false;
		}
	} else if (auto *flagRequest = std::get_if<SetCheckForUpdatesFlagRequest>(&request)) {
		config_set_bool(config, "update", "check_for_updates", flagRequest->enabled);
	} else if (std::holds_alternative<GetCheckForUpdatesEnabledRequest>(request)) {
		return config_get_bool(config, "update", "check_for_updates");
	} else {
		blog(LOG_ERROR, OBS_LOG_HEADER "FATAL ERROR: never reach here");
		return false;
	}

	if (config_save_safe(config, "tmp", nullptr) == CONFIG_SUCCESS) {
		return true;
	} else {
		blog(LOG_WARNING, OBS_LOG_HEADER "Failed to save update config");
		return false;
	}
}

LatestVersionClient &latestVersionClient()
{
	static auto client = createLatestVersionClient();
	return *client;
}

void fetchLatestVersionAsync(std::string url, std::string userAgent)
{
	latestVersionClient().fetchAsync(std::move(url), std::move(userAgent));
}

std::optional<std::string> getLatestVersion()
{
	return latestVersionClient().getLatestVersion();
}

void resetLatestVersion() noexcept
{
	latestVersionClient().reset();
}

} // namespace UpdateConfig
