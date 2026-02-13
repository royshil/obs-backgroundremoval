cmake_minimum_required(VERSION 3.16)

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/common/bootstrap.cmake" NO_POLICY_SCOPE)

project(${_name} VERSION ${_version})

option(ENABLE_FRONTEND_API "Use obs-frontend-api for UI functionality" OFF)
option(ENABLE_QT "Use Qt functionality" OFF)

set(VCPKG_TARGET_TRIPLET "" CACHE STRING "Vcpkg target triplet to use")
option(USE_PKGCONFIG "Use pkg-config to find dependencies" ON)
option(USE_SYSTEM_ONNXRUNTIME "Use system ONNX Runtime" ON)

include(compilerconfig)
include(defaults)
include(helpers)

add_library(${CMAKE_PROJECT_NAME} MODULE)

find_package(libobs REQUIRED)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::libobs)

if(ENABLE_FRONTEND_API)
  find_package(obs-frontend-api REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE OBS::obs-frontend-api)
endif()

if(ENABLE_QT)
  find_package(Qt6 COMPONENTS Widgets Core)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE Qt6::Core Qt6::Widgets)
  target_compile_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-quoted-include-in-framework-header -Wno-comma>
  )
  set_target_properties(
    ${CMAKE_PROJECT_NAME}
    PROPERTIES AUTOMOC ON AUTOUIC ON AUTORCC ON
  )
endif()

if(NOT VCPKG_TARGET_TRIPLET STREQUAL "")
  # Use vcpkg to find dependencies (Official binary)

  message(STATUS "Using vcpkg with target triplet: ${VCPKG_TARGET_TRIPLET}")

  list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_SOURCE_DIR}/vcpkg_installed/${VCPKG_TARGET_TRIPLET}")

  find_package(CURL CONFIG REQUIRED)
  find_package(OpenCV CONFIG REQUIRED)

  add_library(OpenCV::opencv_core ALIAS opencv_core)
  add_library(OpenCV::opencv_imgproc ALIAS opencv_imgproc)
elseif(USE_PKGCONFIG)
  # Use pkg-config to find dependencies (Arch Linux, etc.)

  message(STATUS "Using pkg-config to find dependencies")

  find_package(PkgConfig REQUIRED)

  # CURL
  pkg_check_modules(PC_CURL REQUIRED libcurl)
  add_library(CURL::libcurl INTERFACE IMPORTED)
  target_link_libraries(CURL::libcurl INTERFACE ${PC_CURL_LIBRARIES})
  target_include_directories(CURL::libcurl INTERFACE ${PC_CURL_INCLUDE_DIRS})
  target_compile_definitions(CURL::libcurl INTERFACE ${PC_CURL_CFLAGS_OTHER})
  target_link_directories(CURL::libcurl INTERFACE ${PC_CURL_LIBRARY_DIRS})

  # OpenCV
  pkg_check_modules(PC_OPENCV REQUIRED opencv4)
  add_library(OpenCV::opencv_core INTERFACE IMPORTED)
  target_link_libraries(OpenCV::opencv_core INTERFACE opencv_core)
  target_include_directories(OpenCV::opencv_core INTERFACE ${PC_OPENCV_INCLUDE_DIRS})
  target_compile_definitions(OpenCV::opencv_core INTERFACE ${PC_OPENCV_CFLAGS_OTHER})
  target_link_directories(OpenCV::opencv_core INTERFACE ${PC_OPENCV_LIBRARY_DIRS})
  add_library(OpenCV::opencv_imgproc INTERFACE IMPORTED)
  target_link_libraries(OpenCV::opencv_imgproc INTERFACE opencv_imgproc)
  target_link_directories(OpenCV::opencv_imgproc INTERFACE ${PC_OPENCV_LIBRARY_DIRS})
else()
  # Let system find dependencies (Fallback)

  message(STATUS "Using system to find dependencies")

  find_package(CURL REQUIRED)
  find_package(OpenCV CONFIG REQUIRED)
