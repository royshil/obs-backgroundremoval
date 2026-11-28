/*
 * SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef PLUGIN_NAME
#define PLUGIN_NAME "obs-backgroundremoval"
#endif

#ifndef PLUGIN_VERSION
#define PLUGIN_VERSION "0.0.0"
#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#ifndef PLUGIN_PRIVATE
#ifdef _MSC_VER
#define PLUGIN_PRIVATE
#else
#define PLUGIN_PRIVATE __attribute__((visibility("hidden")))
#endif
#endif

#ifndef PLUGIN_PUBLIC
#ifdef _MSC_VER
#define PLUGIN_PUBLIC __declspec(dllexport)
#else
#define PLUGIN_PUBLIC __attribute__((visibility("default")))
#endif
#endif

typedef struct obs_module obs_module_t;
typedef struct text_lookup lookup_t;

extern void blog(int log_level, const char *format, ...);
extern void text_lookup_destroy(lookup_t *lookup);
extern bool text_lookup_getstr(lookup_t *lookup, const char *lookup_val, const char **out);
extern lookup_t *obs_module_load_locale(obs_module_t *module, const char *default_locale, const char *locale);

#define obs_log_info(log_level, format, ...) blog(300, "[" PLUGIN_NAME "] " format, ##__VA_ARGS__)

// MARK: - Plugin

bool pluginloader_load(void);
void pluginloader_post_load(void);
void pluginloader_unload(void);
const char *pluginloader_get_name(void);
const char *pluginloader_get_description(void);
const char *pluginloader_get_author(void);

// MARK: - Module globals

static obs_module_t *g_obs_module_pointer = NULL;
static lookup_t *g_obs_module_lookup = NULL;

// MARK: - Module

PLUGIN_PUBLIC void obs_module_set_pointer(obs_module_t *module)
{
    g_obs_module_pointer = module;
}

PLUGIN_PRIVATE obs_module_t *obs_current_module(void)
{
    return g_obs_module_pointer;
}

PLUGIN_PUBLIC uint32_t obs_module_ver(void)
{
    return (30u << 24) | (2u << 16) | 3u; /* 30.2.3 */
}

// MARK: - Lifecycle

PLUGIN_PUBLIC bool obs_module_load(void)
{
    if (pluginloader_load())
    {
        obs_log_info(LOG_INFO, "plugin loaded successfully (version " PLUGIN_VERSION ")");
        return true;
    }
    else
    {
        return false;
    }
}

PLUGIN_PUBLIC void obs_module_post_load(void)
{
    pluginloader_post_load();
}

PLUGIN_PUBLIC void obs_module_unload(void)
{
    pluginloader_unload();
}

// MARK: - Module information

PLUGIN_PUBLIC const char *obs_module_name(void)
{
    return pluginloader_get_name();
}

PLUGIN_PUBLIC const char *obs_module_description(void)
{
    return pluginloader_get_description();
}

PLUGIN_PUBLIC const char *obs_module_author(void)
{
    return pluginloader_get_author();
}

// MARK: Locale

PLUGIN_PUBLIC void obs_module_set_locale(const char *locale)
{
    if (g_obs_module_lookup)
    {
        text_lookup_destroy(g_obs_module_lookup);
    }

    g_obs_module_lookup = obs_module_load_locale(g_obs_module_pointer, "en-US", locale);
}

PLUGIN_PUBLIC void obs_module_free_locale(void)
{
    text_lookup_destroy(g_obs_module_lookup);
    g_obs_module_lookup = NULL;
}

PLUGIN_PUBLIC bool obs_module_get_string(const char *lookup_string, const char **translated_string)
{
    return text_lookup_getstr(g_obs_module_lookup, lookup_string, translated_string);
}

PLUGIN_PRIVATE const char *obs_module_text(const char *val)
{
    const char *out = val;
    text_lookup_getstr(g_obs_module_lookup, val, &out);
    return out;
}
