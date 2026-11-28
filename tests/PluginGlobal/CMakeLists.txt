# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# Unit test cases of src/PluginGlobal, docs/specs/SP-01021. SemVer runs on its own, so the test compiles only
# src/PluginGlobal/SemVer.cpp and depends on neither the plugin libraries, libobs nor Qt. Each CTest test runs one
# scenario by its tag.

add_executable(PluginGlobalUnitTest UnitTestMain.cpp SemVerTest.cpp "${PLUGIN_SOURCE_DIR}/PluginGlobal/SemVer.cpp")
target_include_directories(PluginGlobalUnitTest PRIVATE "${PLUGIN_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/tests" "${PROJECT_SOURCE_DIR}/tests/vendor/doctest")
target_compile_features(PluginGlobalUnitTest PRIVATE cxx_std_20)
# The forward declarations of std types in doctest do not match the MSVC STL of Visual Studio 2026.
target_compile_definitions(PluginGlobalUnitTest PRIVATE DOCTEST_CONFIG_USE_STD_HEADERS)
foreach(
    SPEC_CASE
    IN
    ITEMS SP-01021.1 SP-01021.2 SP-01021.3 SP-01021.4 SP-01021.5 SP-01021.6 SP-01021.7 SP-01021.8 SP-01021.9 SP-01021.10
)
    add_test(NAME "${SPEC_CASE}" COMMAND PluginGlobalUnitTest "--test-case=${SPEC_CASE} *")
endforeach()
