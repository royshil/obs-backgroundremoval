# SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
#
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.28)

project(SimdBaseVendored LANGUAGES CXX)

add_library(
  SimdBaseVendored
  STATIC
  src/Simd/SimdBaseCopy.cpp
  src/Simd/SimdBaseGaussianBlur.cpp
  src/Simd/SimdBaseMaxFilter.cpp
  src/Simd/SimdBaseMinFilter.cpp
  src/Simd/SimdBaseResizerBilinear.cpp
  src/Simd/SimdBaseThread.cpp
)

target_compile_features(SimdBaseVendored PUBLIC cxx_std_20)
target_compile_definitions(SimdBaseVendored PUBLIC SIMD_SSE41_DISABLE SIMD_AVX2_DISABLE SIMD_RUNTIME_DISABLE)
target_include_directories(SimdBaseVendored SYSTEM PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")

if(MSVC)
  target_compile_options(SimdBaseVendored PRIVATE /utf-8 /Zc:preprocessor /permissive- /Zc:__cplusplus)
endif()

add_library(Simd::BaseVendored ALIAS SimdBaseVendored)
