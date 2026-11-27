// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PluginLoader.h"

#include <exception>
#include <mutex>
#include <new>
#include <optional>
#include <stddef.h>
#include <string_view>

#ifdef PLUGIN_ENABLE_UPDATE_CHECKER
#include <memory>
#include <stdexcept>
#include <thread>

#include <curl/curl.h>
#endif

#include <obs-frontend-api.h>
#include <obs-module.h>

#include <BackgroundRemoval/FilterContext.hpp>
#include <UI/AboutDialogIntegration.hpp>
#include <UpdateChecker/UpdateChecker.hpp>
#include <plugin-support.h>

namespace
{

constinit std::mutex g_remoteLatestVersionMutex;
constinit PluginLoaderSemver g_remoteLatestVersion{};

#ifdef PLUGIN_ENABLE_UPDATE_CHECKER

struct CurlHandle
{
    CURL *curl{nullptr};

    CurlHandle()
    {
        if (CURLcode res = curl_global_init(CURL_GLOBAL_DEFAULT); res != CURLE_OK)
        {
            obs_log_warning("Failed to initialize cURL globally: %s", curl_easy_strerror(res));
            throw std::runtime_error("Failed to initialize cURL globally");
        }

        curl = curl_easy_init();
        if (!curl)
        {
            curl_global_cleanup();
            obs_log_warning("Failed to initialize cURL handle");
            throw std::runtime_error("Failed to initialize cURL handle");
        }
    }

    CurlHandle(const CurlHandle &) = delete;
    CurlHandle &operator=(const CurlHandle &) = delete;

    ~CurlHandle()
    {
        if (curl)
        {
            curl_easy_cleanup(curl);
        }
        curl = nullptr;
        curl_global_cleanup();
    }
};

struct RemoteSemverBuffer
{
    char data[1024];
    size_t size{0};

    // A response that does not fit in data is an error.
    static size_t writeCallback(void *contents, size_t size, size_t nmemb, void *userp) noexcept
    {
        auto *const buf = static_cast<RemoteSemverBuffer *>(userp);

        if (buf == nullptr)
        {
            return CURL_WRITEFUNC_ERROR;
        }
        else if (size == 0 || nmemb == 0)
        {
            return 0;
        }

        const size_t available = sizeof(buf->data) - buf->size;
        if (nmemb > available / size)
        {
            return CURL_WRITEFUNC_ERROR;
        }

        const size_t nbytes = size * nmemb;
        for (size_t i = 0; i < nbytes; i += 1)
        {
            buf->data[buf->size + i] = static_cast<const char *>(contents)[i];
        }
        buf->size += nbytes;

        return nbytes;
    }

    // The received bytes without trailing whitespace.
    std::string_view view() const noexcept
    {
        std::string_view str(data, size);
        while (!str.empty() && (str.back() == ' ' || str.back() == '\t' || str.back() == '\r' || str.back() == '\n'))
        {
            str.remove_suffix(1);
        }
        return str;
    }
};

void fetchRemoteLatestVersion(CURL *curl) noexcept
{
    RemoteSemverBuffer buf;

    curl_easy_setopt(curl, CURLOPT_URL, PLUGIN_LATEST_VERSION_URL);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, PLUGIN_NAME "/" PLUGIN_VERSION);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, &RemoteSemverBuffer::writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    if (const CURLcode res = curl_easy_perform(curl); res != CURLE_OK)
    {
        obs_log_warning("Failed to get remote latest version: %s", curl_easy_strerror(res));
        return;
    }

    long statusCode = 0;
    if (const CURLcode res = curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode); res != CURLE_OK)
    {
        obs_log_warning("Failed to get HTTP status for remote latest version: %s", curl_easy_strerror(res));
        return;
    }
    else if (statusCode / 100 != 2)
    {
        obs_log_warning("Getting remote latest version returned HTTP status %ld", statusCode);
        return;
    }

    PluginLoaderSemver remoteLatestVersion{};
    if (!remoteLatestVersion.parse(buf.view()))
    {
        obs_log_warning("Failed to parse remote latest version");
        return;
    }

    try
    {
        std::lock_guard lock(g_remoteLatestVersionMutex);
        g_remoteLatestVersion = remoteLatestVersion;
    }
    catch (const std::exception &exception)
    {
        obs_log_warning("Failed to store remote latest version: %s", exception.what());
    }
}

struct StartupThread
{
    std::thread thread_;

    StartupThread()
        : thread_([curl = std::make_unique<CurlHandle>()]() noexcept { fetchRemoteLatestVersion(curl->curl); })
    {
    }

