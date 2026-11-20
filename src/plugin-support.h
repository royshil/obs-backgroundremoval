/*
 * SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
 * SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <util/base.h>

#ifndef OBS_LOG_HEADER
#error OBS_LOG_HEADER must be defined by the build system
#endif

/*
 * Logs through blog() with the plugin's prefix, as obs_log() of the OBS plugin template does. The
 * format must be a string literal: the prefix is joined to it at compile time, so blog() checks the
 * arguments against it and nothing is allocated.
 */
#define obs_log(log_level, format, ...) blog(log_level, OBS_LOG_HEADER format, ##__VA_ARGS__)

#ifdef __cplusplus
extern "C" {
#endif

extern const char *PLUGIN_NAME;
extern const char *PLUGIN_VERSION;

#ifdef __cplusplus
}
#endif
