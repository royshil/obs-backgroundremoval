# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28)

### Options
set(PLUGIN_NAME "obs-backgroundremoval" CACHE STRING "Name of plugin")
set(PLUGIN_DISPLAY_NAME "OBS Background Removal" CACHE STRING "Display name of plugin")
set(PLUGIN_AUTHOR "Roy Shilkrot" CACHE STRING "Author of plugin")
set(PLUGIN_EMAIL "roy.shil@gmail.com" CACHE STRING "Author email of plugin")
set(PLUGIN_BUILD_NUMBER "1" CACHE STRING "Build number in plugin metadata")
set(PLUGIN_BUNDLE_ID "com.royshilkrot.obs-backgroundremoval" CACHE STRING "Bundle ID of plugin installer")
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
  add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
  add_compile_options(
    /utf-8
    /Zc:preprocessor
    $<$<COMPILE_LANGUAGE:CXX>:/permissive->
    $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus>
  )
elseif(APPLE OR OS_LINUX)
  add_compile_options(-fno-omit-frame-pointer -fstack-protector-strong)
endif()

set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO RelWithDebInfo Release)

### Configure paths
if(DEFINED ENV{PLUGIN_BUILD_DIR})
  set(PLUGIN_BUILD_DIR "$ENV{PLUGIN_BUILD_DIR}")
else()
  set(PLUGIN_BUILD_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
endif()

if(MSVC)
  list(
    PREPEND
    CMAKE_PREFIX_PATH
    "${PLUGIN_BUILD_DIR}/.deps/obs-deps-prebuilt"
    "${PLUGIN_BUILD_DIR}/.deps/obs-deps-qt6"
    "${PLUGIN_BUILD_DIR}/obs_installed"
    "${PLUGIN_BUILD_DIR}/build_ort/Release/vcpkg_installed/x64-windows-static-md"
    "${PLUGIN_BUILD_DIR}/ort_installed"
    "${PLUGIN_BUILD_DIR}/vcpkg_installed/x64-windows-static-md-obs"
  )
elseif(APPLE)
  list(
    PREPEND
    CMAKE_PREFIX_PATH
    "${PLUGIN_BUILD_DIR}/.deps/obs-deps-prebuilt"
    "${PLUGIN_BUILD_DIR}/.deps/obs-deps-qt6"
    "${PLUGIN_BUILD_DIR}/obs_installed"
    "${PLUGIN_BUILD_DIR}/obs_installed/Frameworks"
    "${PLUGIN_BUILD_DIR}/vcpkg_ort_installed/universal-osx"
    "${PLUGIN_BUILD_DIR}/ort_installed"
    "${PLUGIN_BUILD_DIR}/vcpkg_installed/universal-osx-obs"
  )
endif()

### Package dependencies
find_package(PkgConfig QUIET)

find_package(libobs REQUIRED)
find_package(obs-frontend-api REQUIRED)

block(PROPAGATE CURL_FOUND)
  find_package(CURL QUIET)

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

  if(TARGET opencv_geometry AND NOT TARGET OpenCV::opencv_geometry)
    add_library(OpenCV::opencv_geometry ALIAS opencv_geometry)
  endif()
endblock()

block(PROPAGATE onnxruntime_FOUND)
  set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS ON)

  if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
  endif()

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
add_library(obs-backgroundremoval_options INTERFACE)

if(MSVC)
  target_compile_options(obs-backgroundremoval_options INTERFACE /W4 /WX)
elseif(APPLE OR OS_LINUX)
  target_compile_definitions(obs-backgroundremoval_options INTERFACE SIMDE_ENABLE_OPENMP)
  target_compile_options(obs-backgroundremoval_options INTERFACE -fopenmp-simd -Wall -Wextra -Werror)
endif()

add_library(${CMAKE_PROJECT_NAME} MODULE)

add_subdirectory(src/update-checker/CurlClient)
target_link_libraries(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    obs-backgroundremoval_options
    OBS::libobs
    OBS::obs-frontend-api
    onnxruntime::onnxruntime
    CurlClient
    OpenCV::opencv_core
    OpenCV::opencv_imgproc
    $<$<TARGET_EXISTS:OpenCV::opencv_geometry>:OpenCV::opencv_geometry>
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

configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/packaging/pkg/Distribution.xml.in"
  "${CMAKE_CURRENT_BINARY_DIR}/Distribution"
  @ONLY
)

