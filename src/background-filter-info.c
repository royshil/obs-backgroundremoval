/*
 * SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
 * SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "background-filter.h"

struct obs_source_info background_removal_filter_info = {
	.id = "background_removal",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = background_filter_getname,
	.create = background_filter_create,
	.destroy = background_filter_destroy,
	.get_defaults = background_filter_defaults,
	.get_properties = background_filter_properties,
	.update = background_filter_update,
	.activate = background_filter_activate,
	.deactivate = background_filter_deactivate,
	.video_tick = background_filter_video_tick,
	.video_render = background_filter_video_render,
};