endif()

if(APPLE)
  if(USE_SYSTEM_ONNXRUNTIME)
    find_package(onnxruntime CONFIG REQUIRED)
  else()
    list(PREPEND CMAKE_PREFIX_PATH "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/ort_vcpkg_installed/universal-osx")

    find_package(absl CONFIG REQUIRED)
    find_package(Boost COMPONENTS mp11 REQUIRED)
    find_package(date CONFIG REQUIRED)
    find_package(Eigen3 CONFIG REQUIRED)
    find_package(flatbuffers CONFIG REQUIRED)
    find_package(Iconv REQUIRED)
    find_package(Microsoft.GSL 4.0 CONFIG REQUIRED)
    find_package(nlohmann_json CONFIG REQUIRED)
    find_package(ONNX CONFIG REQUIRED)
    find_package(Protobuf CONFIG REQUIRED)
    find_package(re2 CONFIG REQUIRED)

    add_library(cpuinfo STATIC IMPORTED)
    set_target_properties(
      cpuinfo
      PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/lib/libcpuinfo.a"
    )
    add_library(cpuinfo::cpuinfo ALIAS cpuinfo)

    add_library(onnxruntime INTERFACE)

    foreach(
      name
      onnxruntime_session
      onnxruntime_optimizer
      onnxruntime_providers
      onnxruntime_lora
      onnxruntime_framework
      onnxruntime_graph
      onnxruntime_util
      onnxruntime_mlas
      onnxruntime_common
      onnxruntime_flatbuffers
      onnxruntime_providers_coreml
      coreml_proto
      kleidiai
    )
      add_library(${name} STATIC IMPORTED)
      set_target_properties(
        ${name}
        PROPERTIES IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/lib/lib${name}.a"
      )
      add_library(onnxruntime::${name} ALIAS ${name})
    endforeach()

    target_include_directories(
      onnxruntime
      INTERFACE
        "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/onnxruntime/include/onnxruntime/core/session"
        "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/onnxruntime/include/onnxruntime/core/providers/cpu"
        "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/onnxruntime/include/onnxruntime/core/providers/coreml"
    )

    target_link_libraries(
      onnxruntime
      INTERFACE
        "onnxruntime::onnxruntime_session;onnxruntime::onnxruntime_providers_coreml;coreml_proto;onnxruntime::onnxruntime_optimizer;onnxruntime::onnxruntime_providers;onnxruntime::onnxruntime_lora;onnxruntime::onnxruntime_framework;onnxruntime::onnxruntime_graph;onnxruntime::onnxruntime_util;onnxruntime::onnxruntime_mlas;onnxruntime::onnxruntime_common;onnxruntime::onnxruntime_flatbuffers;nlohmann_json::nlohmann_json;ONNX::onnx;ONNX::onnx_proto;protobuf::libprotobuf-lite;re2::re2;Boost::headers;safeint_interface;flatbuffers::flatbuffers;Microsoft.GSL::GSL;absl::synchronization;absl::tracing_internal;absl::time;absl::time_zone;absl::civil_time;absl::symbolize;absl::demangle_internal;absl::demangle_rust;absl::stacktrace;absl::debugging_internal;absl::malloc_internal;absl::kernel_timeout_internal;absl::graphcycles_internal;absl::str_format;absl::str_format_internal;absl::flat_hash_set;absl::flat_hash_map;absl::algorithm_container;absl::raw_hash_map;absl::raw_hash_set;absl::prefetch;absl::hashtablez_sampler;absl::hashtable_debug_hooks;absl::hashtable_control_bytes;absl::hash_policy_traits;absl::common_policy_traits;absl::hash_container_defaults;absl::hash_function_defaults;absl::cord;absl::inlined_vector;absl::inlined_vector_internal;absl::span;absl::crc_cord_state;absl::crc32c;absl::cordz_update_tracker;absl::cordz_update_scope;absl::cordz_info;absl::cordz_functions;absl::cord_internal;absl::container_common;absl::container_memory;absl::hash;absl::variant;absl::optional;absl::strings;absl::charset;absl::strings_internal;absl::string_view;absl::int128;absl::compare;absl::function_ref;absl::any_invocable;absl::city;absl::bits;absl::endian;absl::flags;absl::fixed_array;absl::weakly_mixed_integer;absl::memory;absl::meta;absl::throw_delegate;absl::iterator_traits_internal;absl::algorithm;absl::compressed_tuple;absl::utility;absl::base;absl::type_traits;absl::spinlock_wait;absl::raw_logging_internal;absl::errno_saver;absl::nullability;absl::log_severity;absl::dynamic_annotations;absl::base_internal;absl::atomic_hook;absl::core_headers;absl::config;absl::absl_log;absl::log_internal_log_impl;absl::absl_check;absl::log_internal_check_impl;date::date;Eigen3::Eigen;Iconv::Iconv;Threads::Threads;cpuinfo::cpuinfo;onnxruntime::kleidiai"
    )

    add_library(safeint_interface IMPORTED INTERFACE)

    add_library(onnxruntime::onnxruntime ALIAS onnxruntime)
  endif()

  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE onnxruntime::onnxruntime)
