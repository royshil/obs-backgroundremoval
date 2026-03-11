# SPDX-FileCopyrightText: 2018-2026 OBS Project and its contributors
# SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
# SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

# CMake Linux defaults module

include_guard(GLOBAL)

# Set default installation directories
include(GNUInstallDirs)

if(CMAKE_INSTALL_LIBDIR MATCHES "(CMAKE_SYSTEM_PROCESSOR)")
  string(REPLACE "CMAKE_SYSTEM_PROCESSOR" "${CMAKE_SYSTEM_PROCESSOR}" CMAKE_INSTALL_LIBDIR "${CMAKE_INSTALL_LIBDIR}")
endif()

# Enable find_package targets to become globally available targets
set(CMAKE_FIND_PACKAGE_TARGETS_GLOBAL TRUE)

set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-${CMAKE_C_LIBRARY_ARCHITECTURE}")

set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${PLUGIN_EMAIL}")
set(CPACK_SET_DESTDIR ON)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.25.0 OR NOT CMAKE_CROSSCOMPILING)
  set(CPACK_DEBIAN_DEBUGINFO_PACKAGE ON)
endif()

set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/release")

set(CPACK_SOURCE_GENERATOR "TXZ")
set(
  CPACK_SOURCE_IGNORE_FILES
  ".*~$"
  \\.git/
  \\.github/
  \\.gitignore
  \\.ccache/
  build_.*
  cmake/\\.CMakeBuildNumber
  release/
)

set(CPACK_VERBATIM_VARIABLES YES)
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${CPACK_PACKAGE_VERSION}-source")
set(CPACK_ARCHIVE_THREADS 0)
