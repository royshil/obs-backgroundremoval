# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

function(configure_update_config_test TARGET_NAME)
    target_include_directories("${TARGET_NAME}" PRIVATE "${PLUGIN_SOURCE_DIR}/UpdateConfig")

    if(APPLE)
        # libobs, which the plugin libraries link, resolves from OBS Studio as in tests/CMakeLists.txt.
        set(TEST_BUILD_RPATHS ${OBS_MACOS_FRAMEWORKS_DIR})
        foreach(PREFIX IN LISTS CMAKE_PREFIX_PATH)
            if(IS_DIRECTORY "${PREFIX}/lib")
                list(APPEND TEST_BUILD_RPATHS "${PREFIX}/lib")
            endif()
        endforeach()
        set_target_properties("${TARGET_NAME}" PROPERTIES BUILD_RPATH "${TEST_BUILD_RPATHS}")
    endif()

    add_test(NAME "${TARGET_NAME}" COMMAND "${TARGET_NAME}")
endfunction()

add_executable(UpdateConfigSemverTest UpdateConfigSemverTest.cpp "${PLUGIN_SOURCE_DIR}/UpdateConfig/UpdateConfigSemver.cpp")
configure_update_config_test(UpdateConfigSemverTest)
target_use_plugin_options(UpdateConfigSemverTest)

add_executable(UpdateConfigCurlTest UpdateConfigCurlTest.cpp test_obs_module.cpp)

if(WIN32)
    target_sources(UpdateConfigCurlTest PRIVATE test_helpers_windows.cpp)
    target_link_libraries(UpdateConfigCurlTest PRIVATE ws2_32)
else()
    target_sources(UpdateConfigCurlTest PRIVATE test_helpers_linux.cpp)
endif()

configure_update_config_test(UpdateConfigCurlTest)
target_link_libraries(UpdateConfigCurlTest PRIVATE obs-backgroundremoval_pluginlibs)

if(WIN32)
    set_tests_properties(UpdateConfigCurlTest PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${OBS_TEST_BIN_DIR}")
endif()
