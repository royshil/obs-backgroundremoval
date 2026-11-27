// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UpdateConfig.hpp"

#include <memory>
#include <mutex>
#include <optional>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#include <obs-module.h>
#include <util/base.h>
#include <util/bmem.h>
#include <util/config-file.h>
#include <util/platform.h>

#include <PluginGlobal/SemVer.hpp>
#include <plugin-support.h>

namespace PluginGlobal::UpdateConfig
{
namespace
{

using ConfigFile = std::unique_ptr<config_t, void (*)(config_t *)>;
using ModuleString = std::unique_ptr<char, void (*)(void *)>;

constexpr const char *kSection = "update";
constexpr const char *kVersion = "version";
constexpr const char *kCheckForUpdates = "check_for_updates";

// Serializes the reads and writes of update.ini within the process.
constinit std::mutex g_updateConfigMutex;

ModuleString modulePath(const char *file)
{
    return ModuleString(obs_module_config_path(file), &bfree);
}

// Opens path with CONFIG_OPEN_EXISTING, which never creates or empties a file. Returns nullptr when it does not open.
ConfigFile openExisting(const char *path)
{
    config_t *config = nullptr;
    if (config_open(&config, path, CONFIG_OPEN_EXISTING) != CONFIG_SUCCESS)
    {
        return ConfigFile(nullptr, &config_close);
    }
    return ConfigFile(config, &config_close);
}

// A file is valid when its section [update] has a version or a check_for_updates.
bool isValid(config_t *config)
{
    return config_has_user_value(config, kSection, kVersion) ||
           config_has_user_value(config, kSection, kCheckForUpdates);
}

// MARK: Platform

#ifdef _WIN32

using WideString = std::unique_ptr<wchar_t, void (*)(void *)>;

WideString widePath(const char *path)
{
    wchar_t *wide = nullptr;
    os_utf8_to_wcs_ptr(path, 0, &wide);
    return WideString(wide, &bfree);
}

bool flushFile(const char *path)
{
    const WideString wide = widePath(path);
    if (!wide)
    {
        return false;
    }
    const HANDLE file = CreateFileW(wide.get(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                    nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return false;
    }
    const bool flushed = FlushFileBuffers(file);
    return CloseHandle(file) && flushed;
}

// MOVEFILE_WRITE_THROUGH, which libobs's os_rename does not pass, is all that makes the rename durable on Windows.
bool replaceDurably(const char *from, const char *to, bool)
{
    const WideString wideFrom = widePath(from);
    const WideString wideTo = widePath(to);
    return wideFrom && wideTo &&
           MoveFileExW(wideFrom.get(), wideTo.get(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH);
}

#else

// On macOS, fsync does not ask the drive to write its cache, and F_FULLFSYNC does. There is no fallback to fsync.
bool syncDescriptor(int descriptor)
{
#ifdef __APPLE__
    return fcntl(descriptor, F_FULLFSYNC) != -1;
#else
    return fsync(descriptor) == 0;
#endif
}

bool flushPath(const char *path, int flags)
{
    const int descriptor = open(path, flags);
    if (descriptor == -1)
    {
        return false;
    }
    const bool synced = syncDescriptor(descriptor);
    return close(descriptor) == 0 && synced;
}

bool flushFile(const char *path)
{
    return flushPath(path, O_WRONLY);
}

// The directory that holds path, without the file name.
std::string parentOf(std::string_view path)
{
    while (path.size() > 1 && path.back() == '/')
    {
        path.remove_suffix(1);
    }
    const std::size_t slash = path.rfind('/');
    return std::string(slash == std::string_view::npos ? std::string_view(".")
                                                       : path.substr(0, slash == 0 ? 1 : slash));
}

// Renames from to to, and flushes the directory of to, and its parent when the directory has just been created.
bool replaceDurably(const char *from, const char *to, bool directoryCreated)
{
    if (os_rename(from, to) != 0)
    {
        return false;
    }
    const std::string directory = parentOf(to);
    if (!flushPath(directory.c_str(), O_RDONLY | O_DIRECTORY))
    {
        return false;
    }
    return !directoryCreated || flushPath(parentOf(directory).c_str(), O_RDONLY | O_DIRECTORY);
}

#endif

// MARK: Commit

// What a commit writes. A missing version leaves the version of the copy as it is.
struct Record
{
    std::optional<std::string> version;
    bool checkForUpdates;
};

bool holds(const char *path, const Record &record)
{
    const ConfigFile config = openExisting(path);
    if (!config)
    {
        return false;
    }
    if (record.version)
    {
        const char *version = config_get_string(config.get(), kSection, kVersion);
        if (!version || *record.version != version)
        {
            return false;
        }
    }
    return config_has_user_value(config.get(), kSection, kCheckForUpdates) &&
           config_get_bool(config.get(), kSection, kCheckForUpdates) == record.checkForUpdates;
}

// Replaces update.ini with a file that holds record, as SP-00011 requires. update.ini is
// unchanged until the rename, and the commit has succeeded only when every step has.
bool commit(const Record &record, bool firstInstall)
{
    const ModuleString updateConfigPath = modulePath("update.ini");
    const ModuleString temporaryPath = modulePath("update.ini.tmp");
    if (!updateConfigPath || !temporaryPath)
    {
        return false;
    }

    // 1. A leftover that cannot be removed makes a later step fail or is overwritten by it.
    os_unlink(temporaryPath.get());

    // 2. Make update.ini.tmp, from a copy of update.ini or, for a first install, from nothing.
    bool directoryCreated = false;
    ConfigFile config(nullptr, &config_close);
    if (firstInstall)
    {
        const ModuleString directoryPath = modulePath("");
        if (!directoryPath)
        {
            return false;
        }
        std::string directory(directoryPath.get());
        while (directory.size() > 1 && (directory.back() == '/' || directory.back() == '\\'))
        {
            directory.pop_back();
        }
        const int created = os_mkdirs(directory.c_str());
        if (created == MKDIR_ERROR)
        {
            return false;
        }
        directoryCreated = created == MKDIR_SUCCESS;
        config.reset(config_create(temporaryPath.get()));
    }
    else if (os_copyfile(updateConfigPath.get(), temporaryPath.get()) == 0)
    {
        config = openExisting(temporaryPath.get());
    }
    if (!config)
    {
        return false;
    }

    // 3. Set the values, keeping every other key of the copy.
    if (record.version)
    {
        config_set_string(config.get(), kSection, kVersion, record.version->c_str());
    }
    config_set_bool(config.get(), kSection, kCheckForUpdates, record.checkForUpdates);
    if (config_save(config.get()) != CONFIG_SUCCESS)
    {
        return false;
    }
    config.reset();

    // 4. config_save and os_copyfile do not check fclose, so read the file back while update.ini is unchanged.
    // 5. Flush it to the disk.
    // 6. and 7. Rename it over update.ini, which is the commit, and make the rename durable.
    // 8. Read update.ini back: the commit counts only once the operating system presents the new content.
    return holds(temporaryPath.get(), record) && flushFile(temporaryPath.get()) &&
           replaceDurably(temporaryPath.get(), updateConfigPath.get(), directoryCreated) &&
           holds(updateConfigPath.get(), record);
}

bool recordPluginVersionLocked(std::string_view version)
{
    SemVer current;
    if (!current.parse(version))
    {
        obs_log_warning("The plugin version is not a semantic version: %.*s", static_cast<int>(version.size()),
                        version.data());
        return false;
    }

    const ModuleString updateConfigPath = modulePath("update.ini");
    if (!updateConfigPath)
    {
        return false;
    }
    Record record{std::string(version), true};

    if (!os_file_exists(updateConfigPath.get()))
    {
        // The first startup after an install.
        return commit(record, true);
    }

    const ConfigFile config = openExisting(updateConfigPath.get());
    if (!config || !isValid(config.get()))
    {
        return false;
    }

    // A valid file without a version records 0.0.0.
    const char *recordedText = config_get_string(config.get(), kSection, kVersion);
    SemVer recorded;
    if (!recorded.parse(recordedText ? std::string_view(recordedText) : std::string_view("0.0.0")) ||
        !(recorded < current))
    {
        return false;
    }

    // The first startup after an update. check_for_updates comes from the decision, not from the copy.
    if (config_has_user_value(config.get(), kSection, kCheckForUpdates))
    {
        record.checkForUpdates = config_get_bool(config.get(), kSection, kCheckForUpdates);
    }
    return commit(record, false);
}

bool setCheckForUpdatesLocked(bool enabled)
{
    const ModuleString updateConfigPath = modulePath("update.ini");
    if (!updateConfigPath)
    {
        return false;
    }

    const Record record{std::nullopt, enabled};
    if (!os_file_exists(updateConfigPath.get()))
    {
        return commit(record, true);
    }

    // A file that does not open or is not valid may have been read short, so it is left as it is.
    const ConfigFile config = openExisting(updateConfigPath.get());
    return config && isValid(config.get()) && commit(record, false);
}

} // namespace

// When the file cannot be looked for, the startup dialog is treated as turned off, which is the side of silence.
bool startupDialogDisabled() noexcept
try
{
    const ModuleString path = modulePath("NO_STARTUP_DIALOG.txt");
    return !path || os_file_exists(path.get());
}
catch (...)
{
    return true;
}

bool recordPluginVersion(std::string_view version) noexcept
try
{
    const std::lock_guard lock(g_updateConfigMutex);
    return recordPluginVersionLocked(version);
}
catch (...)
{
    return false;
}

bool setCheckForUpdates(bool enabled) noexcept
try
{
    const std::lock_guard lock(g_updateConfigMutex);
    return setCheckForUpdatesLocked(enabled);
}
catch (...)
{
    return false;
}

bool checkForUpdatesEnabled() noexcept
try
{
    const std::lock_guard lock(g_updateConfigMutex);
    const ModuleString updateConfigPath = modulePath("update.ini");
    if (!updateConfigPath)
    {
        return false;
    }
    const ConfigFile config = openExisting(updateConfigPath.get());
    return config && config_has_user_value(config.get(), kSection, kCheckForUpdates) &&
           config_get_bool(config.get(), kSection, kCheckForUpdates);
}
catch (...)
{
    return false;
}

} // namespace PluginGlobal::UpdateConfig
