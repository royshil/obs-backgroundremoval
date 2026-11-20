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

add_subdirectory(UI)
add_subdirectory(UpdateConfig)
