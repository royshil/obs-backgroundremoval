#ifdef _WIN32
#include <windows.h>
#include <delayimp.h>
#include <filesystem>
#include <obs-module.h>
#include "plugin-support.h"
#endif

std::filesystem::path binaryPath;

extern "C" void PluginLoaded()
{
    binaryPath = obs_get_module_binary_path(obs_current_module());
    obs_log(LOG_INFO, "Plugin binary path: %s", binaryPath.string().c_str());
}

extern "C" void PluginUnloaded()
{
    // Do nothing
}

#ifdef _WIN32
extern "C" {
FARPROC WINAPI DelayLoadHook(
    unsigned dliNotify,
    PDelayLoadInfo pdli
) {
    if (dliNotify == dliNotePreLoadLibrary) {
        obs_log(LOG_INFO, "Delay loading DLL: %s", pdli->szDll);
	    std::filesystem::path absPath = std::filesystem::absolute(binaryPath / ".." / PLUGIN_NAME / pdli->szDll);
        obs_log(LOG_INFO, "Resolved DLL path: %s", absPath.string().c_str());
        return (FARPROC)LoadLibraryExW(absPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    }
    return NULL;
}

const PfnDliHook __pfnDliNotifyHook2 = DelayLoadHook;
}
#endif