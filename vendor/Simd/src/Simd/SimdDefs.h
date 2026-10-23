/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2026 Yermalayeu Ihar,
*               2022-2022 Souriya Trinh,
*               Qualcomm Technologies, Inc. and/or its subsidiaries.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/
#ifndef __SimdDefs_h__
#define __SimdDefs_h__

#include "Simd/SimdConfig.h"
#include "Simd/SimdLib.h"

#include <stddef.h>
#include <stdlib.h>
#include <assert.h>
#include <memory.h>
#include <limits.h>
#include <float.h>
#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <cmath>
#include <limits>

#if defined(SIMD_SSE41_DISABLE) && !defined(SIMD_AVX2_DISABLE)
#define SIMD_AVX2_DISABLE
#endif

#if defined(SIMD_AVX2_DISABLE) && !defined(SIMD_AVX512BW_DISABLE)
#define SIMD_AVX512BW_DISABLE
#endif

#if defined(SIMD_AVX512BW_DISABLE) && !defined(SIMD_AVX512VNNI_DISABLE)
#define SIMD_AVX512VNNI_DISABLE
#endif

#if defined(SIMD_AVX512VNNI_DISABLE) && !defined(SIMD_AMXBF16_DISABLE)
#define SIMD_AMXBF16_DISABLE
#endif

#if defined(SIMD_NEON_DISABLE) && !defined(SIMD_SVE_DISABLE)
#define SIMD_SVE_DISABLE
#endif

#if defined(SIMD_SVE_DISABLE) && !defined(SIMD_SVE2_DISABLE)
#define SIMD_SVE2_DISABLE
#endif

#if !defined(SIMD_SYNET_DISABLE)
#define SIMD_SYNET_ENABLE
#endif

#if !defined(SIMD_INT8_DEBUG_DISABLE)
#define SIMD_INT8_DEBUG_ENABLE
#endif

#if defined(_MSC_VER) && defined(_MSC_FULL_VER)

#define SIMD_ALIGNED(x) __declspec(align(x))

#define SIMD_NOINLINE __declspec(noinline)

#ifdef _M_IX86
#define SIMD_X86_ENABLE
#endif

#if defined(_M_X64) || defined(_M_AMD64)
#define SIMD_X64_ENABLE
#endif

#if defined(_M_ARM)
#define SIMD_ARM_ENABLE
#endif

#if defined _M_ARM64
#define SIMD_ARM64_ENABLE
#endif

#if defined(SIMD_X64_ENABLE) || defined(SIMD_X86_ENABLE)

#if !defined(SIMD_SSE41_DISABLE) && _MSC_VER >= 1500
#define SIMD_SSE41_ENABLE
#endif

#if !defined(SIMD_AVX2_DISABLE) && _MSC_VER >= 1700
#define SIMD_AVX2_ENABLE
#endif

#if defined(NDEBUG) && _MSC_VER >= 1700 && _MSC_VER < 1900
#define SIMD_MADDUBS_ERROR // Visual Studio 2012/2013 release mode compiler bug in function _mm256_maddubs_epi16.
#endif

#if !defined(SIMD_AVX512BW_DISABLE) && _MSC_VER >= 1920
#define SIMD_AVX512BW_ENABLE
#endif

#if !defined(SIMD_AVX512VNNI_DISABLE) && _MSC_VER >= 1924
#define SIMD_AVX512VNNI_ENABLE
#endif

#if !defined(SIMD_AMXBF16_DISABLE) && _MSC_VER >= 1933 && defined(_M_X64)
#define SIMD_AMXBF16_ENABLE
#endif

#if defined(NDEBUG) && _MSC_VER <= 1920
#define SIMD_MASKZ_LOAD_ERROR
#endif

#if _MSC_VER < 1924
#define SIMD_AVX512_FLOOR_CEIL_ABSENT
#endif

#if defined(NDEBUG)
#define SIMD_MSVS_COMPILER_OUT_OF_HEAP_SPACE
#endif

