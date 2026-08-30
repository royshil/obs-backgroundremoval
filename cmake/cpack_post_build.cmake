# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

if(NOT CPACK_GENERATOR STREQUAL "DEB" OR "$ENV{GITHUB_OUTPUT}" STREQUAL "")
  return()
endif()

set(_github_output_lines)

foreach(_package_file IN LISTS CPACK_PACKAGE_FILES)
  get_filename_component(_package_name "${_package_file}" NAME)

  if(_package_name MATCHES "\\.ddeb$")
    list(APPEND _github_output_lines "DDEB_ARTIFACT_NAME=${_package_name}")
  elseif(_package_name MATCHES "\\.deb$")
    list(APPEND _github_output_lines "DEB_ARTIFACT_NAME=${_package_name}")
  endif()
endforeach()

if(NOT _github_output_lines)
  message(FATAL_ERROR "No DEB artifacts were generated")
endif()

list(JOIN _github_output_lines "\n" _github_output)
file(APPEND "$ENV{GITHUB_OUTPUT}" "${_github_output}\n")
