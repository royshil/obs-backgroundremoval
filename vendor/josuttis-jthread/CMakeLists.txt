# SPDX-FileCopyrightText: 2025-2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

add_library(jthread::jthread INTERFACE IMPORTED GLOBAL)
target_include_directories(jthread::jthread INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
