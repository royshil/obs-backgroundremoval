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
