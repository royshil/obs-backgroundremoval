# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28)

if(POLICY CMP0167)
    cmake_policy(SET CMP0167 NEW)
endif()

set(PACKAGE_VERSION_SUFFIX "" CACHE STRING "Version suffix for plugin package")

block(PROPAGATE PLUGIN_VERSION)
    file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" VERSION_CONTENT)
    string(STRIP "${VERSION_CONTENT}" PLUGIN_VERSION)
endblock()

project(obs-backgroundremoval VERSION "${PLUGIN_VERSION}" LANGUAGES C CXX)

if(APPLE AND NOT CMAKE_GENERATOR STREQUAL "Xcode")
    message(FATAL_ERROR "The macOS plugin requires the Xcode generator.")
endif()

include(CTest)
include(FetchContent)
include(GNUInstallDirs)

include(./cmake/collect_licenses.cmake OPTIONAL)

set(PLUGIN_DISPLAY_NAME "OBS Background Removal" CACHE STRING "Display name for plugin")
set(PLUGIN_AUTHOR "Roy Shilkrot" CACHE STRING "Author of the plugin")
set(PLUGIN_EMAIL "roy.shil@gmail.com" CACHE STRING "Email of the plugin author")
set(PLUGIN_BUNDLE_ID "com.royshilkrot.obs-backgroundremoval" CACHE STRING "Bundle ID for the plugin")
set(PLUGIN_LATEST_VERSION_URL "https://royshil.github.io/obs-backgroundremoval/metadata/latest-version.txt" CACHE STRING "URL for the latest plugin version")

if(DEFINED ENV{GITHUB_RUN_ID})
    set(PLUGIN_BUILD_NUMBER "$ENV{GITHUB_RUN_ID}")
else()
    set(PLUGIN_BUILD_NUMBER "1")
endif()

### Global configurations
set(CMAKE_C_EXTENSIONS FALSE)
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED TRUE)
set(CMAKE_CXX_EXTENSIONS FALSE)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED TRUE)
set(CMAKE_POSITION_INDEPENDENT_CODE TRUE)

add_library(BuildOptions INTERFACE)

if(MSVC)
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
    target_compile_definitions(BuildOptions INTERFACE NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE)
    target_compile_options(
        BuildOptions
        INTERFACE /utf-8 /Zc:preprocessor $<$<COMPILE_LANGUAGE:CXX>:/permissive-> $<$<COMPILE_LANGUAGE:CXX>:/Zc:__cplusplus> /W4 /WX
    )
elseif(APPLE)
    set(CMAKE_MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release)
    target_compile_options(
        BuildOptions
        INTERFACE -fno-omit-frame-pointer -fstack-protector-strong -Wall -Wextra -Werror
    )
elseif(UNIX)
    target_compile_definitions(BuildOptions INTERFACE SIMDE_ENABLE_OPENMP)
    target_compile_options(
        BuildOptions
        INTERFACE -fno-omit-frame-pointer -fstack-protector-strong -fopenmp-simd -Wall -Wextra -Werror
    )
endif()

target_compile_definitions(BuildOptions INTERFACE PLUGIN_NAME_STR="${CMAKE_PROJECT_NAME}" PLUGIN_VERSION_STR="${PLUGIN_VERSION}" PLUGIN_LATEST_VERSION_URL="${PLUGIN_LATEST_VERSION_URL}" OBS_LOG_HEADER="[${CMAKE_PROJECT_NAME}] ")

### Dependency helpers
function(map_imported_executable_to_release TARGET_NAME FALLBACK_LOCATION)
    if(NOT TARGET "${TARGET_NAME}")
        return()
    endif()

    get_target_property(TARGET_TYPE "${TARGET_NAME}" TYPE)
    get_target_property(TARGET_LOCATION "${TARGET_NAME}" IMPORTED_LOCATION)
    if(NOT TARGET_LOCATION)
        set(TARGET_LOCATION "${FALLBACK_LOCATION}")
    endif()

    if(TARGET_TYPE STREQUAL "EXECUTABLE" AND TARGET_LOCATION)
        set_target_properties(
            "${TARGET_NAME}"
            PROPERTIES IMPORTED_CONFIGURATIONS RELEASE IMPORTED_LOCATION_RELEASE "${TARGET_LOCATION}" MAP_IMPORTED_CONFIG_RELWITHDEBINFO Release
        )
    endif()
endfunction()

### Package dependencies