file(
  CONFIGURE
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/codesign_after_install.cmake"
  CONTENT
    [[
set(INSTALL_DIR "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}")
set(PLUGIN_DIR "${INSTALL_DIR}/Library/Application Support/obs-studio/plugins")

set(BUNDLE_PATH "${PLUGIN_DIR}/@CMAKE_PROJECT_NAME@.plugin")

if(EXISTS "${BUNDLE_PATH}")
  message(STATUS "Re-signing: ${BUNDLE_PATH}...")

  execute_process(
    COMMAND xattr -cr "${BUNDLE_PATH}"
    COMMAND_ERROR_IS_FATAL NONE
  )

  execute_process(
    COMMAND dot_clean -m "${BUNDLE_PATH}"
    COMMAND_ERROR_IS_FATAL NONE
  )

  execute_process(
    COMMAND codesign -s - -f -o runtime --deep "${BUNDLE_PATH}"
    COMMAND_ERROR_IS_FATAL ANY
  )
endif()
]]
)
install(CODE "")

if(APPLE)
  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION "plugin")
  set_source_files_properties(${EFFECTS_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/effects")
  set_source_files_properties(${LOCALE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/locale")
  set_source_files_properties(${MODELS_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/models")
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${EFFECTS_FILES} ${LOCALE_FILES} ${MODELS_FILES})

  install(
    TARGETS "${CMAKE_PROJECT_NAME}"
    BUNDLE DESTINATION "Library/Application Support/obs-studio/plugins"
    LIBRARY DESTINATION "Library/Application Support/obs-studio/plugins"
  )

  install(SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/codesign_after_install.cmake")
elseif(MSVC)
  set(OBS_PLUGIN_BIN_DIR "${CMAKE_PROJECT_NAME}/bin/64bit")
  set(OBS_PLUGIN_DATA_DIR "${CMAKE_PROJECT_NAME}/data")

  install(
    TARGETS "${CMAKE_PROJECT_NAME}"
    RUNTIME DESTINATION "${OBS_PLUGIN_BIN_DIR}"
    LIBRARY DESTINATION "${OBS_PLUGIN_BIN_DIR}"
  )
  install(FILES ${EFFECTS_FILES} DESTINATION "${OBS_PLUGIN_DATA_DIR}/effects")
  install(FILES ${LOCALE_FILES} DESTINATION "${OBS_PLUGIN_DATA_DIR}/locale")
  install(FILES ${MODELS_FILES} DESTINATION "${OBS_PLUGIN_DATA_DIR}/models" OPTIONAL)
elseif(OS_LINUX)
  set(OBS_PLUGIN_DATA_DIR "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}")
  set(OBS_PLUGIN_DOC_DIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/${CMAKE_PROJECT_NAME}")
  set(OBS_PLUGIN_LIB_DIR "${CMAKE_INSTALL_LIBDIR}/obs-plugins")

  set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")

  install(
    TARGETS "${CMAKE_PROJECT_NAME}"
    BUNDLE DESTINATION "${OBS_PLUGIN_LIB_DIR}"
    LIBRARY DESTINATION "${OBS_PLUGIN_LIB_DIR}"
  )
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

set(CPACK_ARCHIVE_THREADS 0)
set(CPACK_OUTPUT_FILE_PREFIX "release")
set(CPACK_SET_DESTDIR TRUE)
set(CPACK_VERBATIM_VARIABLES TRUE)

set(CPACK_DEBIAN_COMPRESSION_LEVEL 19)
set(CPACK_DEBIAN_COMPRESSION_TYPE zstd)
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE TRUE)
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS TRUE)

include(CPack) # This line must be placed after all the package configurations

### Export variables
block()
  set(SH_CONTENT "")
  set(PS1_CONTENT "")

  foreach(VAR_NAME PLUGIN_BUNDLE_ID PLUGIN_DISPLAY_NAME PLUGIN_NAME PLUGIN_VERSION)
    set(VAR_VALUE "${${VAR_NAME}}")
    if(NOT VAR_VALUE MATCHES [[^[-A-Za-z0-9._ ]+$]])
      message(FATAL_ERROR "${VAR_NAME} contains unsupported characters: ${VAR_VALUE}")
    endif()
    string(APPEND SH_CONTENT "${VAR_NAME}=\'${VAR_VALUE}\'\n")
    string(APPEND PS1_CONTENT "\$${VAR_NAME} = \'${VAR_VALUE}\'\n")
  endforeach()

  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmake_export.sh" "${SH_CONTENT}")
  file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/cmake_export.ps1" "${PS1_CONTENT}")
endblock()
