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

add_executable(
  BackgroundRemovalUnitTest
  UnitTestMain.cpp
  OutputSizeTest.cpp
  DualFilterKernelTest.cpp
  BlurVarianceTest.cpp
  MaskThresholdTest.cpp
)
target_include_directories(
  BackgroundRemovalUnitTest
  PRIVATE "${PLUGIN_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/tests" "${PROJECT_SOURCE_DIR}/tests/vendor/doctest"
)
target_link_libraries(BackgroundRemovalUnitTest PRIVATE BuildOptions)
target_compile_features(BackgroundRemovalUnitTest PRIVATE cxx_std_20)
add_spec_tests(
  BackgroundRemovalUnitTest
  SP-01001.1
  SP-01001.2
  SP-01002.1
  SP-01002.2
  SP-01002.3
  SP-01003.1
  SP-01003.2
  SP-01003.3
  SP-01003.4
  SP-01004.3
)

# MARK: On the GPU through libobs

# On Windows, libobs and its D3D11 backend from the OBS Studio release fetched in
# tests/CMakeLists.txt, linked through the vendored import library. On Linux, the system libobs and
# its OpenGL backend, rendering on the X display in DISPLAY.
if(WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(
    BackgroundRemovalGpuTest
    MaskRefinementGpuTest.cpp
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/MaskCompositor/MaskCompositor.cpp"
  )
  target_include_directories(
    BackgroundRemovalGpuTest
    PRIVATE "${PLUGIN_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/tests" "${PROJECT_SOURCE_DIR}/tests/vendor/doctest"
  )
  target_compile_definitions(BackgroundRemovalGpuTest PRIVATE TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data")
  target_link_libraries(BackgroundRemovalGpuTest PRIVATE BuildOptions OBS::libobs)
  target_compile_features(BackgroundRemovalGpuTest PRIVATE cxx_std_20)
  add_spec_tests(BackgroundRemovalGpuTest SP-01004.1 SP-01004.2 SP-01004.4)
  set_tests_properties(SP-01004.1 SP-01004.2 SP-01004.4 PROPERTIES TIMEOUT 120)
  if(WIN32)
    target_compile_definitions(BackgroundRemovalGpuTest PRIVATE OBS_TEST_BIN_DIR="${OBS_TEST_BIN_DIR}")
    set_tests_properties(
      SP-01004.1
      SP-01004.2
      SP-01004.4
      PROPERTIES ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${OBS_TEST_BIN_DIR}"
    )
  endif()
endif()

# MARK: The filter running in libobs

# libobs started headless, with its image source and filters and the built plugin loaded: from the
# OBS Studio release on Windows, and from the system on Linux, rendering on the X display in
# DISPLAY. OBS Studio 33 moved its own modules on Linux from obs-plugins to obs-modules/core.
if(WIN32)
  set(OBS_TEST_MODULE_DIR "${OBS_TEST_ROOT}/obs-plugins/64bit")
  set(OBS_TEST_MODULE_DATA_DIR "${OBS_TEST_ROOT}/data/obs-plugins")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  get_target_property(LIBOBS_FILE OBS::libobs LOCATION)
  cmake_path(GET LIBOBS_FILE PARENT_PATH LIBOBS_DIR)
  find_path(
    OBS_TEST_MODULE_DIR
    NAMES image-source.so
    PATHS "${LIBOBS_DIR}/obs-modules/core" "${LIBOBS_DIR}/obs-plugins"
    NO_DEFAULT_PATH
  )
  set(OBS_MODULE_DATA_CANDIDATES)
  foreach(PREFIX IN LISTS CMAKE_SYSTEM_PREFIX_PATH)
    list(APPEND OBS_MODULE_DATA_CANDIDATES "${PREFIX}/share/obs/obs-modules/core" "${PREFIX}/share/obs/obs-plugins")
  endforeach()
  find_path(OBS_TEST_MODULE_DATA_DIR NAMES image-source PATHS ${OBS_MODULE_DATA_CANDIDATES} NO_DEFAULT_PATH)
  if(NOT OBS_TEST_MODULE_DIR OR NOT OBS_TEST_MODULE_DATA_DIR)
    message(WARNING "OBS Studio is not installed, so the tests running the filter in libobs are not built.")
  endif()
endif()

if(OBS_TEST_MODULE_DIR AND OBS_TEST_MODULE_DATA_DIR)
  # The shader pipeline is also built in, so that SP-01005 can run it on its own.
  add_executable(
    BackgroundRemovalObsTest
    ObsTestMain.cpp
    ObsHarness.cpp
    FilterSizeObsTest.cpp
    FilterOutputObsTest.cpp
    InferenceObsTest.cpp
    PerformanceObsTest.cpp
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/MaskCompositor/MaskCompositor.cpp"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/MediaPipeLandscapeShader/MPLSEffect.cpp"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/MediaPipeLandscapeShader/MPLSPipeline.cpp"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/MediaPipeLandscapeShader/generated/MPLSPipeline.generated.cpp"
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
      PLUGIN_VERSION_STR="${PLUGIN_VERSION}"
      TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data"
  )
  target_link_libraries(BackgroundRemovalObsTest PRIVATE BuildOptions OBS::libobs)
  target_compile_features(BackgroundRemovalObsTest PRIVATE cxx_std_20)
  add_dependencies(BackgroundRemovalObsTest obs-backgroundremoval)

  set(
    OBS_SPEC_CASES
    SP-01005.2
    SP-01006.1
    SP-01006.2
    SP-01007.1
    SP-01008.1
    SP-01009.1
    SP-01010.1
    SP-01011.1
    SP-01012.1
    SP-01012.2
    SP-01012.3
    SP-01013.1
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
    SP-01018.1
    SP-01018.2
    SP-01019.1
    SP-01019.2
    SP-01019.3
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
      PROPERTIES
        WORKING_DIRECTORY "${OBS_TEST_BIN_DIR}"
        ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${OBS_TEST_BIN_DIR}"
    )
  endif()
endif()
