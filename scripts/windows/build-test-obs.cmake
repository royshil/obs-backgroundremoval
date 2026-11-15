# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28)

if(NOT WIN32)
  message(FATAL_ERROR "The CTest OBS runtime is Windows-only")
endif()
get_filename_component(PROJECT_ROOT "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(OBS_SOURCE "${PROJECT_ROOT}/vendor/obs-studio")
if(NOT OBS_TEST_BUILD_DIR)
  set(OBS_TEST_BUILD_DIR "${PROJECT_ROOT}/build/obs-test-runtime")
endif()
set(DEPS_DIR "${OBS_TEST_BUILD_DIR}/deps")
file(READ "${OBS_SOURCE}/buildspec.json" OBS_BUILDSPEC)
string(JSON DEPS_VERSION GET "${OBS_BUILDSPEC}" dependencies prebuilt version)
string(JSON DEPS_URL GET "${OBS_BUILDSPEC}" dependencies prebuilt baseUrl)
string(JSON DEPS_HASH GET "${OBS_BUILDSPEC}" dependencies prebuilt hashes windows-x64)
file(MAKE_DIRECTORY "${DEPS_DIR}")
file(
  DOWNLOAD "${DEPS_URL}/${DEPS_VERSION}/windows-deps-${DEPS_VERSION}-x64.zip"
  "${OBS_TEST_BUILD_DIR}/deps.zip"
  EXPECTED_HASH "SHA256=${DEPS_HASH}"
  TLS_VERIFY ON
)
file(ARCHIVE_EXTRACT INPUT "${OBS_TEST_BUILD_DIR}/deps.zip" DESTINATION "${DEPS_DIR}")
file(STRINGS "${PROJECT_ROOT}/buildspec.props" SDK_LINE REGEX "^windows_sdk_version=")
string(REPLACE "windows_sdk_version=" "" SDK_VERSION "${SDK_LINE}")
execute_process(
  COMMAND
    "${CMAKE_COMMAND}" -S "${OBS_SOURCE}" -B "${OBS_TEST_BUILD_DIR}" -G "Visual Studio 18 2026" -A x64
    "-DCMAKE_PREFIX_PATH=${DEPS_DIR}" "-DCMAKE_SYSTEM_VERSION=${SDK_VERSION}" -DOBS_VERSION_OVERRIDE=29.1.3
    "-DZLIB_LIBRARY_RELEASE=${DEPS_DIR}/lib/zlib.lib"
    "-DCMAKE_PROJECT_obs-studio_INCLUDE=${CMAKE_CURRENT_LIST_DIR}/obs-test-compiler.cmake"
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DENABLE_UI=OFF -DENABLE_PLUGINS=OFF -DENABLE_SCRIPTING=OFF
  COMMAND_ERROR_IS_FATAL ANY
)
execute_process(
  COMMAND
    "${CMAKE_COMMAND}" --build "${OBS_TEST_BUILD_DIR}" --config RelWithDebInfo --parallel 4 --target libobs libobs-d3d11
  COMMAND_ERROR_IS_FATAL ANY
)
set(RUNTIME "${OBS_TEST_BUILD_DIR}/rundir/RelWithDebInfo")
file(GLOB DEPENDENCY_DLLS "${DEPS_DIR}/bin/*.dll")
file(COPY ${DEPENDENCY_DLLS} DESTINATION "${RUNTIME}/bin/64bit")
file(COPY "${OBS_SOURCE}/libobs/data/" DESTINATION "${RUNTIME}/data/libobs")
foreach(BINARY obs.dll libobs-d3d11.dll)
  if(NOT EXISTS "${RUNTIME}/bin/64bit/${BINARY}")
    message(FATAL_ERROR "OBS runtime build did not produce ${BINARY}")
  endif()
endforeach()
message(STATUS "CTest OBS runtime: ${RUNTIME}/bin/64bit")
