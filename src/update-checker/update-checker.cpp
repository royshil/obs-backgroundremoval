#include "update-checker.h"
#include "github-utils.h"
#include "obs-utils/obs-config-utils.h"

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <plugin-support.h>

extern "C" const char *PLUGIN_VERSION;

static std::string latestVersionForUpdate;

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
			latestVersionForUpdate.clear();
			return;
		}

		latestVersionForUpdate = info.version;
	};

	github_utils_get_release_information(callback);
}

const char *get_latest_version(void)
{
	obs_log(LOG_INFO, "get_latest_version: %s", latestVersionForUpdate.c_str());
	if (latestVersionForUpdate.empty()) {
		return nullptr;
	}
	return latestVersionForUpdate.c_str();
}
