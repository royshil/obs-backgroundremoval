# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28)

# Read buildspec.json for project metadata
block(
  PROPAGATE PLUGIN_NAME PLUGIN_VERSION PLUGIN_AUTHOR PLUGIN_EMAIL PREBUILT_VERSION QT6_VERSION MACOS_BUNDLEID
)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/buildspec.json" BUILDSPEC_CONTENT)
  string(JSON PLUGIN_NAME GET "${BUILDSPEC_CONTENT}" name)
  string(JSON PLUGIN_VERSION GET "${BUILDSPEC_CONTENT}" version)
  string(JSON PLUGIN_AUTHOR GET "${BUILDSPEC_CONTENT}" author)
  string(JSON PLUGIN_EMAIL GET "${BUILDSPEC_CONTENT}" email)
  string(JSON PREBUILT_VERSION GET "${BUILDSPEC_CONTENT}" dependencies prebuilt version)
  string(JSON QT6_VERSION GET "${BUILDSPEC_CONTENT}" dependencies qt6 version)
  string(JSON MACOS_BUNDLEID GET "${BUILDSPEC_CONTENT}" platformConfig macos bundleId)
endblock()

if(DEFINED ENV{GITHUB_RUN_ID})
  set(PLUGIN_BUILD_NUMBER "$ENV{GITHUB_RUN_ID}")
else()
  set(PLUGIN_BUILD_NUMBER "1")
endif()

project("${PLUGIN_NAME}" VERSION "${PLUGIN_VERSION}" LANGUAGES C CXX)

set(PLUGIN_VERSION_MAJOR "${CMAKE_PROJECT_VERSION_MAJOR}")

# Build options
set(VCPKG_TARGET_TRIPLET "" CACHE STRING "Vcpkg target triplet to use")

# Basic configuration
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS FALSE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

if(MSVC)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(APPLE)
  add_compile_options(-fno-omit-frame-pointer -fstack-protector-strong)
  add_link_options("-Wl,-no_warn_duplicate_libraries")
else()
  add_compile_options(-fno-omit-frame-pointer -fstack-protector-strong)
endif()

# CPack configuration
set(CPACK_ARCHIVE_THREADS 0)
set(CPACK_DEBIAN_COMPRESSION_LEVEL 19)
set(CPACK_DEBIAN_COMPRESSION_TYPE zstd)
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE TRUE)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS TRUE)
set(CPACK_GENERATOR "DEB")
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/release")
set(CPACK_PACKAGE_CONTACT "${PLUGIN_AUTHOR} <${PLUGIN_EMAIL}>")
set(CPACK_PACKAGE_FILE_NAME "${CMAKE_PROJECT_NAME}-${CMAKE_PROJECT_VERSION}-${CMAKE_C_LIBRARY_ARCHITECTURE}")
set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSES/GPL-3.0-or-later.txt")
set(CPACK_SET_DESTDIR TRUE)
set(CPACK_VERBATIM_VARIABLES TRUE)
set(
  CPACK_SOURCE_IGNORE_FILES
  ".deps_vendor/"
  ".git/"
  "build_x86_64/"
  "build_x64/"
  "build_macos/"
  "release/"
  "vcpkg_installed/"
)

# Include CPack after setting variables
include(CPack)

# Include paths
list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/common")
if(MSVC)
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/windows")
  list(
    PREPEND
    CMAKE_PREFIX_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-deps-qt6-${QT6_VERSION}-x64"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-deps-${PREBUILT_VERSION}-x64"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/ort_x64/Release/vcpkg_installed/x64-windows-static-md"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/ort_x64-prefix"
  )
elseif(APPLE)
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/macos")
  list(
    PREPEND
    CMAKE_PREFIX_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/Frameworks"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-deps-qt6-${QT6_VERSION}-universal"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps/obs-deps-${PREBUILT_VERSION}-universal"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/ort_universal_vcpkg_installed/universal-osx"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/ort_universal-prefix"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/vcpkg_installed_universal/universal-osx-obs"
  )
