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

#ifndef PLUGIN_LATEST_VERSION_URL
#define PLUGIN_LATEST_VERSION_URL "https://royshil.github.io/obs-backgroundremoval/metadata/latest-version.txt"
#endif

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef _MSC_VER
__declspec(dllexport)
#else
__attribute__((__format__(__printf__, 2, 3)))
#endif
void blog(int log_level, const char *format, ...);

#ifdef _MSC_VER
__declspec(noreturn) __declspec(dllexport)
#else
__attribute__((__format__(__printf__, 1, 2), __noreturn__))
#endif
void bcrash(const char *format, ...);

#ifdef __cplusplus
}
#endif

/**
 * Logs a fatal error and crashes the application immediately with crash handling.
 * @param OOPS_NAME A string literal identifying the location of the crash.
 */
#define obs_crash(OOPS_NAME, format, ...)                                                                              \
    (blog(100, "[" PLUGIN_NAME "] Oops! " OOPS_NAME ": " format, ##__VA_ARGS__),                                       \
     bcrash("[" PLUGIN_NAME "] Oops! " OOPS_NAME))

/**
 * Logs a severe error. You SHOULD crash the filter immediately but you MUST NOT crash the entire application.
 */
#define obs_log_error(format, ...) blog(100, "[" PLUGIN_NAME "] " format, ##__VA_ARGS__)

/**
 * Logs a non-fatal error. You MUST NOT crash the filter but it MAY fail to work.
 */
#define obs_log_warning(format, ...) blog(200, "[" PLUGIN_NAME "] " format, ##__VA_ARGS__)

/**
 * Logs a structured log like `field1=value1 field2=value2`.
 */
#define obs_log_info(format, ...) blog(300, "[" PLUGIN_NAME "] " format, ##__VA_ARGS__)

/**
 * Logs a message for debugging purposes.
 */
#define obs_log_debug(format, ...) blog(400, "[" PLUGIN_NAME "] " format, ##__VA_ARGS__)

/* Compatibility log macro */
#define obs_log(log_level, format, ...) blog(log_level, "[" PLUGIN_NAME "] " format, ##__VA_ARGS__)