if(WIN32)
    FetchContent_Declare(obs-deps-qt6 URL https://github.com/obsproject/obs-deps/releases/download/2024-05-08/windows-deps-qt6-2024-05-08-x64.zip URL_HASH SHA256=8f459af5115ce081ae24b108712327e113893f250e14a902b1bd188b43873ed1 DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
elseif(APPLE)
    FetchContent_Declare(obs-deps-qt6 URL https://github.com/obsproject/obs-deps/releases/download/2026-07-15/macos-deps-qt6-2026-07-15-universal.tar.xz URL_HASH SHA256=d4b8058612a7067e44b2205fe7925ee24e9ec6b15ac8c29d3e6230c70030b102 DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
endif()
if(WIN32 OR APPLE)
    FetchContent_MakeAvailable(obs-deps-qt6)
    list(PREPEND CMAKE_PREFIX_PATH "${obs-deps-qt6_SOURCE_DIR}")
endif()

find_package(PkgConfig QUIET)

if(APPLE)
    add_library(OBS::libobs INTERFACE IMPORTED)
    set_target_properties(
        OBS::libobs
        PROPERTIES INTERFACE_COMPILE_DEFINITIONS "SIMDE_ENABLE_OPENMP;ENABLE_HEVC" INTERFACE_COMPILE_OPTIONS "-fopenmp-simd" INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/obsproject/obs-studio/libobs" INTERFACE_LINK_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdk/obs-29.1.3-macos/lib/libobs.tbd"
    )

    add_library(OBS::obs-frontend-api INTERFACE IMPORTED)
    set_target_properties(OBS::obs-frontend-api PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/obsproject/obs-studio/UI/obs-frontend-api" INTERFACE_LINK_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/sdk/obs-29.1.3-macos/lib/obs-frontend-api.tbd;OBS::libobs")
elseif(WIN32)
    add_library(OBS::libobs INTERFACE IMPORTED)
    set_target_properties(OBS::libobs PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/obsproject/obs-studio/libobs" INTERFACE_LINK_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/obs-deps/lib/x64-windows/obs.lib")

    add_library(OBS::obs-frontend-api INTERFACE IMPORTED)
    set_target_properties(OBS::obs-frontend-api PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/obsproject/obs-studio/UI/obs-frontend-api" INTERFACE_LINK_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/obs-deps/lib/x64-windows/obs-frontend-api.lib;OBS::libobs")

    # The libcurl.dll that OBS Studio ships.
    add_library(CURL::libcurl INTERFACE IMPORTED)
    set_target_properties(CURL::libcurl PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/curl/curl/include" INTERFACE_LINK_LIBRARIES "${CMAKE_CURRENT_SOURCE_DIR}/vendor/obs-deps/lib/x64-windows/libcurl_imp.lib")
else()
    find_package(libobs REQUIRED)
    find_package(obs-frontend-api REQUIRED)
endif()

block()
    find_package(Qt6 COMPONENTS Widgets REQUIRED)

    # WORKAROUND: CMake 4 compatibility
    if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
        get_property(QT_TARGETS DIRECTORY PROPERTY IMPORTED_TARGETS)
        foreach(TARGET_NAME IN LISTS QT_TARGETS)
            if(TARGET_NAME MATCHES "^Qt::")
                map_imported_executable_to_release("${TARGET_NAME}" "")
            endif()
        endforeach()
    endif()

    # WORKAROUND: Remove deprecated AGL.framework when it is not available
    if(APPLE AND TARGET WrapOpenGL::WrapOpenGL)
        get_target_property(WRAP_OPENGL_LINK_LIBRARIES WrapOpenGL::WrapOpenGL INTERFACE_LINK_LIBRARIES)
        list(FILTER WRAP_OPENGL_LINK_LIBRARIES EXCLUDE REGEX "(^|/)AGL\\.framework$|^-framework AGL$")
        set_target_properties(WrapOpenGL::WrapOpenGL PROPERTIES INTERFACE_LINK_LIBRARIES "${WRAP_OPENGL_LINK_LIBRARIES}")
    endif()

    if(NOT Qt6_FOUND)
        message(FATAL_ERROR "Qt6 not found via CMake.")
    endif()
endblock()

block()
    if(NOT WIN32)
        if(APPLE)
            # WORKAROUND: Prefer system curl on macOS
            find_package(CURL MODULE QUIET)

            if(NOT CURL_FOUND)
                find_package(CURL CONFIG QUIET)
            endif()
        else()
            find_package(CURL QUIET)
        endif()

        if(NOT CURL_FOUND AND PkgConfig_FOUND)
            pkg_check_modules(PC_CURL libcurl IMPORTED_TARGET)
            if(PC_CURL_FOUND)
                add_library(CURL::libcurl ALIAS PkgConfig::PC_CURL)
                set(CURL_FOUND TRUE)
            endif()
        endif()

        if(NOT CURL_FOUND)
            message(FATAL_ERROR "cURL not found.")
        endif()
    endif()
endblock()

### Plugin main
add_subdirectory(vendor/josuttis-jthread)
add_subdirectory(src/UpdateConfig)

add_library(${CMAKE_PROJECT_NAME} MODULE)

target_link_libraries(
    ${CMAKE_PROJECT_NAME}
    PRIVATE BuildOptions OBS::libobs OBS::obs-frontend-api Qt6::Widgets UpdateConfig
)

target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE src)

target_sources(
    ${CMAKE_PROJECT_NAME}
    PRIVATE src/BackgroundRemoval/FilterContext.cpp src/BackgroundRemoval/MaskCompositor/MaskCompositor.cpp src/BackgroundRemoval/MediaPipeLandscapeShader/MPLSEffect.cpp src/BackgroundRemoval/MediaPipeLandscapeShader/MPLSPipeline.cpp src/BackgroundRemoval/MediaPipeLandscapeShader/generated/MPLSPipeline.generated.cpp src/BackgroundRemoval/ModuleText.cpp src/BackgroundRemoval/PipelineFactory.cpp src/Resources/resources.qrc src/UI/AboutDialog.cpp src/UI/AboutDialogIntegration.cpp src/plugin-main.cpp
)

set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES AUTORCC ON)

if(COMMAND collect_licenses)
    set(legal_header "${CMAKE_CURRENT_BINARY_DIR}/legal.hpp")
    collect_licenses("${legal_header}")
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${legal_header}")
    target_include_directories(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endif()

### Hide symbols
if(APPLE)
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/src/exported_symbols_macos.txt")
elseif(MSVC)
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/exported_symbols_windows.def")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_options(${CMAKE_PROJECT_NAME} PRIVATE "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/src/exported_symbols_linux.map" "-Wl,-Bsymbolic")
endif()

### Install configurations
set(LOCALE_FILES
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

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/packaging/pkg/Distribution.xml.in" "${CMAKE_CURRENT_BINARY_DIR}/Distribution" @ONLY)

if(APPLE)
    set_target_properties(
        ${CMAKE_PROJECT_NAME}
        PROPERTIES BUNDLE TRUE BUNDLE_EXTENSION "plugin" XCODE_ATTRIBUTE_PRODUCT_NAME "${CMAKE_PROJECT_NAME}" XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${PLUGIN_BUNDLE_ID}" XCODE_ATTRIBUTE_CURRENT_PROJECT_VERSION "${PLUGIN_BUILD_NUMBER}" XCODE_ATTRIBUTE_MARKETING_VERSION "${PLUGIN_VERSION}" XCODE_ATTRIBUTE_GENERATE_INFOPLIST_FILE YES XCODE_ATTRIBUTE_INFOPLIST_FILE "" XCODE_ATTRIBUTE_INFOPLIST_KEY_CFBundleDisplayName "${PLUGIN_DISPLAY_NAME}" XCODE_ATTRIBUTE_INFOPLIST_KEY_NSHumanReadableCopyright "Copyright © 2021-2026 Roy Shilkrot; Copyright © 2023-2026 Kaito Udagawa"
    )
    set_source_files_properties(${LOCALE_FILES} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources/locale")
    target_sources(${CMAKE_PROJECT_NAME} PRIVATE ${LOCALE_FILES})

    install(TARGETS "${CMAKE_PROJECT_NAME}" BUNDLE DESTINATION "Library/Application Support/obs-studio/plugins" LIBRARY DESTINATION "Library/Application Support/obs-studio/plugins")
elseif(MSVC)
    set(OBS_PLUGIN_BIN_DIR "${CMAKE_PROJECT_NAME}/bin/64bit")
    set(OBS_PLUGIN_DATA_DIR "${CMAKE_PROJECT_NAME}/data")

    install(TARGETS "${CMAKE_PROJECT_NAME}" RUNTIME DESTINATION "${OBS_PLUGIN_BIN_DIR}" LIBRARY DESTINATION "${OBS_PLUGIN_BIN_DIR}")
    install(FILES ${LOCALE_FILES} DESTINATION "${OBS_PLUGIN_DATA_DIR}/locale")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(OBS_PLUGIN_DATA_DIR "${CMAKE_INSTALL_DATAROOTDIR}/obs/obs-plugins/${CMAKE_PROJECT_NAME}")
    set(OBS_PLUGIN_DOC_DIR "${CMAKE_INSTALL_DATAROOTDIR}/doc/${CMAKE_PROJECT_NAME}")
    set(OBS_PLUGIN_LIB_DIR "${CMAKE_INSTALL_LIBDIR}/obs-plugins")

    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES PREFIX "")

    install(TARGETS "${CMAKE_PROJECT_NAME}" BUNDLE DESTINATION "${OBS_PLUGIN_LIB_DIR}" LIBRARY DESTINATION "${OBS_PLUGIN_LIB_DIR}")
    install(FILES ${LOCALE_FILES} DESTINATION "${OBS_PLUGIN_DATA_DIR}/locale")
    install(FILES LICENSE DESTINATION "${OBS_PLUGIN_DOC_DIR}" RENAME copyright)

    # User plugin
    install(TARGETS "${CMAKE_PROJECT_NAME}" LIBRARY DESTINATION "bin/64bit" COMPONENT UserPlugin EXCLUDE_FROM_ALL)
    install(FILES ${LOCALE_FILES} DESTINATION "data/locale" COMPONENT UserPlugin EXCLUDE_FROM_ALL)
else()
    message("Installing on this platform is not supported officially.")
endif()

### Testing
if(BUILD_TESTING)
    enable_testing()
    set(PLUGIN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src")
    add_subdirectory(tests)
endif()

### CPack
include(./cmake/cpack.cmake OPTIONAL)
