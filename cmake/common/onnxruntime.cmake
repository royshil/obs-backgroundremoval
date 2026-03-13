# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

include_guard(GLOBAL)

find_package(absl CONFIG REQUIRED)
find_package(Boost CONFIG COMPONENTS mp11 REQUIRED)
find_package(date CONFIG REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(flatbuffers CONFIG REQUIRED)
find_package(Microsoft.GSL 4.0 CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(ONNX CONFIG REQUIRED)
find_package(protobuf CONFIG REQUIRED)
find_package(re2 CONFIG REQUIRED)
find_package(Threads REQUIRED)

add_library(safeint_interface IMPORTED INTERFACE)

if(APPLE)
  find_package(Iconv REQUIRED)

  add_library(cpuinfo STATIC IMPORTED)
  set_target_properties(
    cpuinfo
    PROPERTIES
      IMPORTED_LOCATION
        "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/lib/${CMAKE_STATIC_LIBRARY_PREFIX}cpuinfo${CMAKE_STATIC_LIBRARY_SUFFIX}"
  )
  add_library(cpuinfo::cpuinfo ALIAS cpuinfo)
elseif(MSVC)
  find_package(cpuinfo CONFIG REQUIRED)
else()
  find_package(Iconv REQUIRED)
  find_package(cpuinfo CONFIG REQUIRED)
endif()

add_library(onnxruntime INTERFACE)

set(
  ORT_COMPONENTS
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
)

if(APPLE)
  list(APPEND ORT_COMPONENTS onnxruntime_providers_coreml coreml_proto kleidiai)
endif()

foreach(name IN LISTS ORT_COMPONENTS)
  add_library(${name} STATIC IMPORTED)
  set_target_properties(
    ${name}
    PROPERTIES
      IMPORTED_LOCATION
        "${CMAKE_CURRENT_SOURCE_DIR}/.deps_vendor/lib/${CMAKE_STATIC_LIBRARY_PREFIX}${name}${CMAKE_STATIC_LIBRARY_SUFFIX}"
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
    onnxruntime::onnxruntime_session
    onnxruntime::onnxruntime_optimizer
    onnxruntime::onnxruntime_providers
    onnxruntime::onnxruntime_lora
    onnxruntime::onnxruntime_framework
    onnxruntime::onnxruntime_graph
    onnxruntime::onnxruntime_util
    onnxruntime::onnxruntime_mlas
    onnxruntime::onnxruntime_common
    onnxruntime::onnxruntime_flatbuffers
    nlohmann_json::nlohmann_json
    ONNX::onnx
    ONNX::onnx_proto
    protobuf::libprotobuf-lite
    re2::re2
    Boost::headers
    safeint_interface
    flatbuffers::flatbuffers
    Microsoft.GSL::GSL
    absl::synchronization
    absl::tracing_internal
    absl::time
    absl::time_zone
    absl::civil_time
    absl::symbolize
    absl::demangle_internal
    absl::demangle_rust
    absl::stacktrace
    absl::debugging_internal
    absl::malloc_internal
    absl::kernel_timeout_internal
    absl::graphcycles_internal
    absl::str_format
    absl::str_format_internal
    absl::flat_hash_set
    absl::flat_hash_map
    absl::algorithm_container
    absl::raw_hash_map
    absl::raw_hash_set
    absl::prefetch
    absl::hashtablez_sampler
    absl::hashtable_debug_hooks
    absl::hashtable_control_bytes
    absl::hash_policy_traits
    absl::common_policy_traits
    absl::hash_container_defaults
    absl::hash_function_defaults
    absl::cord
    absl::inlined_vector
    absl::inlined_vector_internal
    absl::span
    absl::crc_cord_state
    absl::crc32c
    absl::cordz_update_tracker
    absl::cordz_update_scope
    absl::cordz_info
    absl::cordz_functions
    absl::cord_internal
    absl::container_common
    absl::container_memory
    absl::hash
    absl::variant
    absl::optional
    absl::strings
    absl::charset
    absl::strings_internal
    absl::string_view
    absl::int128
    absl::compare
    absl::function_ref
    absl::any_invocable
    absl::city
    absl::bits
    absl::endian
    absl::flags
    absl::fixed_array
    absl::weakly_mixed_integer
    absl::memory
    absl::meta
    absl::throw_delegate
    absl::iterator_traits_internal
    absl::algorithm
    absl::compressed_tuple
    absl::utility
    absl::base
    absl::type_traits
    absl::spinlock_wait
    absl::raw_logging_internal
    absl::errno_saver
    absl::nullability
    absl::log_severity
    absl::dynamic_annotations
    absl::base_internal
    absl::atomic_hook
    absl::core_headers
    absl::config
    absl::absl_log
    absl::log_internal_log_impl
    absl::absl_check
    absl::log_internal_check_impl
    date::date
    Eigen3::Eigen
    Threads::Threads
    cpuinfo::cpuinfo
)

if(APPLE)
  target_link_libraries(
    onnxruntime
    INTERFACE
      Iconv::Iconv
      onnxruntime::onnxruntime_providers_coreml
      coreml_proto
      onnxruntime::kleidiai
      "-framework Foundation"
  )
elseif(NOT MSVC)
  target_link_libraries(onnxruntime INTERFACE Iconv::Iconv dl rt)
endif()

add_library(onnxruntime::onnxruntime ALIAS onnxruntime)
