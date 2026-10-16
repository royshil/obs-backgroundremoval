// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <obs-module.h>

#include <stdlib.h>

char *obs_module_get_config_path(obs_module_t *module, const char *file)
{
	(void)module;
	(void)file;
	abort();
}

bool obs_get_video_info(struct obs_video_info *ovi)
{
	(void)ovi;
	abort();
}
