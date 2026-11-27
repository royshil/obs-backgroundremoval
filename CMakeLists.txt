# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28)

option(PROD "Production build" FALSE)
set(PACKAGE_VERSION_SUFFIX "" CACHE STRING "Version suffix for plugin package")

if(NOT DEFINED PLUGIN_VERSION)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PLUGIN_VERSION LIMIT_COUNT 1)
endif()

project(obs-backgroundremoval VERSION "${PLUGIN_VERSION}" LANGUAGES C CXX)

set(PLUGIN_DISPLAY_NAME "OBS Background Removal")
set(PLUGIN_COPYRIGHT "Copyright (C) 2021-2026 Roy Shilkrot; Copyright (C) 2023-2026 Kaito Udagawa")
set(PLUGIN_AUTHOR "Roy Shilkrot")
set(PLUGIN_EMAIL "roy.shil@gmail.com")
set(PLUGIN_BUNDLE_ID "com.royshilkrot.obs-backgroundremoval")

if(WIN32)
    set(obs_deps_qt6_url "https://github.com/obsproject/obs-deps/releases/download/2024-05-08/windows-deps-qt6-2024-05-08-x64.zip")
    set(obs_deps_qt6_hash "SHA256=8f459af5115ce081ae24b108712327e113893f250e14a902b1bd188b43873ed1")
elseif(APPLE)
    set(obs_deps_qt6_url "https://github.com/obsproject/obs-deps/releases/download/2026-07-15/macos-deps-qt6-2026-07-15-universal.tar.xz")
    set(obs_deps_qt6_hash "SHA256=d4b8058612a7067e44b2205fe7925ee24e9ec6b15ac8c29d3e6230c70030b102")
endif()

if(NOT DEFINED PLUGIN_BUILD_NUMBER)
    if(DEFINED ENV{GITHUB_RUN_ID})
        set(PLUGIN_BUILD_NUMBER "$ENV{GITHUB_RUN_ID}")
    else()
        set(PLUGIN_BUILD_NUMBER "1")
    endif()
endif()

include(CTest)
include(FetchContent)
include(GNUInstallDirs)

# MARK: - Plugin dependencies

# MARK: OBS dependencies

