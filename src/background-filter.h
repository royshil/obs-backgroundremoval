/*
 * SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
 * SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <obs-module.h>

#ifdef __cplusplus
extern "C" {
#endif

const char *background_filter_getname(void *unused);
void *background_filter_create(obs_data_t *settings, obs_source_t *source);
void background_filter_destroy(void *data);
void background_filter_defaults(obs_data_t *settings);
obs_properties_t *background_filter_properties(void *data);
void background_filter_update(void *data, obs_data_t *settings);
void background_filter_activate(void *data);
void background_filter_deactivate(void *data);
void background_filter_video_tick(void *data, float seconds);
void background_filter_video_render(void *data, gs_effect_t *_effect);

#ifdef __cplusplus
}
#endif
