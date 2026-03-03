# SPDX-FileCopyrightText: 2018-2026 OBS Project and its contributors
# SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
# SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

# CMake operating system bootstrap module

include_guard(GLOBAL)

if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Windows")
  set(CMAKE_C_EXTENSIONS FALSE)
  set(CMAKE_CXX_EXTENSIONS FALSE)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows")
  set(OS_WINDOWS TRUE)
elseif(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin")
  set(CMAKE_C_EXTENSIONS FALSE)
  set(CMAKE_CXX_EXTENSIONS FALSE)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos")
  set(OS_MACOS TRUE)
elseif(CMAKE_HOST_SYSTEM_NAME MATCHES "Linux|FreeBSD|OpenBSD")
  set(CMAKE_CXX_EXTENSIONS FALSE)
  list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux")
  string(TOUPPER "${CMAKE_HOST_SYSTEM_NAME}" _SYSTEM_NAME_U)
  set(OS_${_SYSTEM_NAME_U} TRUE)
endif()
