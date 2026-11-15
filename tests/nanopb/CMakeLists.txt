# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0
add_library(
  test_onnx_nanopb
  STATIC
  onnx.pb.c
  "${PROJECT_SOURCE_DIR}/vendor/nanopb/pb_common.c"
  "${PROJECT_SOURCE_DIR}/vendor/nanopb/pb_decode.c"
  "${PROJECT_SOURCE_DIR}/vendor/nanopb/pb_encode.c"
)
target_include_directories(test_onnx_nanopb PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_SOURCE_DIR}/vendor/nanopb")
