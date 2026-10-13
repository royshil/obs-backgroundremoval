# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(
  DmlProgramTest
  DmlProgramTest.cpp
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlCompiler.cpp"
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlProgramRuntime.cpp"
)
target_include_directories(DmlProgramTest PRIVATE "${PLUGIN_SOURCE_DIR}")
target_link_libraries(DmlProgramTest PRIVATE d3d12 directml)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/DmlProgramTest.cpp")
discover_test_cases(DmlProgramTest.cpp DML_TEST_CASES)
foreach(TEST_CASE IN LISTS DML_TEST_CASES)
  add_test(NAME "DmlProgramTest.${TEST_CASE}" COMMAND DmlProgramTest "${TEST_CASE}")
  set_tests_properties("DmlProgramTest.${TEST_CASE}" PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 60)
endforeach()

# The full-model reference is the original pinned ONNX, not a lowered fixture.
set(MEDIAPIPE_LANDSCAPE_ONNX
  "${PROJECT_SOURCE_DIR}/.cache/huggingface/hub/models--onnx-community--mediapipe_selfie_segmentation_landscape/snapshots/2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3/onnx/model.onnx"
  CACHE FILEPATH "Original pinned MediaPipe Landscape ONNX for WinML comparison")

add_executable(WinMlComparison WinMlComparison.cpp
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlCompiler.cpp"
  "${PLUGIN_SOURCE_DIR}/DmlProgram/BaselineDmlProgramRuntime.cpp")
target_include_directories(WinMlComparison PRIVATE "${PLUGIN_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/vendor/doctest")
target_link_libraries(WinMlComparison PRIVATE d3d12 directml windowsapp test_onnx_nanopb)

foreach(OPERATOR HardSwish ReduceMean Resize Add Mul Relu Sigmoid Clip Conv DepthwiseConv ConvTranspose)
  add_test(NAME "WinMlComparison.${OPERATOR}" COMMAND WinMlComparison "--test-case=${OPERATOR}" --no-skip)
  set_tests_properties("WinMlComparison.${OPERATOR}" PROPERTIES TIMEOUT 120)
endforeach()

if(EXISTS "${MEDIAPIPE_LANDSCAPE_ONNX}")
  file(SHA256 "${MEDIAPIPE_LANDSCAPE_ONNX}" LANDSCAPE_ONNX_SHA256)
  if(NOT LANDSCAPE_ONNX_SHA256 STREQUAL "7a0adcfdb1715d3b0ff0f61486d8a181c33f4a208343a5abfccbc6540e872d24")
    message(FATAL_ERROR "MEDIAPIPE_LANDSCAPE_ONNX must be the original pinned revision 2497d5bec26c626c7b3c4edc6e1fefc21b64f6c3")
  endif()
  set(LANDSCAPE_GENERATED "${PLUGIN_SOURCE_DIR}/BackgroundRemoval/windows/generated")
  target_sources(WinMlComparison PRIVATE
    "${LANDSCAPE_GENERATED}/MediaPipeLandscapeDmlProgram.generated.cpp"
    "${LANDSCAPE_GENERATED}/MediaPipeLandscapeDmlProgram.generated.rc")
  target_include_directories(WinMlComparison PRIVATE "${LANDSCAPE_GENERATED}")
  target_compile_definitions(WinMlComparison PRIVATE MEDIAPIPE_LANDSCAPE_ONNX="${MEDIAPIPE_LANDSCAPE_ONNX}")
  target_compile_definitions(WinMlComparison PRIVATE LANDSCAPE_PORTRAIT="${PROJECT_SOURCE_DIR}/tests/Scenarios/0000-portrait-umireon.jpg")
  target_link_libraries(WinMlComparison PRIVATE windowscodecs ole32)
  add_test(NAME WinMlComparison.MediaPipeLandscape COMMAND WinMlComparison --test-case=MediaPipeLandscape --no-skip)
  set_tests_properties(WinMlComparison.MediaPipeLandscape PROPERTIES TIMEOUT 600)
else()
  message(STATUS "MediaPipe Landscape comparison unavailable: set MEDIAPIPE_LANDSCAPE_ONNX to the pinned original model")
endif()