else()
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux")
  list(
    PREPEND
    CMAKE_PREFIX_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/ort_x86_64/Release/vcpkg_installed/x64-linux"
    "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/ort_x86_64-prefix"
  )
endif()

if(VCPKG_TARGET_TRIPLET STREQUAL "")
  message(STATUS "Build without vcpkg")
else()
  message(STATUS "Build with vcpkg with target triplet: ${VCPKG_TARGET_TRIPLET}")
  list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")
endif()

# Common initialization
include(bootstrap)
include(compilerconfig)
include(defaults)
include(helpers)

# Dependent packages

## PkgConfig
find_package(PkgConfig QUIET)

## OBS and Qt
find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)

## CURL
find_package(CURL CONFIG QUIET)

if(NOT CURL_FOUND AND PkgConfig_FOUND)
  pkg_check_modules(PC_CURL libcurl IMPORTED_TARGET)
  if(PC_CURL_FOUND)
    add_library(CURL::libcurl ALIAS PkgConfig::PC_CURL)
    set(CURL_FOUND TRUE)
  endif()
endif()

if(NOT CURL_FOUND)
  message(FATAL_ERROR "CURL not found via CMake or pkg-config.")
endif()

## OpenCV
find_package(OpenCV CONFIG QUIET)

if(NOT OpenCV_FOUND AND PkgConfig_FOUND)
  pkg_check_modules(PC_OPENCV opencv4 IMPORTED_TARGET)
  if(PC_OPENCV_FOUND)
    add_library(OpenCV::opencv_core ALIAS PkgConfig::PC_OPENCV)
    add_library(OpenCV::opencv_imgproc ALIAS PkgConfig::PC_OPENCV)
    set(OpenCV_FOUND TRUE)
  endif()
endif()

if(NOT OpenCV_FOUND)
  message(FATAL_ERROR "OpenCV not found via CMake or pkg-config.")
endif()

if(NOT TARGET OpenCV::opencv_core)
  add_library(OpenCV::opencv_core ALIAS opencv_core)
endif()
if(NOT TARGET OpenCV::opencv_imgproc)
  add_library(OpenCV::opencv_imgproc ALIAS opencv_imgproc)
endif()

## ONNX Runtime
find_package(onnxruntime CONFIG)

if(NOT onnxruntime_FOUND AND PkgConfig_FOUND)
  pkg_check_modules(PC_onnxruntime onnxruntime IMPORTED_TARGET)
  if(PC_onnxruntime_FOUND)
    add_library(onnxruntime::onnxruntime ALIAS PkgConfig::PC_onnxruntime)
    set(onnxruntime_FOUND TRUE)
  endif()
endif()

if(NOT onnxruntime_FOUND)
  message(FATAL_ERROR "ONNX Runtime not found via CMake or pkg-config.")
endif()

if(NOT TARGET onnxruntime::onnxruntime)
  add_library(onnxruntime::onnxruntime ALIAS onnxruntime)
endif()

# Plugin build definitions
add_library(${CMAKE_PROJECT_NAME} MODULE)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE onnxruntime::onnxruntime)
add_subdirectory(src/update-checker/CurlClient)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CurlClient OpenCV::opencv_core OpenCV::opencv_imgproc)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.c
    src/ort-utils/ort-session-utils.cpp
    src/obs-utils/obs-utils.cpp
    src/obs-utils/obs-config-utils.cpp
    src/update-checker/github-utils.cpp
    src/update-checker/update-checker.cpp
    src/background-filter-info.c
    src/background-filter.cpp
    src/enhance-filter.cpp
    src/enhance-filter-info.c
)
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-error=unused-command-line-argument>
)

if(APPLE)
  target_link_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/src/exported_symbols_macos.txt"
  )
elseif(MSVC)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/exported_symbols_windows.def")
else()
  target_link_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/src/exported_symbols_linux.map" "-Wl,-Bsymbolic"
  )
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${PLUGIN_NAME})