    StartupThread(const StartupThread &) = delete;
    StartupThread &operator=(const StartupThread &) = delete;

    ~StartupThread() noexcept
    {
        if (thread_.joinable())
        {
            thread_.join();
        }
    }
};

#else // PLUGIN_ENABLE_UPDATE_CHECKER

struct StartupThread
{
};

#endif // PLUGIN_ENABLE_UPDATE_CHECKER

constinit std::mutex g_startupThreadMutex;
constinit std::optional<StartupThread> g_startupThread;

// Waits for the fetch in progress, so that it does not outlive libobs.
void joinStartupThread() noexcept
{
    try
    {
        std::lock_guard lock(g_startupThreadMutex);
        g_startupThread.reset();
    }
    catch (const std::exception &exception)
    {
        obs_log_warning("Failed to join the startup thread: %s", exception.what());
    }
}

void showStartupDialogHandler(enum obs_frontend_event event, void *) noexcept;

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

// Runs once OBS has started and no modal dialog is open. The About dialog is shown on the first startup of
// each version, and the remote latest version is fetched only after the user has seen it, so that the user can
// turn update checks off before the first fetch.
void runStartupProcedure() noexcept
try
{
    if (!(UpdateChecker::recordPluginVersion(PLUGIN_VERSION) && AboutDialogIntegration::show()))
    {
        PluginLoaderFetchRemoteLatestVersionAsync();
    }
}
catch (const std::bad_alloc &exception)
{
    obs_crash("runStartupProcedure", "%s", exception.what());
}
catch (const std::exception &exception)
{
    obs_log_warning("Failed to run the startup procedure: %s", exception.what());
}
catch (...)
{
    obs_log_warning("Failed to run the startup procedure: Unknown error");
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
        if (setStartupCallbackRegistration(false) && !AboutDialogIntegration::runWhenNoModalDialog(runStartupProcedure))
        {
            runStartupProcedure();
        }
    }
    catch (const std::bad_alloc &exception)
    {
        obs_crash("showStartupDialogHandler", "%s", exception.what());
    }
    catch (const std::exception &exception)
    {
        obs_log_warning("Failed to schedule the startup procedure: %s", exception.what());
    }
    catch (...)
    {
        obs_log_warning("Failed to schedule the startup procedure: Unknown error");
    }
}

} // namespace

void PluginLoaderFetchRemoteLatestVersionAsync()
{
    if (!UpdateChecker::checkForUpdatesEnabled())
    {
        return;
    }

    std::lock_guard lock(g_startupThreadMutex);
    if (!g_startupThread)
    {
        g_startupThread.emplace();
    }
}

// The functions below are called from C, so no exception may leave them.

extern "C"
{

bool pluginloader_load(void) noexcept
{
    obs_register_source(&BackgroundRemoval::FilterContext::kSourceInfo);
    return true;
}

// The startup procedure waits for OBS_FRONTEND_EVENT_FINISHED_LOADING, which OBS sends after every module has
// been loaded, and then for any modal dialog of OBS to close.
void pluginloader_post_load(void) noexcept
try
{
    setStartupCallbackRegistration(true);
}
catch (const std::bad_alloc &exception)
{
    obs_crash("pluginloader_post_load", "%s", exception.what());
}
catch (const std::exception &exception)
{
    obs_log_warning("Exception occurred in pluginloader_post_load: %s", exception.what());
}
catch (...)
{
    obs_log_warning("Unknown exception occurred in pluginloader_post_load");
}

// libobs is still running here, so the frontend callback is removed and the fetch is joined before libobs
// shuts down, not when static objects are destroyed at exit.
void pluginloader_unload(void) noexcept
{
    try
    {
        setStartupCallbackRegistration(false);
    }
    catch (const std::exception &exception)
    {
        obs_log_warning("Failed to remove the startup callback: %s", exception.what());
    }
    catch (...)
    {
        obs_log_warning("Failed to remove the startup callback: Unknown error");
    }

    AboutDialogIntegration::cancelRunWhenNoModalDialog();
    joinStartupThread();
}

const char *pluginloader_get_name(void) noexcept
{
    return "Background Removal";
}

const char *pluginloader_get_description(void) noexcept
{
    return "A plugin for OBS Studio that allows you to replace the background in portrait images and video, as well as enhance low-light scenes";
}

const char *pluginloader_get_author(void) noexcept
{
    return "Roy Shilkrot";
}

struct pluginloader_semver pluginloader_get_remote_latest_version(void) noexcept
{
    try
    {
        std::lock_guard lock(g_remoteLatestVersionMutex);
        return g_remoteLatestVersion;
    }
    catch (const std::exception &exception)
    {
        obs_log_warning("Failed to read remote latest version: %s", exception.what());
        return {};
    }
}
}

