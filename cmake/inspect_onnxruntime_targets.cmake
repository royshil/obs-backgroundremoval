# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

function(write_onnxruntime_target_properties)
  set(output_file "${CMAKE_BINARY_DIR}/onnxruntime-static-targets.txt")
  file(WRITE "${output_file}" "")

  foreach(target IN ITEMS onnxruntime onnxruntime_providers_webgpu dawn::dawn_native dawn::dawn_proc)
    if(NOT TARGET "${target}")
      file(APPEND "${output_file}" "TARGET ${target}: NOT FOUND\n\n")
      continue()
    endif()

    file(APPEND "${output_file}" "TARGET ${target}\n")
    foreach(
      property
      IN
      ITEMS
        TYPE
        IMPORTED
        ALIASED_TARGET
        INTERFACE_INCLUDE_DIRECTORIES
        INTERFACE_COMPILE_DEFINITIONS
        INTERFACE_COMPILE_OPTIONS
        INTERFACE_LINK_DIRECTORIES
        INTERFACE_LINK_LIBRARIES
        INTERFACE_LINK_OPTIONS
        LINK_LIBRARIES
    )
      get_target_property(value "${target}" "${property}")
      if(value STREQUAL "value-NOTFOUND")
        set(value "")
      endif()
      file(APPEND "${output_file}" "${property}=${value}\n")
    endforeach()
    file(APPEND "${output_file}" "\n")
  endforeach()
endfunction()

cmake_language(DEFER CALL write_onnxruntime_target_properties)
