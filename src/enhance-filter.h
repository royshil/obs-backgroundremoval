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

#include <obs-module.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *enhance_filter_getname(void *unused);
void *enhance_filter_create(obs_data_t *settings, obs_source_t *source);
void enhance_filter_destroy(void *data);
void enhance_filter_defaults(obs_data_t *settings);
obs_properties_t *enhance_filter_properties(void *data);
void enhance_filter_update(void *data, obs_data_t *settings);
void enhance_filter_activate(void *data);
void enhance_filter_deactivate(void *data);
void enhance_filter_video_tick(void *data, float seconds);
void enhance_filter_video_render(void *data, gs_effect_t *_effect);

#ifdef __cplusplus
}
#endif
