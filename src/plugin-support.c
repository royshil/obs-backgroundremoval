/*
 * SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
 * SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "plugin-support.h"

#ifndef PLUGIN_NAME_STR
#error PLUGIN_NAME_STR must be defined by the build system
#endif

#ifndef PLUGIN_VERSION_STR
#error PLUGIN_VERSION_STR must be defined by the build system
#endif

const char *PLUGIN_NAME = PLUGIN_NAME_STR;
const char *PLUGIN_VERSION = PLUGIN_VERSION_STR;
