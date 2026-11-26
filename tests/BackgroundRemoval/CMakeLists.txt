# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# Plugin unit test cases, docs/specs/SP-01xxx, and plugin performance test cases, SP-03xxx. Each
# CTest test runs one scenario by its tag.

function(add_spec_tests TARGET_NAME)
    foreach(SPEC_CASE IN LISTS ARGN)
        add_test(NAME "${SPEC_CASE}" COMMAND "${TARGET_NAME}" "--test-case=${SPEC_CASE} *")
    endforeach()
endfunction()

# MARK: Without GPU

add_executable(BackgroundRemovalUnitTest UnitTestMain.cpp DualFilterKernelTest.cpp BlurVarianceTest.cpp)
target_include_directories(BackgroundRemovalUnitTest PRIVATE "${PLUGIN_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/tests" "${PROJECT_SOURCE_DIR}/tests/vendor/doctest")
target_use_plugin_options(BackgroundRemovalUnitTest)
target_compile_features(BackgroundRemovalUnitTest PRIVATE cxx_std_20)
add_spec_tests(
    BackgroundRemovalUnitTest
    SP-01002.1
    SP-01002.2
    SP-01002.3
    SP-01003.1
    SP-01003.2
    SP-01003.3
    SP-01003.4
)

# MARK: On the GPU through libobs

