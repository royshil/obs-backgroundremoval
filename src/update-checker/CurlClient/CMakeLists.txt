# SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
# SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: GPL-3.0-or-later

add_library(CurlClient STATIC CurlClient.cpp)
target_link_libraries(CurlClient PRIVATE OBS::libobs plugin-support CURL::libcurl)
target_include_directories(CurlClient PRIVATE "${CMAKE_SOURCE_DIR}/src")
