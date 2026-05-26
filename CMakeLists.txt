# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28)

### Options
set(PLUGIN_AUTHOR "Roy Shilkrot" CACHE STRING "Author of plugin metadata")
set(PLUGIN_EMAIL "roy.shil@gmail.com" CACHE STRING "Email of plugin metadata")
set(PLUGIN_BUILD_NUMBER "1" CACHE STRING "Build number of plugin metadata")
set(PACKAGE_VERSION_SUFFIX "" CACHE STRING "Version suffix for plugin package")

### Project
block(PROPAGATE PLUGIN_VERSION)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VERSION_CONTENT)
  string(STRIP "${VERSION_CONTENT}" PLUGIN_VERSION)
endblock()

project(obs-backgroundremoval VERSION "${PLUGIN_VERSION}" LANGUAGES C CXX)

block(PROPAGATE OS_LINUX_DETECTED)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(EXISTS "/etc/os-release")
      file(STRINGS "/etc/os-release" OS_RELEASE_ID_LINE REGEX "^ID=")
      string(TOLOWER "${OS_RELEASE_ID_LINE}" OS_RELEASE_ID_LINE_LOWER)

      if(OS_RELEASE_ID_LINE_LOWER MATCHES [[^id=['"]?debian['"]?$]])
        set(OS_LINUX_DETECTED Debian)
      elseif(OS_RELEASE_ID_LINE_LOWER MATCHES [[^id=['"]?ubuntu['"]?$]])
        set(OS_LINUX_DETECTED Ubuntu)
      else()
        set(OS_LINUX_DETECTED Linux)
      endif()
    else()
      set(OS_LINUX_DETECTED Linux)
    endif()
  else()
    set(OS_LINUX_DETECTED FALSE)
  endif()
endblock()

set(OS_LINUX ${OS_LINUX_DETECTED} CACHE STRING "Linux variant: Linux Debian Ubuntu FALSE")

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT AND OS_LINUX MATCHES "(Debian|Ubuntu)")
  set(CMAKE_INSTALL_PREFIX "/usr" CACHE PATH "Install prefix" FORCE)
endif()

### CMake modules
include(GNUInstallDirs)

### Global configurations
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_C_EXTENSIONS FALSE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

if(MSVC)
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
elseif(APPLE OR OS_LINUX)
  add_compile_options(-fno-omit-frame-pointer -fstack-protector-strong)
endif()

### Package dependencies
find_package(PkgConfig QUIET)
find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)

block(PROPAGATE CURL_FOUND)
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
endblock()

block(PROPAGATE OpenCV_FOUND)
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
endblock()

block(PROPAGATE onnxruntime_FOUND)
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
endblock()

### Plugin main
add_library(${CMAKE_PROJECT_NAME} MODULE)

if(MSVC)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE /W4 /WX /MP)
elseif(APPLE OR OS_LINUX)
  target_compile_options(${CMAKE_PROJECT_NAME} PRIVATE -Wall -Wextra -Werror)
endif()

add_subdirectory(src/update-checker/CurlClient)
target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    OBS::libobs
    OBS::obs-frontend-api
    onnxruntime::onnxruntime
    CurlClient
    OpenCV::opencv_core
    OpenCV::opencv_imgproc
)

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.c
    src/plugin-support.c
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

### Hide symbols
if(APPLE)
  target_link_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/src/exported_symbols_macos.txt"
  )
elseif(MSVC)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/exported_symbols_windows.def")
elseif(OS_LINUX)
  target_link_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/src/exported_symbols_linux.map" "-Wl,-Bsymbolic"
  )
endif()

### Install configurations
set(
  EFFECTS_FILES
  data/effects/blend_images.effect
  data/effects/kawase_blur.effect
  data/effects/mask_alpha_filter.effect
)

set(
  LOCALE_FILES
  data/locale/ar-EG.ini
  data/locale/bn-IN.ini
  data/locale/en-US.ini
  data/locale/es-SP.ini
  data/locale/fr-FR.ini
  data/locale/hi-IN.ini
  data/locale/it-IT.ini
  data/locale/ja-JP.ini
  data/locale/ko-KR.ini
  data/locale/pt-BR.ini
  data/locale/ru-RU.ini
  data/locale/ta-IN.ini
  data/locale/tr-TR.ini
  data/locale/zh-CN.ini
)

set(
  MODELS_FILES
  data/models/mediapipe.with_runtime_opt.ort
  data/models/mediapipe.with_runtime_opt.ort.license
  data/models/pphumanseg_fp32.with_runtime_opt.ort
  data/models/pphumanseg_fp32.with_runtime_opt.ort.license
  data/models/rvm_mobilenetv3_fp32.with_runtime_opt.ort
  data/models/rvm_mobilenetv3_fp32.with_runtime_opt.ort.license
  data/models/selfie_multiclass_256x256.with_runtime_opt.ort
  data/models/selfie_multiclass_256x256.with_runtime_opt.ort.license
  data/models/selfie_segmentation.with_runtime_opt.ort
  data/models/selfie_segmentation.with_runtime_opt.ort.license
  data/models/semantic_guided_llie_180x324.with_runtime_opt.ort
  data/models/semantic_guided_llie_180x324.with_runtime_opt.ort.license
  data/models/SINet_Softmax_simple.with_runtime_opt.ort
  data/models/SINet_Softmax_simple.with_runtime_opt.ort.license
  data/models/tbefn_fp32.with_runtime_opt.ort
  data/models/tbefn_fp32.with_runtime_opt.ort.license
  data/models/tcmonodepth_tcsmallnet_192x320.with_runtime_opt.ort
  data/models/tcmonodepth_tcsmallnet_192x320.with_runtime_opt.ort.license
  data/models/uretinex_net_180x320.with_runtime_opt.ort
  data/models/uretinex_net_180x320.with_runtime_opt.ort.license
)

if(APPLE)
elseif(MSVC)
elseif(OS_LINUX MATCHES "(Debian|Ubuntu)")
  set(OBS_PLUGIN_DATA_DIR "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}")
  set(OBS_PLUGIN_DOC_DIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/${CMAKE_PROJECT_NAME}")
  set(OBS_PLUGIN_LIB_DIR "${CMAKE_INSTALL_LIBDIR}/obs-plugins")

  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")

  install(TARGETS "${CMAKE_PROJECT_NAME}" LIBRARY DESTINATION "${OBS_PLUGIN_LIB_DIR}")
  install(FILES ${EFFECTS_FILES} DESTINATION "${OBS_PLUGIN_DATA_DIR}/effects")
  install(FILES ${LOCALE_FILES} DESTINATION "${OBS_PLUGIN_DATA_DIR}/locale")
  install(FILES ${MODELS_FILES} DESTINATION "${OBS_PLUGIN_DATA_DIR}/models" OPTIONAL)
  install(FILES LICENSE DESTINATION "${OBS_PLUGIN_DOC_DIR}" RENAME copyright)
else()
  message("Installing on this platform is not supported officially.")
endif()

### CPack configurations
set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}${PACKAGE_VERSION_SUFFIX}")
set(CPACK_PACKAGE_CONTACT "${PLUGIN_AUTHOR} <${PLUGIN_EMAIL}>")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

set(CPACK_ARCHIVE_THREADS 0)
set(CPACK_OUTPUT_FILE_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}/release")
set(CPACK_SET_DESTDIR TRUE)
set(CPACK_VERBATIM_VARIABLES TRUE)

set(CPACK_DEBIAN_COMPRESSION_LEVEL 19)
set(CPACK_DEBIAN_COMPRESSION_TYPE zstd)
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE TRUE)
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS TRUE)

include(CPack) # This line must be placed after all the package configurations
