/*
 * SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#ifndef PLUGIN_NAME
#define PLUGIN_NAME "obs-backgroundremoval"
#endif

#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "0.0.0"
#endif

#ifndef PLUGIN_DISPLAY_NAME
#define PLUGIN_DISPLAY_NAME "Background Removal"
#endif

#ifndef PLUGIN_LONG_DESCRIPTION
#define PLUGIN_LONG_DESCRIPTION                                                                                        \
    "A plugin for OBS Studio that allows you to replace the background in portrait images and video, as well as "      \
    "enhance low-light scenes"
#endif

#ifndef PLUGIN_AUTHOR
#define PLUGIN_AUTHOR "Roy Shilkrot"
#endif

#ifndef PLUGIN_LOG_HEADER
#define PLUGIN_LOG_HEADER "[" PLUGIN_NAME "] "
#endif

#ifndef PLUGIN_LATEST_VERSION_URL
#define PLUGIN_LATEST_VERSION_URL "https://royshil.github.io/obs-backgroundremoval/metadata/latest-version.txt"
#endif

#include <util/base.h>

#define obs_log(log_level, format, ...) blog(log_level, PLUGIN_LOG_HEADER format, ##__VA_ARGS__)
