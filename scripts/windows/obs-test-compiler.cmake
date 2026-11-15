# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

# MSVC 19.51 miscompiles the pinned OBS matrix4_mul union/SIMD accesses with /O2.
# Keep the workaround local to that source file in the CTest runtime.
if(MSVC AND MSVC_VERSION EQUAL 1951)
  cmake_language(
    DEFER
    CALL set_source_files_properties
    "${CMAKE_SOURCE_DIR}/libobs/graphics/matrix4.c"
    DIRECTORY
    "${CMAKE_SOURCE_DIR}/libobs"
    PROPERTIES
    COMPILE_OPTIONS
    /Od
  )
endif()
