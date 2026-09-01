# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.28)

set(LIBOBS_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../obs-studio/libobs")
set(JANSSON_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/../obs-studio/deps/jansson/src")

find_package(Threads REQUIRED)

add_library(
  libobs-stub-jansson OBJECT
  "${JANSSON_SOURCE_DIR}/dump.c"
  "${JANSSON_SOURCE_DIR}/error.c"
  "${JANSSON_SOURCE_DIR}/hashtable.c"
  "${JANSSON_SOURCE_DIR}/hashtable_seed.c"
  "${JANSSON_SOURCE_DIR}/load.c"
  "${JANSSON_SOURCE_DIR}/memory.c"
  "${JANSSON_SOURCE_DIR}/pack_unpack.c"
  "${JANSSON_SOURCE_DIR}/strbuffer.c"
  "${JANSSON_SOURCE_DIR}/strconv.c"
  "${JANSSON_SOURCE_DIR}/utf.c"
  "${JANSSON_SOURCE_DIR}/value.c"
)
target_compile_definitions(libobs-stub-jansson PRIVATE HAVE_CONFIG_H)
target_include_directories(
  libobs-stub-jansson
  PRIVATE "${CMAKE_CURRENT_LIST_DIR}" "${JANSSON_SOURCE_DIR}"
)
set_target_properties(
  libobs-stub-jansson
  PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO
)

add_library(
  libobs-stub STATIC
  libobs-stub.c
  "$<TARGET_OBJECTS:libobs-stub-jansson>"
  "${LIBOBS_SOURCE_DIR}/obs-data.c"
  "${LIBOBS_SOURCE_DIR}/util/base.c"
  "${LIBOBS_SOURCE_DIR}/util/bmem.c"
  "${LIBOBS_SOURCE_DIR}/util/cf-lexer.c"
  "${LIBOBS_SOURCE_DIR}/util/cf-parser.c"
  "${LIBOBS_SOURCE_DIR}/util/config-file.c"
  "${LIBOBS_SOURCE_DIR}/util/dstr.c"
  "${LIBOBS_SOURCE_DIR}/util/lexer.c"
  "${LIBOBS_SOURCE_DIR}/util/platform.c"
  "${LIBOBS_SOURCE_DIR}/util/utf8.c"
)

if(WIN32)
  target_sources(
    libobs-stub
    PRIVATE
      "${LIBOBS_SOURCE_DIR}/util/platform-windows.c"
      "${CMAKE_CURRENT_LIST_DIR}/../obs-studio/deps/w32-pthreads/pthread.c"
  )
  target_compile_definitions(libobs-stub PRIVATE PTW32_STATIC_LIB __CLEANUP_C)
  target_include_directories(
    libobs-stub
    PRIVATE "${CMAKE_CURRENT_LIST_DIR}/../obs-studio/deps/w32-pthreads"
  )
  target_link_libraries(libobs-stub PUBLIC Rpcrt4 winmm)
elseif(APPLE)
  target_sources(
    libobs-stub
    PRIVATE
      "${LIBOBS_SOURCE_DIR}/util/platform-cocoa.m"
      "${LIBOBS_SOURCE_DIR}/util/platform-nix.c"
  )
  target_link_libraries(
    libobs-stub
    PUBLIC
      "-framework CoreFoundation"
      "-framework Foundation"
      "-framework IOKit"
  )
elseif(UNIX)
  target_sources(
    libobs-stub
    PRIVATE
      "${LIBOBS_SOURCE_DIR}/util/platform-nix.c"
      "${LIBOBS_SOURCE_DIR}/util/threading-posix.c"
  )
  target_compile_definitions(libobs-stub PRIVATE _GNU_SOURCE)
  target_link_libraries(libobs-stub PUBLIC uuid)
endif()

target_compile_definitions(libobs-stub PRIVATE IS_LIBOBS)
target_include_directories(
  libobs-stub
  PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}"
    "${CMAKE_CURRENT_LIST_DIR}/../obs-studio/deps/libcaption"
    "${CMAKE_CURRENT_LIST_DIR}/../obs-studio/deps/uthash"
    "${JANSSON_SOURCE_DIR}"
  PUBLIC "${LIBOBS_SOURCE_DIR}"
)
target_link_libraries(libobs-stub PUBLIC Threads::Threads)

set_target_properties(
  libobs-stub
  PROPERTIES C_STANDARD 17 C_STANDARD_REQUIRED YES C_EXTENSIONS NO
)
