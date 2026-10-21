# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(
  DmlProgramTest
  DmlProgramTest.cpp
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlCompiler.cpp"
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlProgramRuntime.cpp"
  "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/TraceLoggingProvider.cpp"
)
target_include_directories(DmlProgramTest PRIVATE "${PLUGIN_SOURCE_DIR}")
target_link_libraries(DmlProgramTest PRIVATE d3d12 d3dcompiler directml advapi32)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/DmlProgramTest.cpp")
discover_test_cases(DmlProgramTest.cpp DML_TEST_CASES)
foreach(TEST_CASE IN LISTS DML_TEST_CASES)
  add_test(NAME "DmlProgramTest.${TEST_CASE}" COMMAND DmlProgramTest "${TEST_CASE}")
  set_tests_properties("DmlProgramTest.${TEST_CASE}" PROPERTIES TIMEOUT 60)
endforeach()

set(LANDSCAPE_GENERATED "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/generated")

add_executable(
  LandscapeModelCheck
  EXCLUDE_FROM_ALL
  LandscapeModelCheck.cpp
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlCompiler.cpp"
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlProgramRuntime.cpp"
  "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/TraceLoggingProvider.cpp"
  "${LANDSCAPE_GENERATED}/MediaPipeLandscapeDmlProgram.generated.cpp"
)
target_include_directories(LandscapeModelCheck PRIVATE "${PLUGIN_SOURCE_DIR}")
target_link_libraries(LandscapeModelCheck PRIVATE d3d12 d3dcompiler directml advapi32)

add_executable(
  OperatorFixtureTest
  OperatorFixtureTest.cpp
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlCompiler.cpp"
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlProgramRuntime.cpp"
  "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/TraceLoggingProvider.cpp"
)
target_include_directories(OperatorFixtureTest PRIVATE "${PLUGIN_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/vendor/doctest")
target_compile_definitions(
  OperatorFixtureTest
  PRIVATE OPERATOR_FIXTURES="${PROJECT_SOURCE_DIR}/tests/fixtures/operators"
)
target_link_libraries(
  OperatorFixtureTest
  PRIVATE d3d12 d3dcompiler directml windowsapp advapi32
)
foreach(
  OPERATOR
  MPLD10Float32Postprocess
  MPLD10DepthwiseConv2dRelu
  MPLD10DepthwiseConv2dReluContract
  HardSwish
  HardSwishFused
  ReduceMean
  Resize
  Add
  Mul
  Relu
  Sigmoid
  ClipFloat32
  ClipFloat32Contract
  Conv
  DepthwiseConv
  ConvTranspose
  ConvRelu
  DepthwiseConvRelu
  PointwiseConvTranspose
)
  add_test(NAME "OperatorFixtureTest.${OPERATOR}" COMMAND OperatorFixtureTest "--test-case=${OPERATOR}" --no-skip)
  set_tests_properties("OperatorFixtureTest.${OPERATOR}" PROPERTIES TIMEOUT 120)
endforeach()

# Built from the pinned OBS submodule by scripts/windows/build-test-obs.cmake.
set(
  OBS_TEST_BIN_DIR
  "${PROJECT_BINARY_DIR}/obs-test-runtime/rundir/RelWithDebInfo/bin/64bit"
  CACHE PATH
  "Source-built OBS runtime for GPU input integration tests"
)

set(
  OBS_TEST_LIBRARY
  "${PROJECT_BINARY_DIR}/obs-test-runtime/libobs/RelWithDebInfo/obs.lib"
  CACHE FILEPATH
  "Source-built libobs import library for CTest"
)
if(NOT EXISTS "${OBS_TEST_LIBRARY}")
  message(
    FATAL_ERROR
    "Required libobs test library not found: ${OBS_TEST_LIBRARY}; run scripts/windows/build-test-obs.cmake"
  )
endif()
add_library(CTest::libobs SHARED IMPORTED GLOBAL)
set_target_properties(
  CTest::libobs
  PROPERTIES
    IMPORTED_CONFIGURATIONS RELEASE
    IMPORTED_IMPLIB_RELEASE "${OBS_TEST_LIBRARY}"
    IMPORTED_LOCATION_RELEASE "${OBS_TEST_BIN_DIR}/obs.dll"
    INTERFACE_INCLUDE_DIRECTORIES "${PROJECT_SOURCE_DIR}/vendor/obs-studio/libobs"
)

foreach(obs_binary obs.dll libobs-d3d11.dll)
  if(NOT EXISTS "${OBS_TEST_BIN_DIR}/${obs_binary}")
    message(FATAL_ERROR "Required OBS test binary not found: ${OBS_TEST_BIN_DIR}/${obs_binary}")
  endif()
