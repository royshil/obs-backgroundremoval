// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UpdateChecker.hpp"

#include <memory>
#include <mutex>
#include <string>

#include <obs-module.h>
#include <util/base.h>
#include <util/config-file.h>
#include <util/platform.h>

#include <plugin-support.h>

namespace UpdateChecker
{
namespace
{

using UpdateConfig = std::unique_ptr<config_t, void (*)(config_t *)>;

// Serializes the reads and writes of update.ini.
std::mutex updateConfigMutex;

// Opens update.ini in the config directory of the plugin, creating both when they are missing.
// Returns nullptr when that fails.
UpdateConfig openUpdateConfig()
{
    UpdateConfig failure(nullptr, &config_close);

    const std::unique_ptr<char, void (*)(void *)> configDirectory(obs_module_config_path(nullptr), &bfree);
    if (!configDirectory || configDirectory.get()[0] == '\0')
    {
        obs_log(LOG_ERROR, "Invalid config directory path (empty)");
        return failure;
    }

    const std::unique_ptr<char, void (*)(void *)> updateConfigPath(obs_module_config_path("update.ini"), &bfree);
    if (!updateConfigPath || updateConfigPath.get()[0] == '\0')
    {
        obs_log(LOG_ERROR, "Invalid update config path (empty)");
        return failure;
    }

    if (os_mkdirs(configDirectory.get()) == MKDIR_ERROR)
    {
        obs_log(LOG_WARNING, "Failed to create config directory");
        return failure;
    }

    config_t *config = nullptr;
    if (config_open(&config, updateConfigPath.get(), CONFIG_OPEN_ALWAYS) != CONFIG_SUCCESS)
    {
        obs_log(LOG_WARNING, "Failed to open update config");
        return failure;
    }

    config_set_default_bool(config, "update", "check_for_updates", true);
    return UpdateConfig(config, &config_close);
}

bool saveUpdateConfig(config_t *config)
{
    if (config_save_safe(config, "tmp", nullptr) != CONFIG_SUCCESS)
    {
        obs_log(LOG_WARNING, "Failed to save update config");
        return false;
    }
    return true;
}

} // namespace

bool recordPluginVersion(std::string_view version)
{
    if (version.empty())
    {
        obs_log(LOG_ERROR, "Invalid plugin version (empty)");
        return false;
    }

    const std::lock_guard lock(updateConfigMutex);
    const UpdateConfig config = openUpdateConfig();
    if (!config)
    {
        return false;
    }

    const char *recordedVersion = config_get_string(config.get(), "update", "version");
    if (recordedVersion && version == recordedVersion)
    {
        return false;
    }

    if (!config_has_user_value(config.get(), "update", "check_for_updates"))
    {
        config_set_bool(config.get(), "update", "check_for_updates", true);
    }
    config_set_string(config.get(), "update", "version", std::string(version).c_str());
    return saveUpdateConfig(config.get());
}

bool setCheckForUpdates(bool enabled)
{
    const std::lock_guard lock(updateConfigMutex);
    const UpdateConfig config = openUpdateConfig();
    if (!config)
    {
        return false;
    }

    config_set_bool(config.get(), "update", "check_for_updates", enabled);
    return saveUpdateConfig(config.get());
}

bool checkForUpdatesEnabled()
{
    const std::lock_guard lock(updateConfigMutex);
    const UpdateConfig config = openUpdateConfig();
    return config && config_get_bool(config.get(), "update", "check_for_updates");
}

} // namespace UpdateChecker
