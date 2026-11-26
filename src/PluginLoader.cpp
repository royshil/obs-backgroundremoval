// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <array>
#include <cstddef>
#include <exception>
#include <mutex>
#include <string_view>

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <BackgroundRemoval/FilterContext.hpp>
#include <UI/AboutDialogIntegration.hpp>
#include <UpdateConfig/UpdateConfig.hpp>
#include <plugin-support.h>

extern "C" const char g_plugin_author[] = PLUGIN_AUTHOR;
extern "C" const char g_plugin_display_name[] = PLUGIN_DISPLAY_NAME;
extern "C" const char g_plugin_long_description[] = PLUGIN_LONG_DESCRIPTION;

namespace
{

// obs_register_source() copies each obs_source_info, so getSourceInfos() can return temporaries.
auto getSourceInfos() -> std::array<obs_source_info, 1>
{
    return {BackgroundRemoval::FilterContext::getSourceInfo()};
}

void showStartupDialogHandler(enum obs_frontend_event event, void *) noexcept;

void fetchLatestVersionIfEnabled()
{
    UpdateConfig::Request request = UpdateConfig::GetCheckForUpdatesEnabledRequest{};
    if (UpdateConfig::doRequest(request))
    {
        UpdateConfig::fetchLatestVersionAsync();
    }
}

bool setStartupCallbackRegistration(bool registered)
{
    static std::mutex mutex;
    static bool isRegistered = false;
    const std::lock_guard lock(mutex);

    if (isRegistered == registered)
    {
        return false;
    }

    if (registered)
    {
        obs_frontend_add_event_callback(showStartupDialogHandler, nullptr);
    }
    else
    {
        obs_frontend_remove_event_callback(showStartupDialogHandler, nullptr);
    }
    isRegistered = registered;
    return true;
}

// The OBS frontend calls this, so no exception may leave it.
void showStartupDialogHandler(enum obs_frontend_event event, void *) noexcept
{
    if (event != OBS_FRONTEND_EVENT_FINISHED_LOADING)
    {
        return;
    }

    try
    {
        if (!setStartupCallbackRegistration(false))
        {
            return;
        }

        if (!AboutDialogIntegration::show())
        {
            fetchLatestVersionIfEnabled();
        }
    }
    catch (const std::exception &exception)
    {
        obs_log(LOG_ERROR, "Failed to show the About dialog at startup: %s", exception.what());
    }
    catch (...)
    {
        obs_log(LOG_ERROR, "Failed to show the About dialog at startup: Unknown error");
    }
}

} // namespace

// The functions below are called from C, so no exception may leave them.

extern "C" bool pluginloader_load(void) noexcept
{
    const auto sourceInfos = getSourceInfos();

    try
    {
        const char *sourceId = nullptr;
        for (std::size_t index = 0; obs_enum_source_types(index, &sourceId); ++index)
        {
            for (const obs_source_info &sourceInfo : sourceInfos)
            {
                if (sourceId && std::string_view(sourceInfo.id) == sourceId)
                {
                    obs_log(LOG_ERROR, "Source ID '%s' is already registered by another plugin", sourceId);
                    return false;
                }
            }
        }

        UpdateConfig::latestVersionClient();
    }
    catch (const std::exception &exception)
    {
        obs_log(LOG_ERROR, "Failed to load the plugin: %s", exception.what());
        return false;
    }
    catch (...)
    {
        obs_log(LOG_ERROR, "Failed to load the plugin: Unknown error");
        return false;
    }

    for (const obs_source_info &sourceInfo : sourceInfos)
    {
        obs_register_source(&sourceInfo);
    }

    // The filter is registered by now, so a failure to set up the update check is logged and the load
    // still succeeds.
    try
    {
        UpdateConfig::Request request = UpdateConfig::HandleFirstRunRequest{PLUGIN_VERSION};
        if (!(UpdateConfig::doRequest(request) && setStartupCallbackRegistration(true)))
        {
            fetchLatestVersionIfEnabled();
        }
    }
    catch (const std::exception &exception)
    {
        obs_log(LOG_WARNING, "Failed to set up the update check: %s", exception.what());
    }
    catch (...)
    {
        obs_log(LOG_WARNING, "Failed to set up the update check: Unknown error");
    }

    return true;
}

extern "C" void pluginloader_post_load(void) noexcept
{
}

// libobs objects are still valid here, so the frontend callback is removed while libobs runs,
// not when static objects are destroyed at exit.
extern "C" void pluginloader_unload(void) noexcept
{
    try
    {
        setStartupCallbackRegistration(false);
    }
    catch (const std::exception &exception)
    {
        obs_log(LOG_WARNING, "Failed to remove the startup callback: %s", exception.what());
    }
    catch (...)
    {
        obs_log(LOG_WARNING, "Failed to remove the startup callback: Unknown error");
    }
}
