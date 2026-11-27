# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# The OBS Studio release that the vendored headers and import libraries are taken from. Tests that run
# libobs load obs.dll and its dependencies from OBS_TEST_BIN_DIR.
if(WIN32)
    FetchContent_Declare(
        obs-studio-windows
        URL https://github.com/obsproject/obs-studio/releases/download/30.2.3/OBS-Studio-30.2.3-Windows.zip
        URL_HASH SHA256=c5bf721258f6d8b2342ed39f04e65cdd3a07a9d1300654471d4be91d87fee65b
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(obs-studio-windows)
    set(OBS_TEST_ROOT "${obs-studio-windows_SOURCE_DIR}")
    set(OBS_TEST_BIN_DIR "${OBS_TEST_ROOT}/bin/64bit")
endif()

# On macOS the plugin only links libobs as a stub library, so the tests load libobs, its OpenGL
# backend and the OBS Studio modules from an installed OBS Studio. The tests are linked with
# OBS_MACOS_FRAMEWORKS_DIR as a run path, so that the install names of libobs, obs-frontend-api and
# Qt resolve without DYLD_FRAMEWORK_PATH.
if(APPLE)
    foreach(APP_DIR IN ITEMS "/Applications/OBS.app" "$ENV{HOME}/Applications/OBS.app")
        if(EXISTS "${APP_DIR}/Contents/Frameworks/libobs.framework")
            set(OBS_MACOS_FRAMEWORKS_DIR "${APP_DIR}/Contents/Frameworks")
            set(OBS_MACOS_PLUGINS_DIR "${APP_DIR}/Contents/PlugIns")
            break()
        endif()
    endforeach()
    if(NOT OBS_MACOS_FRAMEWORKS_DIR)
        message(WARNING "OBS Studio was not found in /Applications or ~/Applications, so the tests that run libobs are not built.")
    endif()
endif()

# Unit tests compile the plugin sources they test themselves, so that they need neither libobs nor
# Qt, but with the compile and link options of the plugin.
function(target_use_plugin_options TARGET_NAME)
    target_link_libraries("${TARGET_NAME}" PRIVATE $<COMPILE_ONLY:obs-backgroundremoval_pluginlibs>)
    target_link_options("${TARGET_NAME}" PRIVATE $<TARGET_PROPERTY:obs-backgroundremoval_pluginlibs,INTERFACE_LINK_OPTIONS>)
endfunction()

add_subdirectory(BackgroundRemoval)
add_subdirectory(UI)
add_subdirectory(UpdateChecker)
