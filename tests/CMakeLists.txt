# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

if(WIN32)
  set(
    OBS_STUDIO_BIN_PATH
    "C:/Program Files/obs-studio/bin/64bit"
    CACHE PATH
    "Directory containing obs.dll for running tests [Windows]"
  )
endif()

add_subdirectory(UI)
add_subdirectory(UpdateConfig)
