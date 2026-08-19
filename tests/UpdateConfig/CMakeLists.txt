# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

function(configure_update_config_test TARGET_NAME)
  target_include_directories("${TARGET_NAME}" PRIVATE "${PLUGIN_SOURCE_DIR}/UpdateConfig")
  target_link_libraries("${TARGET_NAME}" PRIVATE BuildOptions OBS::libobs)

  if(APPLE)
    set(TEST_BUILD_RPATHS)
    foreach(PREFIX IN LISTS CMAKE_PREFIX_PATH)
      if(IS_DIRECTORY "${PREFIX}/lib")
        list(APPEND TEST_BUILD_RPATHS "${PREFIX}/lib")
      endif()
    endforeach()
    set_target_properties("${TARGET_NAME}" PROPERTIES BUILD_RPATH "${TEST_BUILD_RPATHS}")
  endif()

  add_test(NAME "${TARGET_NAME}" COMMAND "${TARGET_NAME}")
endfunction()

add_executable(
  UpdateConfigDummyTest
  UpdateConfigDummyTest.cpp
  test_obs_module.cpp
  "${PLUGIN_SOURCE_DIR}/UpdateConfig/UpdateConfigCommon.cpp"
  "${PLUGIN_SOURCE_DIR}/UpdateConfig/UpdateConfigDummy.cpp"
)
configure_update_config_test(UpdateConfigDummyTest)

if(CURL_FOUND)
  add_executable(
    UpdateConfigCurlTest
    UpdateConfigCurlTest.cpp
    test_helpers_linux.cpp
    test_obs_module.cpp
    "${PLUGIN_SOURCE_DIR}/UpdateConfig/UpdateConfigCommon.cpp"
    "${PLUGIN_SOURCE_DIR}/UpdateConfig/UpdateConfigCurl.cpp"
  )
  configure_update_config_test(UpdateConfigCurlTest)
  target_link_libraries(UpdateConfigCurlTest PRIVATE CURL::libcurl)
  if(APPLE)
    target_link_libraries(UpdateConfigCurlTest PRIVATE jthread::jthread)
  endif()
endif()
