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

#include "obs-utils.h"

#include <obs-module.h>

/**
  * @brief Get RGBA from the stage surface
  *
  * @param tf  The filter data
  * @param width  The width of the stage surface (output)
  * @param height  The height of the stage surface (output)
  * @return true  if successful
  * @return false if unsuccessful
*/
bool getRGBAFromStageSurface(filter_data *tf, uint32_t &width, uint32_t &height)
{

	if (!obs_source_enabled(tf->source)) {
		return false;
	}

	obs_source_t *target = obs_filter_get_target(tf->source);
	if (!target) {
		return false;
	}
	width = obs_source_get_base_width(target);
	height = obs_source_get_base_height(target);
	if (width == 0 || height == 0) {
		return false;
	}
	gs_texrender_reset(tf->texrender);
	if (!gs_texrender_begin(tf->texrender, width, height)) {
		return false;
	}
	struct vec4 background;
	vec4_zero(&background);
	gs_clear(GS_CLEAR_COLOR, &background, 0.0f, 0);
	gs_ortho(0.0f, static_cast<float>(width), 0.0f, static_cast<float>(height), -100.0f, 100.0f);
	gs_blend_state_push();
	gs_blend_function(GS_BLEND_ONE, GS_BLEND_ZERO);
	obs_source_video_render(target);
	gs_blend_state_pop();
	gs_texrender_end(tf->texrender);

	if (tf->stagesurface) {
		uint32_t stagesurf_width = gs_stagesurface_get_width(tf->stagesurface);
		uint32_t stagesurf_height = gs_stagesurface_get_height(tf->stagesurface);
		if (stagesurf_width != width || stagesurf_height != height) {
			gs_stagesurface_destroy(tf->stagesurface);
			tf->stagesurface = nullptr;
		}
	}
	if (!tf->stagesurface) {
		tf->stagesurface = gs_stagesurface_create(width, height, GS_BGRA);
	}
	gs_stage_texture(tf->stagesurface, gs_texrender_get_texture(tf->texrender));
	uint8_t *video_data;
	uint32_t linesize;
	if (!gs_stagesurface_map(tf->stagesurface, &video_data, &linesize)) {
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(tf->inputBGRALock);
		// Create a temporary Mat that wraps the video_data pointer
		cv::Mat temp(height, width, CV_8UC4, video_data, linesize);
		// Clone the data to ensure tf->inputBGRA has its own copy
		// This prevents use-after-unmap race condition
		tf->inputBGRA = temp.clone();
	}
	gs_stagesurface_unmap(tf->stagesurface);
	return true;
}
