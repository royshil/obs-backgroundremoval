/*
 * SPDX-FileCopyrightText: Copyright (C) 2021 Roy Shilkrot roy.shil@gmail.com
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
