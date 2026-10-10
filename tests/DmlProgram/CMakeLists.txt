# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(
  DmlProgramTest
  DmlProgramTest.cpp
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlCompiler.cpp"
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlProgramRuntime.cpp"
)
target_include_directories(DmlProgramTest PRIVATE "${PLUGIN_SOURCE_DIR}")
target_link_libraries(DmlProgramTest PRIVATE d3d12 directml)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/DmlProgramTest.cpp")
discover_test_cases(DmlProgramTest.cpp DML_TEST_CASES)
foreach(TEST_CASE IN LISTS DML_TEST_CASES)
  add_test(NAME "DmlProgramTest.${TEST_CASE}" COMMAND DmlProgramTest "${TEST_CASE}")
  set_tests_properties("DmlProgramTest.${TEST_CASE}" PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
endforeach()
