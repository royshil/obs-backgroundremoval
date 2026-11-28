// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

bool pluginloader_load(void)
#ifdef __cplusplus
    noexcept
#endif
    ;

void pluginloader_post_load(void)
#ifdef __cplusplus
    noexcept
#endif
    ;
void pluginloader_unload(void)
#ifdef __cplusplus
    noexcept
#endif
    ;

const char *pluginloader_get_name(void)
#ifdef __cplusplus
    noexcept
#endif
    ;

const char *pluginloader_get_description(void)
#ifdef __cplusplus
    noexcept
#endif
    ;

const char *pluginloader_get_author(void)
#ifdef __cplusplus
    noexcept
#endif
    ;

struct pluginloader_semver
{
    long version_core[3];
    char pre_release[100];
    char build[100];
};

struct pluginloader_semver pluginloader_get_remote_latest_version(void)
#ifdef __cplusplus
    noexcept
#endif
    ;

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus

#include <string_view>

// Starts fetching the remote latest version once per session, unless the user has turned update checks off.
void PluginLoaderFetchRemoteLatestVersionAsync();

struct PluginLoaderSemver : public pluginloader_semver
{
    bool parse(std::string_view str) noexcept;
};

#endif
