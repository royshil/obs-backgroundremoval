// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <mutex>
#include <string_view>

#include <QResource>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include "UI/AboutDialogIntegration.hpp"
#include "UpdateConfig/UpdateConfig.hpp"
#include "BackgroundRemoval/macos/FilterContext.hpp"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

namespace {

void showStartupDialogHandler(enum obs_frontend_event event, void *);

void fetchLatestVersionIfEnabled()
{
	UpdateConfig::Request request = UpdateConfig::GetCheckForUpdatesEnabledRequest{};
	if (UpdateConfig::doRequest(request)) {
		UpdateConfig::fetchLatestVersionAsync();
	}
}

bool setStartupCallbackRegistration(bool registered)
{
	static std::mutex mutex;
	static bool isRegistered = false;
	const std::lock_guard lock(mutex);

	if (isRegistered == registered) {
		return false;
	}

	if (registered) {
		obs_frontend_add_event_callback(showStartupDialogHandler, nullptr);
	} else {
		obs_frontend_remove_event_callback(showStartupDialogHandler, nullptr);
	}
	isRegistered = registered;
	return true;
}

void showStartupDialogHandler(enum obs_frontend_event event, void *)
{
	if (event != OBS_FRONTEND_EVENT_FINISHED_LOADING) {
		return;
	}

	if (!setStartupCallbackRegistration(false)) {
		return;
	}

	if (!AboutDialogIntegration::show()) {
		fetchLatestVersionIfEnabled();
	}
}

} // namespace

bool obs_module_load(void)
{
	static obs_source_info background_removal_source_info = BackgroundRemoval::FilterContext::getSourceInfo();

	const std::string_view registeringSourceId = background_removal_source_info.id;

	const char *sourceId = nullptr;
	for (std::size_t i = 0; obs_enum_source_types(i, &sourceId); i++) {
		if (sourceId && registeringSourceId == sourceId) {
			blog(LOG_ERROR, OBS_LOG_HEADER "Source ID '%s' is already registered by another plugin",
			     sourceId);
			return false;
		}
	}

	UpdateConfig::latestVersionClient();

	static struct GlobalQResourceGuard {
		GlobalQResourceGuard() { Q_INIT_RESOURCE(resources); }
		~GlobalQResourceGuard() { Q_CLEANUP_RESOURCE(resources); }
	} globalQResourceGuard;

	obs_register_source(&background_removal_source_info);

	static struct GlobalStartupGuard {
		bool scheduled{false};
		GlobalStartupGuard()
		{
			UpdateConfig::Request request = UpdateConfig::HandleFirstRunRequest{PLUGIN_VERSION_STR};
			scheduled = UpdateConfig::doRequest(request) && setStartupCallbackRegistration(true);
		}
		~GlobalStartupGuard() { setStartupCallbackRegistration(false); }
	} globalStartupGuard;

	if (!globalStartupGuard.scheduled) {
		fetchLatestVersionIfEnabled();
	}

	blog(LOG_INFO, OBS_LOG_HEADER "Plugin loaded successfully (version %s)", PLUGIN_VERSION_STR);

	return true;
}

void obs_module_unload()
{
	blog(LOG_INFO, OBS_LOG_HEADER "plugin unloaded");
}
