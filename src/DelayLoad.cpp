#include <windows.h>
#include <delayimp.h>

#include <filesystem>
#include <string>

#include <obs-module.h>
#include "plugin-support.h"

extern "C" {

FARPROC WINAPI DelayLoadHook(unsigned dliNotify, PDelayLoadInfo pdli)
{
	if (dliNotify == dliNotePreLoadLibrary) {
		const std::string dllName(pdli->szDll);
		if (dllName == "onnxruntime.dll") {
			const std::filesystem::path binaryDir("../../obs-plugins/64bit");
			const std::filesystem::path absPath =
				std::filesystem::absolute(binaryDir / PLUGIN_NAME / dllName);
			obs_log(LOG_INFO, "Loading %S from %S", dllName.c_str(), absPath.c_str());
			return (FARPROC)LoadLibraryExW(absPath.c_str(), NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
		} else {
			return NULL;
		}
	}
	return NULL;
}

const PfnDliHook __pfnDliNotifyHook2 = DelayLoadHook;
}
