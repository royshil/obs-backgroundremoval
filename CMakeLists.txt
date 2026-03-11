# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28)

# Read buildspec.json for project metadata
block(
  PROPAGATE PLUGIN_NAME PLUGIN_VERSION PREBUILT_VERSION QT6_VERSION MACOS_BUNDLEID
)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/buildspec.json" FILE_CONTENT)
  string(JSON PLUGIN_NAME GET "${FILE_CONTENT}" name)
  string(JSON PLUGIN_VERSION GET "${FILE_CONTENT}" version)
  string(JSON PREBUILT_VERSION GET "${FILE_CONTENT}" dependencies prebuilt version)
  string(JSON QT6_VERSION GET "${FILE_CONTENT}" dependencies qt6 version)
  string(JSON MACOS_BUNDLEID GET "${FILE_CONTENT}" platformConfig macos bundleId)
endblock()

if(DEFINED ENV{GITHUB_RUN_ID})
  set(PLUGIN_BUILD_NUMBER "$ENV{GITHUB_RUN_ID}")
else()
  set(PLUGIN_BUILD_NUMBER "1")
endif()

project("${PLUGIN_NAME}" VERSION "${PLUGIN_VERSION}" LANGUAGES C CXX)

# Build options
set(VCPKG_TARGET_TRIPLET "" CACHE STRING "Vcpkg target triplet to use")

# Basic configuration
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS FALSE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)

set(CMAKE_COMPILE_WARNING_AS_ERROR TRUE)

if(MSVC)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(APPLE)
  add_link_options("-Wl,-no_warn_duplicate_libraries")
endif()

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
  )
else()
  list(PREPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake/linux")
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

## OBS and Qt
find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Widgets Core)

## CURL
find_package(CURL CONFIG QUIET)
if(NOT CURL_FOUND)
  find_package(CURL QUIET)
endif()

if(NOT CURL_FOUND AND PkgConfig_FOUND)
  pkg_check_modules(PC_CURL libcurl)
  if(PC_CURL_FOUND)
    add_library(CURL::libcurl INTERFACE IMPORTED)
    target_link_libraries(CURL::libcurl INTERFACE ${PC_CURL_LIBRARIES})
    target_include_directories(CURL::libcurl INTERFACE ${PC_CURL_INCLUDE_DIRS})
    target_compile_definitions(CURL::libcurl INTERFACE ${PC_CURL_CFLAGS_OTHER})
    target_link_directories(CURL::libcurl INTERFACE ${PC_CURL_LIBRARY_DIRS})
    set(CURL_FOUND TRUE)
  endif()
endif()

if(NOT CURL_FOUND)
  message(FATAL_ERROR "CURL not found via CMake or pkg-config.")
endif()

## OpenCV
find_package(OpenCV CONFIG QUIET)
if(NOT OpenCV_FOUND)
  find_package(OpenCV QUIET)
endif()

if(NOT OpenCV_FOUND AND PkgConfig_FOUND)
  pkg_check_modules(PC_OPENCV opencv4)
  if(PC_OPENCV_FOUND)
    add_library(opencv_core INTERFACE IMPORTED)
    target_link_libraries(opencv_core INTERFACE opencv_core)
    target_include_directories(opencv_core INTERFACE ${PC_OPENCV_INCLUDE_DIRS})
    target_compile_definitions(opencv_core INTERFACE ${PC_OPENCV_CFLAGS_OTHER})
    target_link_directories(opencv_core INTERFACE ${PC_OPENCV_LIBRARY_DIRS})
    add_library(opencv_imgproc INTERFACE IMPORTED)
    target_link_libraries(opencv_imgproc INTERFACE opencv_imgproc)
    target_link_directories(opencv_imgproc INTERFACE ${PC_OPENCV_LIBRARY_DIRS})
    set(OpenCV_FOUND TRUE)

    add_library(OpenCV::opencv_core ALIAS opencv_core)
    add_library(OpenCV::opencv_imgproc ALIAS opencv_imgproc)
  endif()
endif()

if(NOT OpenCV_FOUND)
  message(FATAL_ERROR "OpenCV not found via CMake or pkg-config.")
endif()

add_library(OpenCV::opencv_core ALIAS opencv_core)
add_library(OpenCV::opencv_imgproc ALIAS opencv_imgproc)

## ONNX Runtime
include(onnxruntime)

# Plugin build definitions
add_library(${CMAKE_PROJECT_NAME} MODULE)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
)
set_target_properties(
  ${CMAKE_PROJECT_NAME}
  PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
)
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
