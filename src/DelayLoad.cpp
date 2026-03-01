/*
 * SPDX-FileCopyrightText: Copyright (C) 2025 Kaito Udagawa umireon@kaito.tokyo
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * OBS Plugin: Portrait Background Removal / Virtual Green-screen and Low-Light Enhancement
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

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