#endif//defined(SIMD_X64_ENABLE) || defined(SIMD_X86_ENABLE)

#if defined(SIMD_ARM_ENABLE) || defined(SIMD_ARM64_ENABLE)

#if !defined(SIMD_NEON_DISABLE) && _MSC_VER >= 1700
#define SIMD_NEON_ENABLE
#endif

#endif

#if _MSC_VER >= 1900
#define SIMD_CPP_2011_ENABLE
#endif

#if _MSVC_LANG >= 201402L
#define SIMD_CPP_2014_ENABLE
#endif

#if _MSVC_LANG >= 201703L
#define SIMD_CPP_2017_ENABLE
#endif

#define SIMD_FUNCTION __FUNCTION__

#elif defined(__GNUC__)

#define SIMD_ALIGNED(x) __attribute__ ((aligned(x)))

#define SIMD_NOINLINE __attribute__ ((noinline))

#ifdef __i386__
#define SIMD_X86_ENABLE
#endif

#if defined(__x86_64__) || defined(__amd64__)
#define SIMD_X64_ENABLE
#endif

#if defined(__riscv)
#define SIMD_RISCV_ENABLE
#endif

#if defined(__riscv) && defined(__riscv_xlen) && (__riscv_xlen == 64)
#define SIMD_RISCV64_ENABLE
#endif

#if (defined(_BIG_ENDIAN) && !defined(_LITTLE_ENDIAN)) || (defined(__BIG_ENDIAN__) && !defined(__LITTLE_ENDIAN__))
#define SIMD_BIG_ENDIAN
#elif defined(__GLIBC__) || (defined(__GNUC__) && !defined(__llvm__) && !defined(__MINGW32__) && !defined(__FreeBSD__) && defined(__BYTE_ORDER__))
  #include <endian.h>
  #if defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN)
    #define SIMD_BIG_ENDIAN
  #endif
#elif defined(__sparc) || defined(__sparc__) || defined(_POWER) || defined(__powerpc__) || defined(__ppc__) ||         \
    defined(__hpux) || defined(_MIPSEB) || defined(_POWER) || defined(__s390__)
  #define SIMD_BIG_ENDIAN
#endif

#if defined __arm__
#define SIMD_ARM_ENABLE
#endif

#if defined __aarch64__
#define SIMD_ARM64_ENABLE
#endif

#if defined __hexagon__
#define SIMD_HEXAGON_ENABLE
#endif

#if defined(SIMD_X86_ENABLE) || defined(SIMD_X64_ENABLE)

#if !defined(SIMD_SSE41_DISABLE) && defined(__SSE__) && defined(__SSE2__) && defined(__SSE3__) && defined(__SSSE3__) && defined(__SSE4_1__) && defined(__SSE4_2__)
#define SIMD_SSE41_ENABLE
#endif

#if !defined(SIMD_AVX2_DISABLE) && defined(__AVX__) && defined(__AVX2__)
#define SIMD_AVX2_ENABLE
#endif

#if !defined(__clang__) || (defined(__clang__) && __clang_major__ >= 4)
#if !defined(SIMD_AVX512BW_DISABLE) && defined(__AVX512BW__)
#define SIMD_AVX512BW_ENABLE
#endif

#if !defined(SIMD_AVX512VNNI_DISABLE) && defined(__AVX512VNNI__)
#define SIMD_AVX512VNNI_ENABLE
#endif

#if !defined(SIMD_AMXBF16_DISABLE) && defined(__AMX_TILE__) && defined(__AMX_INT8__) && defined(__AMX_BF16__) && defined(__AVX512BF16__)
#define SIMD_AMXBF16_ENABLE
#endif
#endif

#endif//defined(SIMD_X86_ENABLE) || defined(SIMD_X64_ENABLE)

#if defined(SIMD_ARM_ENABLE) || defined(SIMD_ARM64_ENABLE)

#if !defined(SIMD_NEON_DISABLE) && (defined(__ARM_NEON) || defined(SIMD_ARM64_ENABLE))
#define SIMD_NEON_ENABLE
#endif