elseif(MSVC)
  if(USE_SYSTEM_ONNXRUNTIME)
    message(FATAL_ERROR "USE_SYSTEM_ONNXRUNTIME is not supported on Windows")
  endif()

  set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/onnxruntime")

  add_library(onnxruntime::onnxruntime_provider_shared SHARED IMPORTED)
  set_target_properties(
    onnxruntime::onnxruntime_provider_shared
    PROPERTIES
      IMPORTED_IMPLIB "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.lib"
      IMPORTED_LOCATION "${ONNXRUNTIME_DIR}/lib/onnxruntime_providers_shared.dll"
      INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_DIR}/include"
  )

  add_library(onnxruntime::onnxruntime SHARED IMPORTED)
  set_target_properties(
    onnxruntime::onnxruntime
    PROPERTIES
      IMPORTED_IMPLIB "${ONNXRUNTIME_DIR}/lib/onnxruntime.lib"
      IMPORTED_LOCATION "${ONNXRUNTIME_DIR}/lib/onnxruntime.dll"
      INTERFACE_INCLUDE_DIRECTORIES "${ONNXRUNTIME_DIR}/include"
      INTERFACE_LINK_OPTIONS "/DELAYLOAD:onnxruntime.dll"
  )
  target_link_libraries(onnxruntime::onnxruntime INTERFACE onnxruntime::onnxruntime_provider_shared)

  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE onnxruntime::onnxruntime)

  target_sources(${CMAKE_PROJECT_NAME} PRIVATE src/DelayLoad.cpp)
  file(GLOB ONNXRUNTIME_DLL_PDB_FILES "${ONNXRUNTIME_DIR}/lib/*.dll" "${ONNXRUNTIME_DIR}/lib/*.pdb")
  install(FILES ${ONNXRUNTIME_DLL_PDB_FILES} DESTINATION "${CMAKE_PROJECT_NAME}/bin/64bit/obs-backgroundremoval")
else()
  if(NOT USE_SYSTEM_ONNXRUNTIME)
    set(ONNXRUNTIME_DIR "${CMAKE_SOURCE_DIR}/onnxruntime")
    list(PREPEND CMAKE_PREFIX_PATH "${ONNXRUNTIME_DIR}")
  endif()

  find_package(onnxruntime CONFIG REQUIRED)
  target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE onnxruntime::onnxruntime)

  if(NOT USE_SYSTEM_ONNXRUNTIME)
    file(GLOB ONNXRUNTIME_SO_FILES "${ONNXRUNTIME_DIR}/lib64/libonnxruntime.so.*")
    install(
      FILES ${ONNXRUNTIME_SO_FILES} "${ONNXRUNTIME_DIR}/lib64/libonnxruntime_providers_shared.so"
      DESTINATION ${CMAKE_INSTALL_LIBDIR}/obs-plugins/${CMAKE_PROJECT_NAME}
    )
    set_target_properties(${CMAKE_PROJECT_NAME} PROPERTIES INSTALL_RPATH "$ORIGIN/${CMAKE_PROJECT_NAME}")
  endif()