if(WIN32 OR APPLE)
    add_library(OBS::libobs INTERFACE IMPORTED)
    target_include_directories(OBS::libobs INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/obsproject/obs-studio/libobs")
    add_library(OBS::obs-frontend-api INTERFACE IMPORTED)
    target_include_directories(OBS::obs-frontend-api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/obsproject/obs-studio/UI/obs-frontend-api")
    target_link_libraries(OBS::obs-frontend-api INTERFACE OBS::libobs)

    if(WIN32)
        target_link_libraries(OBS::libobs INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/obs-deps/lib/x64-windows/obs.lib")
        target_link_libraries(OBS::obs-frontend-api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/obs-deps/lib/x64-windows/obs-frontend-api.lib")

        add_library(CURL::libcurl INTERFACE IMPORTED)
        target_include_directories(CURL::libcurl INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/curl/curl/include")
        target_link_libraries(CURL::libcurl INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/obs-deps/lib/x64-windows/libcurl_imp.lib")
    elseif(APPLE)
        target_link_libraries(OBS::libobs INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdk/obs-29.1.3-macos/lib/libobs.tbd")
        target_link_libraries(OBS::obs-frontend-api INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdk/obs-29.1.3-macos/lib/obs-frontend-api.tbd")
    endif()
else()
    find_package(libobs REQUIRED)
    find_package(obs-frontend-api REQUIRED)
endif()

# MARK: Qt6 dependencies

if(WIN32 OR APPLE)
    FetchContent_Declare(obs-deps-qt6 URL ${obs_deps_qt6_url} URL_HASH ${obs_deps_qt6_hash} DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
    FetchContent_MakeAvailable(obs-deps-qt6)
    list(PREPEND CMAKE_PREFIX_PATH "${obs-deps-qt6_SOURCE_DIR}")
endif()

find_package(Qt6 COMPONENTS Widgets REQUIRED)

# MARK: cURL dependency

if(NOT TARGET CURL::libcurl)
    find_package(CURL REQUIRED)
endif()

# MARK: - Plugin main

# MARK: Plugin support

add_library(plugin-support INTERFACE)
target_compile_definitions(plugin-support INTERFACE PLUGIN_VERSION="${PLUGIN_VERSION}")
target_include_directories(plugin-support INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/src")

if(MSVC)
    target_compile_options(
        plugin-support
        INTERFACE /utf-8 /Zc:preprocessor $<$<COMPILE_LANGUAGE:CXX>:/permissive-> $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus> /W4 /WX
    )
else()
    target_compile_options(
        plugin-support
        INTERFACE -fno-omit-frame-pointer -fstack-protector-strong -Wall -Wextra -Werror
    )

    if(NOT PROD)
        target_compile_options(plugin-support INTERFACE -fsanitize=undefined -fno-sanitize-recover=undefined)
        target_link_options(plugin-support INTERFACE -fsanitize=undefined -fno-sanitize-recover=undefined)
    endif()
endif()

# MARK: Plugin pluginlibs

add_library(${CMAKE_PROJECT_NAME}_pluginlibs STATIC)
target_compile_features(${CMAKE_PROJECT_NAME}_pluginlibs PUBLIC c_std_17 cxx_std_20)
target_include_directories(${CMAKE_PROJECT_NAME}_pluginlibs PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/include" src)
target_sources(
    ${CMAKE_PROJECT_NAME}_pluginlibs
    PRIVATE
        src/BackgroundRemoval/FilterContext.cpp
        src/BackgroundRemoval/MaskCompositor/MaskCompositor.cpp
        src/BackgroundRemoval/MediaPipeLandscapeShader/MPLSEffect.cpp
        src/BackgroundRemoval/MediaPipeLandscapeShader/MPLSPipeline.cpp
        src/BackgroundRemoval/MediaPipeLandscapeShader/generated/MPLSPipeline.generated.cpp
        src/BackgroundRemoval/PipelineFactory.cpp
        src/PluginLoader.cpp
        src/UI/AboutDialog.cpp
        src/UI/AboutDialogIntegration.cpp
        src/UpdateConfig/UpdateConfigCommon.cpp
        src/UpdateConfig/UpdateConfigCurl.cpp
        src/UpdateConfig/UpdateConfigSemver.cpp
        src/legal.cpp
)
target_link_libraries(
    ${CMAKE_PROJECT_NAME}_pluginlibs
    PUBLIC CURL::libcurl Qt6::Widgets OBS::libobs OBS::obs-frontend-api plugin-support
)
set_target_properties(
    ${CMAKE_PROJECT_NAME}_pluginlibs
    PROPERTIES
        C_EXTENSIONS OFF
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

# MARK: License files

set(LICENSE_FILES
    LICENSE
    NOTICE
    LICENSES/Apache-2.0.txt
    LICENSES/CC-BY-4.0.txt
    LICENSES/CC-BY-SA-4.0.txt
    LICENSES/CC0-1.0.txt
    LICENSES/curl.txt
    LICENSES/GPL-2.0-or-later.txt
    LICENSES/GPL-3.0-or-later.txt
    LICENSES/MIT.txt
)

block()
    foreach(license_file IN LISTS LICENSE_FILES)
        file(READ "${CMAKE_CURRENT_SOURCE_DIR}/${license_file}" license_content)
        file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/include/${license_file}.inc" CONTENT "R\"AZBYCX(@license_content@)AZBYCX\"" @ONLY)
    endforeach()
endblock()

# MARK: Plugin main module

add_library(${CMAKE_PROJECT_NAME} MODULE src/plugin-main.c)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE ${CMAKE_PROJECT_NAME}_pluginlibs)

if(NOT WIN32 AND NOT APPLE)
    # Hides STL symbols with libstdc++. See SP-00009.
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE LINKER:--exclude-libs,ALL)
endif()

set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES
        C_EXTENSIONS OFF
        CXX_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
        CXX_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN ON
)

# MARK: Testing

if(BUILD_TESTING)
    enable_testing()
    set(PLUGIN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_subdirectory(tests)
endif()

# MARK: - Packaging

# MARK: Plugin installation

set(LOCALE_FILES data/locale/en-US.ini)

if(WIN32)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/data/obs-module.rc.in" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.rc" @ONLY)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_PROJECT_NAME}.rc")

    # cmake -B build && cmake --build build --config RelWithDebInfo && cmake --install build --config RelWithDebInfo --prefix "C:\ProgramData\obs-studio\plugins"
    install(TARGETS "${CMAKE_PROJECT_NAME}" LIBRARY DESTINATION "${CMAKE_PROJECT_NAME}/bin/64bit")
    install(FILES ${LOCALE_FILES} DESTINATION "${CMAKE_PROJECT_NAME}/data/locale")
    install(FILES data/logo.png DESTINATION "${CMAKE_PROJECT_NAME}/data")
elseif(APPLE)
    set_target_properties(
        ${CMAKE_PROJECT_NAME}
        PROPERTIES
            BUNDLE TRUE
            BUNDLE_EXTENSION "plugin"
            MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/data/Info.plist.in"
            XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${PLUGIN_BUNDLE_ID}"
    )
    set_source_files_properties(${LOCALE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/locale")
    set_source_files_properties(data/logo.png PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${LOCALE_FILES} data/logo.png)

    # cmake -B build -G Xcode && cmake --build build --config RelWithDebInfo && cmake --install build --config RelWithDebInfo --prefix ~ && codesign -f -s - -o runtime ~/"Library/Application Support/obs-studio/plugins/obs-backgroundremoval.plugin"
    install(TARGETS "${CMAKE_PROJECT_NAME}" LIBRARY DESTINATION "Library/Application Support/obs-studio/plugins")
else()
    # OBS plugins SHOULD NOT have a `lib` prefix for their shared libraries
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")

    # cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX=/usr && cmake --build build && sudo cmake --install build
    install(TARGETS "${CMAKE_PROJECT_NAME}" LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}/obs-plugins")
    install(FILES ${LOCALE_FILES} DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}/locale")
    install(FILES data/logo.png DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}")
    install(FILES LICENSE DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/doc/${CMAKE_PROJECT_NAME}" RENAME copyright)

    # cmake -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo && cmake --build build && cmake --install build --prefix ~/.config/obs-studio/plugins/obs-backgroundremoval --component UserPlugin
    install(TARGETS "${CMAKE_PROJECT_NAME}" LIBRARY DESTINATION "bin/64bit" COMPONENT UserPlugin EXCLUDE_FROM_ALL)
    install(FILES ${LOCALE_FILES} DESTINATION "data/locale" COMPONENT UserPlugin EXCLUDE_FROM_ALL)
    install(FILES data/logo.png DESTINATION "data" COMPONENT UserPlugin EXCLUDE_FROM_ALL)
endif()

# MARK: Packaging configuration

set(CPACK_PACKAGE_NAME "${CMAKE_PROJECT_NAME}")
set(CPACK_PACKAGE_VERSION "${CMAKE_PROJECT_VERSION}${PACKAGE_VERSION_SUFFIX}")
set(CPACK_PACKAGE_CONTACT "${PLUGIN_AUTHOR} <${PLUGIN_EMAIL}>")

set(CPACK_ARCHIVE_THREADS 0)
set(CPACK_SET_DESTDIR TRUE)
set(CPACK_VERBATIM_VARIABLES TRUE)

set(CPACK_DEBIAN_COMPRESSION_LEVEL 19)
set(CPACK_DEBIAN_COMPRESSION_TYPE zstd)
set(CPACK_DEBIAN_DEBUGINFO_PACKAGE TRUE)
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS TRUE)
set(CPACK_DEBIAN_FILE_NAME "DEB-DEFAULT" CACHE STRING "DEB package file name")

if(APPLE)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/packaging/pkg/Distribution.xml.in" "${CMAKE_CURRENT_BINARY_DIR}/Distribution" @ONLY)
endif()

include(CPack)
