# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(UpdateConfig STATIC UpdateConfigCommon.cpp UpdateConfigCurl.cpp)

target_link_libraries(UpdateConfig PRIVATE BuildOptions OBS::libobs CURL::libcurl)

if(APPLE)
  target_link_libraries(UpdateConfig PRIVATE jthread::jthread)
endif()
