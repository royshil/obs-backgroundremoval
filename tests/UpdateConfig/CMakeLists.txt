# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_subdirectory(
  "${CMAKE_SOURCE_DIR}/vendor/obs-studio-libobs-stub"
  "${CMAKE_CURRENT_BINARY_DIR}/obs-studio-libobs-stub"
)

add_executable(UpdateConfigSemverTest UpdateConfigSemverTest.cpp "${PLUGIN_SOURCE_DIR}/UpdateConfig/UpdateConfigSemver.cpp")
target_include_directories(UpdateConfigSemverTest PRIVATE "${PLUGIN_SOURCE_DIR}")
discover_test_cases(UpdateConfigSemverTest.cpp UPDATE_CONFIG_SEMVER_TEST_CASES)
foreach(TEST_CASE IN LISTS UPDATE_CONFIG_SEMVER_TEST_CASES)
  add_test(NAME "UpdateConfigSemverTest.${TEST_CASE}" COMMAND UpdateConfigSemverTest "${TEST_CASE}")
endforeach()

add_executable(
  UpdateConfigCurlTest
  UpdateConfigCurlTest.cpp
  test_helpers.cpp
  "${PLUGIN_SOURCE_DIR}/UpdateConfig/UpdateConfigCommon.cpp"
  "${PLUGIN_SOURCE_DIR}/UpdateConfig/UpdateConfigCurl.cpp"
)
target_include_directories(UpdateConfigCurlTest PRIVATE "${PLUGIN_SOURCE_DIR}")
discover_test_cases(UpdateConfigCurlTest.cpp UPDATE_CONFIG_CURL_TEST_CASES)
foreach(TEST_CASE IN LISTS UPDATE_CONFIG_CURL_TEST_CASES)
  add_test(NAME "UpdateConfigCurlTest.${TEST_CASE}" COMMAND UpdateConfigCurlTest "${TEST_CASE}")
  list(APPEND UPDATE_CONFIG_CURL_TEST_NAMES "UpdateConfigCurlTest.${TEST_CASE}")
endforeach()
target_include_directories(UpdateConfigCurlTest PRIVATE "${CMAKE_SOURCE_DIR}/vendor/cpp-httplib")
target_link_libraries(UpdateConfigCurlTest PRIVATE CURL::libcurl libobs-stub)
if(APPLE)
  target_link_libraries(UpdateConfigCurlTest PRIVATE jthread::jthread)
endif()

if(WIN32)
  set_tests_properties(
    ${UPDATE_CONFIG_CURL_TEST_NAMES}
    PROPERTIES ENVIRONMENT_MODIFICATION
               "PATH=path_list_prepend:${CMAKE_SOURCE_DIR}/.deps/obs-deps/bin"
  )
endif()