# On Windows, libobs and its D3D11 backend from the OBS Studio release fetched in
# tests/CMakeLists.txt, linked through the vendored import library. On Linux, the system libobs and
# its OpenGL backend, rendering on the X display in DISPLAY. On macOS, libobs and its OpenGL backend
# from the installed OBS Studio found in tests/CMakeLists.txt.
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux" OR OBS_MACOS_FRAMEWORKS_DIR)
    add_executable(BackgroundRemovalGpuTest MaskRefinementGpuTest.cpp)
    target_include_directories(
        BackgroundRemovalGpuTest
        PRIVATE "${PLUGIN_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/tests" "${PROJECT_SOURCE_DIR}/tests/vendor/doctest"
    )
    target_compile_definitions(BackgroundRemovalGpuTest PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")
    target_link_libraries(BackgroundRemovalGpuTest PRIVATE obs-backgroundremoval_pluginlibs)
    target_compile_features(BackgroundRemovalGpuTest PRIVATE cxx_std_20)
    add_spec_tests(BackgroundRemovalGpuTest SP-01004.1 SP-01004.2 SP-01004.4 SP-01004.5)
    set_tests_properties(SP-01004.1 SP-01004.2 SP-01004.4 SP-01004.5 PROPERTIES TIMEOUT 120)
    if(WIN32)
        target_compile_definitions(BackgroundRemovalGpuTest PRIVATE OBS_TEST_BIN_DIR="${OBS_TEST_BIN_DIR}")
        set_tests_properties(SP-01004.1 SP-01004.2 SP-01004.4 SP-01004.5 PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${OBS_TEST_BIN_DIR}")
    elseif(APPLE)
        target_compile_definitions(BackgroundRemovalGpuTest PRIVATE OBS_MACOS_FRAMEWORKS_DIR="${OBS_MACOS_FRAMEWORKS_DIR}")
        set_target_properties(BackgroundRemovalGpuTest PROPERTIES BUILD_RPATH "${OBS_MACOS_FRAMEWORKS_DIR}")
    endif()
endif()

# MARK: The filter running in libobs

# libobs started headless, with its image source and filters and the built plugin loaded: from the
# OBS Studio release on Windows, from the installed OBS Studio on macOS, and from the system on
# Linux, rendering on the X display in DISPLAY. OBS Studio 33 moved its own modules on Linux from
# obs-plugins to obs-modules/core. On macOS each module is an application-bundle plugin, so both the
# modules and their data are looked for under Contents/PlugIns; see ObsHarness.cpp.
if(WIN32)
    set(OBS_TEST_MODULE_DIR "${OBS_TEST_ROOT}/obs-plugins/64bit")
    set(OBS_TEST_MODULE_DATA_DIR "${OBS_TEST_ROOT}/data/obs-plugins")
elseif(APPLE)
    set(OBS_TEST_MODULE_DIR "${OBS_MACOS_PLUGINS_DIR}")
    set(OBS_TEST_MODULE_DATA_DIR "${OBS_MACOS_PLUGINS_DIR}")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # Not cached, so that the modules are found again after OBS Studio is upgraded. The data is looked
    # for only in the prefix libobs is installed in, as lib or lib/<multiarch>, and in the layout the
    # modules were found in, so that both come from the same OBS Studio.
    get_target_property(LIBOBS_FILE OBS::libobs LOCATION)
    cmake_path(GET LIBOBS_FILE PARENT_PATH LIBOBS_DIR)
    unset(OBS_TEST_MODULE_DIR CACHE)
    unset(OBS_TEST_MODULE_DATA_DIR CACHE)
    find_path(OBS_TEST_MODULE_DIR NAMES image-source.so PATHS "${LIBOBS_DIR}/obs-modules/core" "${LIBOBS_DIR}/obs-plugins" NO_DEFAULT_PATH NO_CACHE)
    if(OBS_TEST_MODULE_DIR)
        if(OBS_TEST_MODULE_DIR MATCHES "/obs-modules/core/?$")
            set(OBS_TEST_MODULE_DATA_SUFFIX share/obs/obs-modules/core)
        else()
            set(OBS_TEST_MODULE_DATA_SUFFIX share/obs/obs-plugins)
        endif()
        find_path(
            OBS_TEST_MODULE_DATA_DIR
            NAMES image-source
            PATHS "${LIBOBS_DIR}/.." "${LIBOBS_DIR}/../.."
            PATH_SUFFIXES ${OBS_TEST_MODULE_DATA_SUFFIX}
            NO_DEFAULT_PATH
            NO_CACHE
        )
    endif()
    if(NOT OBS_TEST_MODULE_DIR OR NOT OBS_TEST_MODULE_DATA_DIR)
        message(
            WARNING
            "The modules of OBS Studio or their data were not found next to ${LIBOBS_FILE}, so the tests running the filter in libobs are not built."
        )
    endif()
endif()

if(OBS_TEST_MODULE_DIR AND OBS_TEST_MODULE_DATA_DIR)
    # The shader pipeline is also linked in from the plugin core, so that SP-01005 can run it on its own.
    add_executable(
        BackgroundRemovalObsTest
        ObsTestMain.cpp
        ObsHarness.cpp
        FilterSizeObsTest.cpp
        FilterOutputObsTest.cpp
        InferenceObsTest.cpp
        PerformanceObsTest.cpp
    )
    target_include_directories(
        BackgroundRemovalObsTest
        PRIVATE "${PLUGIN_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/tests" "${PROJECT_SOURCE_DIR}/tests/vendor/doctest"
    )
    target_compile_definitions(
        BackgroundRemovalObsTest
        PRIVATE
            OBS_TEST_MODULE_DIR="${OBS_TEST_MODULE_DIR}"
            OBS_TEST_MODULE_DATA_DIR="${OBS_TEST_MODULE_DATA_DIR}"
            OBS_TEST_MODULE_SUFFIX="${CMAKE_SHARED_MODULE_SUFFIX}"
            PLUGIN_FILE="$<TARGET_FILE:obs-backgroundremoval>"
            PLUGIN_DATA_DIR="${PROJECT_SOURCE_DIR}/data"
            TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data"
    )
    if(APPLE)
        target_compile_definitions(BackgroundRemovalObsTest PRIVATE OBS_MACOS_FRAMEWORKS_DIR="${OBS_MACOS_FRAMEWORKS_DIR}")
        set_target_properties(BackgroundRemovalObsTest PROPERTIES BUILD_RPATH "${OBS_MACOS_FRAMEWORKS_DIR}")
    endif()
    target_link_libraries(BackgroundRemovalObsTest PRIVATE obs-backgroundremoval_pluginlibs)
    target_compile_features(BackgroundRemovalObsTest PRIVATE cxx_std_20)
    add_dependencies(BackgroundRemovalObsTest obs-backgroundremoval)

    set(OBS_SPEC_CASES
        SP-01005.2
        SP-01006.1
        SP-01007.1
        SP-01008.1
        SP-01009.1
        SP-01010.1
        SP-01011.1
        SP-01012.1
        SP-01012.2
        SP-01012.3
        SP-01013.1
        SP-01013.2
        SP-01014.1
        SP-01014.2
        SP-01015.1
        SP-01016.1
        SP-01016.2
        SP-01016.3
        SP-01016.4
        SP-01017.1
        SP-01017.2
        SP-01017.3
        SP-01017.4
        SP-01018.1
        SP-01020.1
        SP-01020.2
        SP-01020.3
    )
    add_spec_tests(BackgroundRemovalObsTest ${OBS_SPEC_CASES})
    set_tests_properties(${OBS_SPEC_CASES} PROPERTIES TIMEOUT 300)

    # Plugin performance test cases, docs/specs/SP-03xxx. They need a hardware GPU, so they are
    # labeled performance, for CI to leave out.
    set(OBS_PERFORMANCE_CASES SP-03001.1 SP-03001.2)
    add_spec_tests(BackgroundRemovalObsTest ${OBS_PERFORMANCE_CASES})
    set_tests_properties(${OBS_PERFORMANCE_CASES} PROPERTIES TIMEOUT 300 LABELS performance)

    if(WIN32)
        target_compile_definitions(BackgroundRemovalObsTest PRIVATE OBS_TEST_ROOT="${OBS_TEST_ROOT}")
        set_tests_properties(
            ${OBS_SPEC_CASES}
            ${OBS_PERFORMANCE_CASES}
            PROPERTIES WORKING_DIRECTORY "${OBS_TEST_BIN_DIR}" ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${OBS_TEST_BIN_DIR}"
        )
    endif()
endif()
