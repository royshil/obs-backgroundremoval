// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <mutex>

#include <array>
#include <string_view>

#include <QResource>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include "UI/AboutDialogIntegration.hpp"
#include "UpdateConfig/UpdateConfig.hpp"

#include "background-filter.h"
#include "enhance-filter.h"

#ifndef PLUGIN_VERSION_STR
#error PLUGIN_VERSION_STR must be defined by the build system
#endif

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

obs_source_info getBackgroundRemovalSourceInfo()
{
	obs_source_info si{};
	si.id = "background_removal";
	si.type = OBS_SOURCE_TYPE_FILTER;
	si.output_flags = OBS_SOURCE_VIDEO;
	si.get_name = background_filter_getname;
	si.create = background_filter_create;
	si.destroy = background_filter_destroy;
	si.get_defaults = background_filter_defaults;
	si.get_properties = background_filter_properties;
	si.update = background_filter_update;
	si.activate = background_filter_activate;
	si.deactivate = background_filter_deactivate;
	si.video_tick = background_filter_video_tick;
	si.video_render = background_filter_video_render;
	return si;
}

obs_source_info getEnhanceFilterSourceInfo()
{
	obs_source_info si{};
	si.id = "enhanceportrait";
	si.type = OBS_SOURCE_TYPE_FILTER;
	si.output_flags = OBS_SOURCE_VIDEO;
	si.get_name = enhance_filter_getname;
	si.create = enhance_filter_create;
	si.destroy = enhance_filter_destroy;
	si.get_defaults = enhance_filter_defaults;
	si.get_properties = enhance_filter_properties;
	si.update = enhance_filter_update;
	si.activate = enhance_filter_activate;
	si.deactivate = enhance_filter_deactivate;
	si.video_tick = enhance_filter_video_tick;
	si.video_render = enhance_filter_video_render;
	return si;
}

} // namespace

bool obs_module_load(void)
{
	static obs_source_info background_removal_source_info = getBackgroundRemovalSourceInfo();
	static obs_source_info enhance_filter_source_info = getEnhanceFilterSourceInfo();

	std::array<std::string_view, 2> registeringSourceIds{
		background_removal_source_info.id,
		enhance_filter_source_info.id,
	};

	const char *sourceId = nullptr;
	for (std::size_t i = 0; obs_enum_source_types(i, &sourceId); i++) {
		if (sourceId && (registeringSourceIds[0] == sourceId || registeringSourceIds[1] == sourceId)) {
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
	obs_register_source(&enhance_filter_source_info);

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
