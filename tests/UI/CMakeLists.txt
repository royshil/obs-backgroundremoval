# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

add_executable(AboutDialogRunner AboutDialogRunner.cpp)
target_include_directories(AboutDialogRunner PRIVATE "${PLUGIN_SOURCE_DIR}")
target_link_libraries(AboutDialogRunner PRIVATE Qt6::Widgets)
target_compile_features(AboutDialogRunner PRIVATE cxx_std_20)

target_sources(AboutDialogRunner PRIVATE "${PLUGIN_SOURCE_DIR}/UI/AboutDialog.cpp")
target_compile_definitions(AboutDialogRunner PRIVATE ABOUT_DIALOG_LOGO_FILE="${PROJECT_SOURCE_DIR}/data/logo.png")

if(COMMAND collect_licenses)
    set(test_legal_header "${CMAKE_CURRENT_BINARY_DIR}/legal.hpp")
    collect_licenses("${test_legal_header}")
    target_sources(AboutDialogRunner PRIVATE "${test_legal_header}")
    target_include_directories(AboutDialogRunner PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endif()