bool PluginLoaderSemver::parse(std::string_view str) noexcept
{
    size_t i = 0;

    char phrase = '0';
    size_t ph_start = 0;
    size_t ph_pos = 0;

    size_t j = 0;

    pre_release[0] = '\0';
    build[0] = '\0';

    for (;;)
    {
        switch (phrase)
        {
        case '\0': /* acceptable end */
            if (i == str.size() || str.at(i) == '\0')
            {
                return true;
            }
            else
            {
                phrase = 'u'; /* unexpected char */
                continue;
            }

        case '0': /* <version core> */
            ph_start = i;

            if (i < str.size() && str.at(i) == '0')
            {
                version_core[ph_pos] = 0;
                i += 1;
            }
            else if (i < str.size() && str.at(i) >= '1' && str.at(i) <= '9')
            {
                version_core[ph_pos] = str.at(i) - '0';
                i += 1;
                while (i < str.size() && str.at(i) >= '0' && str.at(i) <= '9' && i - ph_start < 9)
                {
                    version_core[ph_pos] = version_core[ph_pos] * 10 + (str.at(i) - '0');
                    i += 1;
                }

                if (i < str.size() && str.at(i) >= '0' && str.at(i) <= '9')
                {
                    phrase = 'o'; /* too large number */
                    continue;
                }
            }
            else
            {
                phrase = 'u'; /* unexpected char */
                continue;
            }

            if (ph_pos < 2)
            {
                if (i < str.size() && str.at(i) == '.')
                {
                    i += 1;
                    phrase = '0'; /* <version core> */
                    ph_pos += 1;  /* next version core component */
                    continue;
                }
                else
                {
                    phrase = 'u'; /* unexpected char */
                    continue;
                }
            }

            if (i < str.size() && (str.at(i) == '-' || str.at(i) == '+'))
            {
                phrase = str.at(i); /* <pre-release> or <build> */
                i += 1;
                ph_start = i;
                ph_pos = i;
                continue;
            }
            else
            {
                phrase = '\0'; /* acceptable end */
                continue;
            }
        case '-': /* <pre-release> */
        case '+': /* <build> */
        {
            while (i < str.size() && str.at(i) >= '0' && str.at(i) <= '9')
            {
                i += 1;
            }

            j = i; /* end of the leading digits */

            while (i < str.size() && ((str.at(i) == '-') || (str.at(i) >= '0' && str.at(i) <= '9') ||
                                      (str.at(i) >= 'A' && str.at(i) <= 'Z') || (str.at(i) >= 'a' && str.at(i) <= 'z')))
            {
                i += 1;
            }

            if (i == ph_pos)
            {
                phrase = 'u'; /* empty identifier */
                continue;
            }
            else if (phrase == '-' && j == i && i - ph_pos > 1 && str.at(ph_pos) == '0')
            {
                phrase = 'z'; /* leading zero in numeric identifier */
                continue;
            }
            else if (i < str.size() && str.at(i) == '.')
            {
                i += 1;
                ph_pos = i; /* next identifier */
                continue;
            }
            else
            {
                phrase |= 0x40; /* <pre-release> copy: m, <build> copy: k */
                continue;
            }
        }
        case 'm': /* <pre-release> copy: data[ph_start] to data[i - 1] */
            if (i - ph_start >= sizeof(pre_release))
            {
                phrase = 'f'; /* buffer full */
                continue;
            }

            for (j = ph_start; j < i; j++)
            {
                pre_release[j - ph_start] = str.at(j);
            }
            pre_release[i - ph_start] = '\0';

            if (i < str.size() && str.at(i) == '+')
            {
                phrase = '+'; /* <build> */
                i += 1;
                ph_start = i;
                ph_pos = i;
                continue;
            }
            else
            {
                phrase = '\0'; /* acceptable end */
                continue;
            }
        case 'k': /* <build> copy: data[ph_start] to data[i - 1] */
            if (i - ph_start >= sizeof(build))
            {
                phrase = 'f'; /* buffer full */
                continue;
            }

            for (j = ph_start; j < i; j++)
            {
                build[j - ph_start] = str.at(j);
            }
            build[i - ph_start] = '\0';

            phrase = '\0'; /* acceptable end */
            continue;
        default: /* rejected */
            version_core[0] = 0;
            version_core[1] = 0;
            version_core[2] = 0;

            pre_release[0] = '\0';
            build[0] = '\0';

            break;
        }

        break;
    }

    return false;
}
