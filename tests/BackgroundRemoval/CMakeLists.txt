# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# Plugin unit test cases, docs/specs/SP-01xxx. Each CTest test runs one scenario by its tag.

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

# libobs and its D3D11 backend from the OBS Studio release fetched in tests/CMakeLists.txt, linked
# through the vendored import library.
if(WIN32)
  add_executable(
    BackgroundRemovalGpuTest
    MaskRefinementGpuTest.cpp
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/MaskCompositor/MaskCompositor.cpp"
  )
  target_include_directories(
    BackgroundRemovalGpuTest
    PRIVATE "${PLUGIN_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/tests" "${PROJECT_SOURCE_DIR}/tests/vendor/doctest"
  )
  target_compile_definitions(
    BackgroundRemovalGpuTest
    PRIVATE OBS_TEST_BIN_DIR="${OBS_TEST_BIN_DIR}" TEST_DATA_DIR="${PROJECT_SOURCE_DIR}/tests/data"
  )
  target_link_libraries(BackgroundRemovalGpuTest PRIVATE BuildOptions OBS::libobs)
  target_compile_features(BackgroundRemovalGpuTest PRIVATE cxx_std_20)
  add_spec_tests(BackgroundRemovalGpuTest SP-01004.1 SP-01004.2 SP-01004.4)
  foreach(SPEC_CASE IN ITEMS SP-01004.1 SP-01004.2 SP-01004.4)
    set_tests_properties(
      "${SPEC_CASE}"
      PROPERTIES TIMEOUT 120 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${OBS_TEST_BIN_DIR}"
    )
  endforeach()
endif()

# MARK: The filter running in libobs

# libobs started headless from the OBS Studio release, with its image source and filters and the
# built plugin loaded.
if(WIN32)
  # The shader pipeline is also built in, so that SP-01005 can run it on its own.
  add_executable(
    BackgroundRemovalObsTest
    ObsTestMain.cpp
    ObsHarness.cpp
    FilterSizeObsTest.cpp
    FilterOutputObsTest.cpp
    InferenceObsTest.cpp
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
      OBS_TEST_ROOT="${OBS_TEST_ROOT}"
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
    SP-01015.2
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
  set_tests_properties(
    ${OBS_SPEC_CASES}
    PROPERTIES
      TIMEOUT 300
      WORKING_DIRECTORY "${OBS_TEST_BIN_DIR}"
      ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${OBS_TEST_BIN_DIR}"
  )
endif()
