# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(
  "${PROJECT_SOURCE_DIR}/vendor/google/benchmark"
  "${CMAKE_CURRENT_BINARY_DIR}/google-benchmark"
  EXCLUDE_FROM_ALL
)
set(LANDSCAPE_GENERATED "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/generated")

add_executable(
  MPLD10Benchmark
  EXCLUDE_FROM_ALL
  MPLD10Benchmark.cpp
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlCompiler.cpp"
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlProgramRuntime.cpp"
  "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/TraceLoggingProvider.cpp"
  "${LANDSCAPE_GENERATED}/MediaPipeLandscapeDmlProgram.generated.cpp"
)
target_include_directories(MPLD10Benchmark PRIVATE "${PLUGIN_SOURCE_DIR}")
target_link_libraries(
  MPLD10Benchmark
  PRIVATE benchmark::benchmark d3d12 d3dcompiler directml advapi32
)

add_executable(
  MPLD10CameraBenchmark
  EXCLUDE_FROM_ALL
  MPLD10CameraBenchmark.cpp
  "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/MPLD10Pipeline.cpp"
  "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/MediaPipeLandscapeDirectML10/MPLD10Engine.cpp"
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlCompiler.cpp"
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlProgramRuntime.cpp"
  "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/TraceLoggingProvider.cpp"
  "${LANDSCAPE_GENERATED}/MediaPipeLandscapeDmlProgram.generated.cpp"
)
set(
  OBS_PROFILE_BIN_DIR
  "$ENV{ProgramFiles}/obs-studio/bin/64bit"
  CACHE PATH
  "OBS installation with win-dshow for manual camera profiling (not CTest)"
)
target_include_directories(MPLD10CameraBenchmark PRIVATE "${PLUGIN_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_definitions(
  MPLD10CameraBenchmark
  PRIVATE OBS_TEST_BIN_DIR="${OBS_PROFILE_BIN_DIR}" OBS_LOG_HEADER="[CameraBenchmark] "
)
target_link_libraries(
  MPLD10CameraBenchmark
  PRIVATE benchmark::benchmark CTest::libobs d3d11 d3d12 dxgi d3dcompiler directml advapi32 ole32 gdi32
)