#if !defined(SIMD_NEON_ASM_DISABLE) && defined(__GNUC__)
#define SIMD_NEON_ASM_ENABLE
#endif

#if !defined(SIMD_NEON_FP16_DISABLE) && (defined(__ARM_FP16_FORMAT_IEEE) || defined(__ARM_FP16_FORMAT_ALTERNATIVE))
#define SIMD_NEON_FP16_ENABLE
#endif

#if !defined(SIMD_SVE_DISABLE) && defined(__ARM_FEATURE_SVE) && defined(__ARM_NEON)
#define SIMD_SVE_ENABLE
#endif

#if !defined(SIMD_SVE2_DISABLE) && defined(__ARM_FEATURE_SVE2)
#define SIMD_SVE2_ENABLE
#define SIMD_SVE2_VECTOR_SIZE_MAX 64
#endif

#endif//defined(SIMD_ARM_ENABLE) || defined(SIMD_ARM64_ENABLE)

#if defined(SIMD_HEXAGON_ENABLE)

#if !defined(SIMD_HVX_DISABLE) && defined(__HVX__)
#define SIMD_HVX_ENABLE
#endif

#endif//defined(SIMD_HEXAGON_ENABLE)

#if defined(__clang__) && !defined(__hexagon__)
#define SIMD_CLANG_AVX2_BGR_TO_BGRA_ERROR
#endif

#define SIMD_FUNCTION __PRETTY_FUNCTION__

#else

#error This platform is unsupported!

#endif

#ifdef SIMD_X64_ENABLE
#include <mmintrin.h>
#include <emmintrin.h>
#endif

#ifdef SIMD_SSE41_ENABLE
#include <nmmintrin.h>
#endif

#if defined(SIMD_AVX2_ENABLE) \
    || defined(SIMD_AVX512BW_ENABLE) || defined(SIMD_AVX512VNNI_ENABLE) \
    || defined(SIMD_AMXBF16_ENABLE)
#include <immintrin.h>
#endif

#if defined(SIMD_NEON_ENABLE)
#include <arm_neon.h>
#endif

#if defined(SIMD_SVE_ENABLE) || defined(SIMD_SVE2_ENABLE)
#include <arm_sve.h>
#endif

#if defined(SIMD_HVX_ENABLE)
#include <hexagon_types.h>
#include <hvx_hexagon_protos.h>
#endif

#if defined(SIMD_HVX_ENABLE)
#define SIMD_ALIGN 128
#elif defined(SIMD_SVE2_ENABLE)
#define SIMD_ALIGN SIMD_SVE2_VECTOR_SIZE_MAX
#elif defined(SIMD_AVX512BW_ENABLE) || defined(SIMD_AVX512VNNI_ENABLE) || defined(SIMD_AMXBF16_ENABLE)
#define SIMD_ALIGN 64
#elif defined(SIMD_AVX2_ENABLE)
#define SIMD_ALIGN 32
#elif defined(SIMD_SSE41_ENABLE) \
	|| defined(SIMD_NEON_ENABLE)
#define SIMD_ALIGN 16
#elif defined (SIMD_X64_ENABLE) || defined(SIMD_ARM64_ENABLE) || defined(SIMD_RISCV64_ENABLE)
#define SIMD_ALIGN 8
#else
#define SIMD_ALIGN 4
#endif

#if (defined(SIMD_AVX512BW_ENABLE) || defined(SIMD_AVX512VNNI_ENABLE) || defined(SIMD_AMXBF16_ENABLE))
#ifdef SIMD_X64_ENABLE
#if defined(__GNUC__) || (defined(_MSC_VER) && _MSC_VER >= 1915)
#define SIMD_ZMM_COUNT 32
#else
#define SIMD_ZMM_COUNT 16
#endif
#else
#define SIMD_ZMM_COUNT 8
#endif
#endif

#define SIMD_CAT_DO(a, b) a##b
#define SIMD_CAT(a, b) SIMD_CAT_DO(a, b)

#endif
