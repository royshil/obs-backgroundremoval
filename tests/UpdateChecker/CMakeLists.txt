# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

function(configure_update_checker_test TARGET_NAME)
    target_include_directories("${TARGET_NAME}" PRIVATE "${PLUGIN_SOURCE_DIR}/UpdateChecker")

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

add_executable(UpdateCheckerSemverTest UpdateCheckerSemverTest.cpp "${PLUGIN_SOURCE_DIR}/UpdateChecker/UpdateCheckerSemver.cpp")
configure_update_checker_test(UpdateCheckerSemverTest)
target_use_plugin_options(UpdateCheckerSemverTest)