endif()

add_subdirectory(src/update-checker/CurlClient)
target_link_libraries(${CMAKE_PROJECT_NAME} PRIVATE CurlClient OpenCV::opencv_core OpenCV::opencv_imgproc)

if(NOT APPLE)
  # Check for ONNX Runtime Execution Providers
  include(CheckLibraryExists)

  check_library_exists(
    onnxruntime::onnxruntime
    OrtSessionOptionsAppendExecutionProvider_CUDA
    onnxruntime_c_api.h
    HAVE_ONNXRUNTIME_CUDA_EP
  )
  if(HAVE_ONNXRUNTIME_CUDA_EP)
    message(STATUS "ONNX Runtime CUDA Execution Provider found")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_ONNXRUNTIME_CUDA_EP)
  endif()

  check_library_exists(
    onnxruntime::onnxruntime
    OrtSessionOptionsAppendExecutionProvider_ROCM
    onnxruntime_c_api.h
    HAVE_ONNXRUNTIME_ROCM_EP
  )
  if(HAVE_ONNXRUNTIME_ROCM_EP)
    message(STATUS "ONNX Runtime ROCM Execution Provider found")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_ONNXRUNTIME_ROCM_EP)
  endif()

  check_library_exists(
    onnxruntime::onnxruntime
    OrtSessionOptionsAppendExecutionProvider_MIGraphX
    onnxruntime_c_api.h
    HAVE_ONNXRUNTIME_MIGRAPHX_EP
  )
  if(HAVE_ONNXRUNTIME_MIGRAPHX_EP)
    message(STATUS "ONNX Runtime MIGraphX Execution Provider found")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_ONNXRUNTIME_MIGRAPHX_EP)
  endif()

  check_library_exists(
    onnxruntime::onnxruntime
    OrtSessionOptionsAppendExecutionProvider_Tensorrt
    onnxruntime_c_api.h
    HAVE_ONNXRUNTIME_TENSORRT_EP
  )
  if(HAVE_ONNXRUNTIME_TENSORRT_EP)
    message(STATUS "ONNX Runtime TensorRT Execution Provider found")
    target_compile_definitions(${CMAKE_PROJECT_NAME} PRIVATE HAVE_ONNXRUNTIME_TENSORRT_EP)
  endif()
endif()

target_sources(
  ${CMAKE_PROJECT_NAME}
  PRIVATE
    src/plugin-main.c
    src/ort-utils/ort-session-utils.cpp
    src/obs-utils/obs-utils.cpp
    src/obs-utils/obs-config-utils.cpp
    src/update-checker/github-utils.cpp
    src/update-checker/update-checker.cpp
    src/background-filter-info.c
    src/background-filter.cpp
    src/enhance-filter.cpp
    src/enhance-filter-info.c
)
target_compile_options(
  ${CMAKE_PROJECT_NAME}
  PRIVATE $<$<C_COMPILER_ID:Clang,AppleClang>:-Wno-error=unused-command-line-argument>
)

if(APPLE)
  target_link_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE "-Wl,-exported_symbols_list,${CMAKE_CURRENT_SOURCE_DIR}/src/exported_symbols_macos.txt"
  )
elseif(MSVC)
  target_sources(${CMAKE_PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/exported_symbols_windows.def")
else()
  target_link_options(
    ${CMAKE_PROJECT_NAME}
    PRIVATE "-Wl,--version-script,${CMAKE_CURRENT_SOURCE_DIR}/src/exported_symbols_linux.map" "-Wl,-Bsymbolic"
  )
endif()

set_target_properties_plugin(${CMAKE_PROJECT_NAME} PROPERTIES OUTPUT_NAME ${_name})
