# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

add_executable(AboutDialogRunner AboutDialogRunner.cpp)
target_include_directories(AboutDialogRunner PRIVATE "${PLUGIN_SOURCE_DIR}")
target_link_libraries(AboutDialogRunner PRIVATE Qt6::Widgets)
set_target_properties(AboutDialogRunner PROPERTIES AUTORCC ON)

target_sources(
  AboutDialogRunner
  PRIVATE "${PLUGIN_SOURCE_DIR}/UI/AboutDialog.cpp" "${PLUGIN_SOURCE_DIR}/Resources/resources.qrc"
)

if(COMMAND collect_licenses)
  set(test_legal_header "${CMAKE_CURRENT_BINARY_DIR}/legal.hpp")
  collect_licenses("${test_legal_header}")
  target_sources(AboutDialogRunner PRIVATE "${test_legal_header}")
  target_include_directories(AboutDialogRunner PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
endif()
