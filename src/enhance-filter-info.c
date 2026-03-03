/*
 * SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
 * SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "enhance-filter.h"

struct obs_source_info enhance_filter_info = {
	.id = "enhanceportrait",
	.type = OBS_SOURCE_TYPE_FILTER,
	.output_flags = OBS_SOURCE_VIDEO,
	.get_name = enhance_filter_getname,
	.create = enhance_filter_create,
	.destroy = enhance_filter_destroy,
	.get_defaults = enhance_filter_defaults,
	.get_properties = enhance_filter_properties,
	.update = enhance_filter_update,
	.activate = enhance_filter_activate,
	.deactivate = enhance_filter_deactivate,
	.video_tick = enhance_filter_video_tick,
	.video_render = enhance_filter_video_render,
};
