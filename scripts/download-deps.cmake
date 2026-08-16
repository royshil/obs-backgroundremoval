# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

if(NOT DEFINED ENV{URL})
  message(FATAL_ERROR "URL envvar is required")
endif()

if(NOT DEFINED ENV{SHA256})
  message(FATAL_ERROR "SHA256 envvar is required")
endif()

if(NOT DEFINED ENV{JOB_TEMP})
  message(FATAL_ERROR "JOB_TEMP envvar is required")
endif()

if(NOT DEFINED ENV{DESTDIR})
  message(FATAL_ERROR "DESTDIR envvar is required")
endif()

get_filename_component(filename "$ENV{URL}" NAME)
set(archive "$ENV{JOB_TEMP}/${filename}")

file(MAKE_DIRECTORY "$ENV{JOB_TEMP}" "$ENV{DESTDIR}")
file(DOWNLOAD "$ENV{URL}" "${archive}" EXPECTED_HASH "SHA256=$ENV{SHA256}" STATUS download_status TLS_VERIFY ON)

list(GET download_status 0 status_code)
list(GET download_status 1 status_message)
if(NOT status_code EQUAL 0)
  message(FATAL_ERROR "Download failed: ${status_message}")
endif()

file(ARCHIVE_EXTRACT INPUT "${archive}" DESTINATION "$ENV{DESTDIR}")