endforeach()
add_executable(
  GpuImageInferenceTest
  GpuImageInferenceTest.cpp
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlCompiler.cpp"
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlProgramRuntime.cpp"
  "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/TraceLoggingProvider.cpp"
)
target_include_directories(GpuImageInferenceTest PRIVATE "${PLUGIN_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/vendor/doctest")
target_compile_definitions(GpuImageInferenceTest PRIVATE OBS_TEST_BIN_DIR="${OBS_TEST_BIN_DIR}")
target_link_libraries(
  GpuImageInferenceTest
  PRIVATE CTest::libobs d3d11 d3d12 dxgi d3dcompiler directml advapi32
)
add_executable(
  MPLD10EngineTest
  MPLD10EngineTest.cpp
  "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/MediaPipeLandscapeDirectML10/MPLD10Engine.cpp"
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlCompiler.cpp"
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlProgramRuntime.cpp"
  "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/TraceLoggingProvider.cpp"
  "${LANDSCAPE_GENERATED}/MediaPipeLandscapeDmlProgram.generated.cpp"
)
target_include_directories(MPLD10EngineTest PRIVATE "${PLUGIN_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/vendor/doctest")
target_compile_definitions(
  MPLD10EngineTest
  PRIVATE
    OBS_TEST_BIN_DIR="${OBS_TEST_BIN_DIR}"
    LANDSCAPE_PORTRAIT="${PROJECT_SOURCE_DIR}/tests/fixtures/0000-portrait-umireon.jpg"
)
target_link_libraries(
  MPLD10EngineTest
  PRIVATE CTest::libobs d3d11 d3d12 dxgi d3dcompiler directml advapi32 windowsapp windowscodecs ole32
)
add_test(NAME MPLD10EngineTest.LandscapeEngineBgraInput COMMAND MPLD10EngineTest)
set_tests_properties(
  MPLD10EngineTest.LandscapeEngineBgraInput
  PROPERTIES TIMEOUT 120 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${OBS_TEST_BIN_DIR}"
)
target_sources(
  GpuImageInferenceTest
  PRIVATE
    LandscapeMaskEffectTest.cpp
    PortraitFixtureTest.cpp
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/PipelineFactory.cpp"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/MPLD10Pipeline.cpp"
    "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/MediaPipeLandscapeDirectML10/MPLD10Engine.cpp"
    "${LANDSCAPE_GENERATED}/MediaPipeLandscapeDmlProgram.generated.cpp"
)
target_compile_definitions(
  GpuImageInferenceTest
  PRIVATE
    GPU_LANDSCAPE_MODEL
    OBS_LOG_HEADER="[GPU-Test] "
    LANDSCAPE_PORTRAIT="${PROJECT_SOURCE_DIR}/tests/fixtures/0000-portrait-umireon.jpg"
    LANDSCAPE_CUTOUT="${PROJECT_SOURCE_DIR}/tests/fixtures/0000-portrait-umireon-cutout.png"
)
target_link_libraries(GpuImageInferenceTest PRIVATE windowsapp windowscodecs ole32)
add_test(NAME GpuImageInferenceTest.PortraitFixture COMMAND GpuImageInferenceTest --test-case=PortraitFixture)
set_tests_properties(
  GpuImageInferenceTest.PortraitFixture
  PROPERTIES TIMEOUT 120 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${OBS_TEST_BIN_DIR}"
)
add_test(NAME GpuImageInferenceTest.LandscapeMaskEffect COMMAND GpuImageInferenceTest --test-case=LandscapeMaskEffect)
set_tests_properties(
  GpuImageInferenceTest.LandscapeMaskEffect
  PROPERTIES TIMEOUT 120 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${OBS_TEST_BIN_DIR}"
)
add_test(
  NAME GpuImageInferenceTest.LandscapeRenderingInitialization
  COMMAND GpuImageInferenceTest --test-case=LandscapeRenderingInitialization
)
set_tests_properties(
  GpuImageInferenceTest.LandscapeRenderingInitialization
  PROPERTIES TIMEOUT 120 ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${OBS_TEST_BIN_DIR}"
)


add_executable(MotionMeasurementTest MotionMeasurementTest.cpp)
target_include_directories(
  MotionMeasurementTest
  PRIVATE "${PLUGIN_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/vendor/doctest"
)
add_test(NAME MotionMeasurementTest COMMAND MotionMeasurementTest)
set_tests_properties(MotionMeasurementTest PROPERTIES TIMEOUT 30)

if(MPLD10_GPU_DIAGNOSTICS)
  foreach(timing_target MPLD10EngineTest GpuImageInferenceTest)
    target_compile_definitions(${timing_target} PRIVATE MPLD10_GPU_DIAGNOSTICS)
  endforeach()
endif()
