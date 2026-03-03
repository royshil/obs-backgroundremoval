/*
 * SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
 * SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <obs-module.h>

#include "plugin-support.h"

#include "update-checker/update-checker.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE(PLUGIN_NAME, "en-US")

MODULE_EXPORT const char *obs_module_description(void)
{
	return obs_module_text("PortraitBackgroundFilterPlugin");
}

extern struct obs_source_info background_removal_filter_info;
extern struct obs_source_info enhance_filter_info;

bool obs_module_load(void)
{
	obs_register_source(&background_removal_filter_info);
	obs_register_source(&enhance_filter_info);
	obs_log(LOG_INFO, "Plugin loaded successfully (version %s)", PLUGIN_VERSION);

	check_update();

	return true;
}

void obs_module_unload()
{
	obs_log(LOG_INFO, "plugin unloaded");
}
