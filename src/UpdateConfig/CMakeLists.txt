# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(UpdateConfig STATIC UpdateConfigCommon.cpp)

target_link_libraries(UpdateConfig PRIVATE BuildOptions OBS::libobs)

if(CURL_FOUND)
  message(STATUS "UpdateConfig cURL backend selected")
  target_sources(UpdateConfig PRIVATE UpdateConfigCurl.cpp)
  target_link_libraries(UpdateConfig PRIVATE CURL::libcurl)
  if(APPLE)
    target_link_libraries(UpdateConfig PRIVATE jthread::jthread)
  endif()
else()
  message(STATUS "UpdateConfig dummy backend selected")
  target_sources(UpdateConfig PRIVATE UpdateConfigDummy.cpp)
endif()
