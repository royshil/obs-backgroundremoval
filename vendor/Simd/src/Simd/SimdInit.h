/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2024 Yermalayeu Ihar.
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
#ifndef __SimdInit_h__
#define __SimdInit_h__

#include "Simd/SimdDefs.h"

namespace Simd
{
#if defined(_MSC_VER) && !defined(__clang__) && (defined(SIMD_X64_ENABLE) || defined(SIMD_X86_ENABLE))

#define SIMD_INIT_AS_CHAR

#elif defined(__GNUC__) || defined(__clang__)

#define SIMD_INIT_AS_LONGLONG

#elif defined(_MSC_VER) && defined(SIMD_NEON_ENABLE)

#define SIMD_INIT_AS_ULONGLONG

#else

#error This platform is unsupported!

#endif

#if defined(SIMD_INIT_AS_CHAR)

    template <class T> SIMD_INLINE char GetChar(T value, size_t index)
    {
        return ((char*)&value)[index];
    }

#define SIMD_AS_CHAR(a) char(a)

#define SIMD_AS_2CHARS(a) \
	Simd::GetChar(int16_t(a), 0), Simd::GetChar(int16_t(a), 1)

#define SIMD_AS_4CHARS(a) \
	Simd::GetChar(int32_t(a), 0), Simd::GetChar(int32_t(a), 1), \
	Simd::GetChar(int32_t(a), 2), Simd::GetChar(int32_t(a), 3)

#define SIMD_AS_8CHARS(a) \
	Simd::GetChar(int64_t(a), 0), Simd::GetChar(int64_t(a), 1), \
	Simd::GetChar(int64_t(a), 2), Simd::GetChar(int64_t(a), 3), \
	Simd::GetChar(int64_t(a), 4), Simd::GetChar(int64_t(a), 5), \
	Simd::GetChar(int64_t(a), 6), Simd::GetChar(int64_t(a), 7)

#elif defined(SIMD_INIT_AS_LONGLONG)

#define SIMD_CHAR_AS_LONGLONG(a) (((long long)a) & 0xFF)

#define SIMD_SHORT_AS_LONGLONG(a) (((long long)a) & 0xFFFF)

#define SIMD_INT_AS_LONGLONG(a) (((long long)a) & 0xFFFFFFFF)

#define SIMD_LL_SET1_EPI8(a) \
    SIMD_CHAR_AS_LONGLONG(a) | (SIMD_CHAR_AS_LONGLONG(a) << 8) | \
    (SIMD_CHAR_AS_LONGLONG(a) << 16) | (SIMD_CHAR_AS_LONGLONG(a) << 24) | \
    (SIMD_CHAR_AS_LONGLONG(a) << 32) | (SIMD_CHAR_AS_LONGLONG(a) << 40) | \
    (SIMD_CHAR_AS_LONGLONG(a) << 48) | (SIMD_CHAR_AS_LONGLONG(a) << 56)

#define SIMD_LL_SET2_EPI8(a, b) \
    SIMD_CHAR_AS_LONGLONG(a) | (SIMD_CHAR_AS_LONGLONG(b) << 8) | \
    (SIMD_CHAR_AS_LONGLONG(a) << 16) | (SIMD_CHAR_AS_LONGLONG(b) << 24) | \
    (SIMD_CHAR_AS_LONGLONG(a) << 32) | (SIMD_CHAR_AS_LONGLONG(b) << 40) | \
    (SIMD_CHAR_AS_LONGLONG(a) << 48) | (SIMD_CHAR_AS_LONGLONG(b) << 56)

#define SIMD_LL_SETR_EPI8(a, b, c, d, e, f, g, h) \
    SIMD_CHAR_AS_LONGLONG(a) | (SIMD_CHAR_AS_LONGLONG(b) << 8) | \
    (SIMD_CHAR_AS_LONGLONG(c) << 16) | (SIMD_CHAR_AS_LONGLONG(d) << 24) | \
    (SIMD_CHAR_AS_LONGLONG(e) << 32) | (SIMD_CHAR_AS_LONGLONG(f) << 40) | \
    (SIMD_CHAR_AS_LONGLONG(g) << 48) | (SIMD_CHAR_AS_LONGLONG(h) << 56)

#define SIMD_LL_SET1_EPI16(a) \
    SIMD_SHORT_AS_LONGLONG(a) | (SIMD_SHORT_AS_LONGLONG(a) << 16) | \
    (SIMD_SHORT_AS_LONGLONG(a) << 32) | (SIMD_SHORT_AS_LONGLONG(a) << 48)

#define SIMD_LL_SET2_EPI16(a, b) \
    SIMD_SHORT_AS_LONGLONG(a) | (SIMD_SHORT_AS_LONGLONG(b) << 16) | \
    (SIMD_SHORT_AS_LONGLONG(a) << 32) | (SIMD_SHORT_AS_LONGLONG(b) << 48)

#define SIMD_LL_SETR_EPI16(a, b, c, d) \
    SIMD_SHORT_AS_LONGLONG(a) | (SIMD_SHORT_AS_LONGLONG(b) << 16) | \
    (SIMD_SHORT_AS_LONGLONG(c) << 32) | (SIMD_SHORT_AS_LONGLONG(d) << 48)

#define SIMD_LL_SET1_EPI32(a) \
    SIMD_INT_AS_LONGLONG(a) | (SIMD_INT_AS_LONGLONG(a) << 32)

#define SIMD_LL_SET2_EPI32(a, b) \
    SIMD_INT_AS_LONGLONG(a) | (SIMD_INT_AS_LONGLONG(b) << 32)

#elif defined(SIMD_INIT_AS_ULONGLONG)

#define SIMD_CHAR_AS_ULONGLONG(a) (((unsigned long long)a) & 0xFF)

#define SIMD_SHORT_AS_ULONGLONG(a) (((unsigned long long)a) & 0xFFFF)

#define SIMD_INT_AS_ULONGLONG(a) (((unsigned long long)a) & 0xFFFFFFFF)

#define SIMD_ULL_SET1_EPI8(a) \
    SIMD_CHAR_AS_ULONGLONG(a) | (SIMD_CHAR_AS_ULONGLONG(a) << 8) | \
    (SIMD_CHAR_AS_ULONGLONG(a) << 16) | (SIMD_CHAR_AS_ULONGLONG(a) << 24) | \
    (SIMD_CHAR_AS_ULONGLONG(a) << 32) | (SIMD_CHAR_AS_ULONGLONG(a) << 40) | \
    (SIMD_CHAR_AS_ULONGLONG(a) << 48) | (SIMD_CHAR_AS_ULONGLONG(a) << 56)

#define SIMD_ULL_SET2_EPI8(a, b) \
    SIMD_CHAR_AS_ULONGLONG(a) | (SIMD_CHAR_AS_ULONGLONG(b) << 8) | \
    (SIMD_CHAR_AS_ULONGLONG(a) << 16) | (SIMD_CHAR_AS_ULONGLONG(b) << 24) | \
    (SIMD_CHAR_AS_ULONGLONG(a) << 32) | (SIMD_CHAR_AS_ULONGLONG(b) << 40) | \
    (SIMD_CHAR_AS_ULONGLONG(a) << 48) | (SIMD_CHAR_AS_ULONGLONG(b) << 56)

#define SIMD_ULL_SETR_EPI8(a, b, c, d, e, f, g, h) \
    SIMD_CHAR_AS_ULONGLONG(a) | (SIMD_CHAR_AS_ULONGLONG(b) << 8) | \
    (SIMD_CHAR_AS_ULONGLONG(c) << 16) | (SIMD_CHAR_AS_ULONGLONG(d) << 24) | \
    (SIMD_CHAR_AS_ULONGLONG(e) << 32) | (SIMD_CHAR_AS_ULONGLONG(f) << 40) | \
    (SIMD_CHAR_AS_ULONGLONG(g) << 48) | (SIMD_CHAR_AS_ULONGLONG(h) << 56)

#define SIMD_ULL_SET1_EPI16(a) \
    SIMD_SHORT_AS_ULONGLONG(a) | (SIMD_SHORT_AS_ULONGLONG(a) << 16) | \
    (SIMD_SHORT_AS_ULONGLONG(a) << 32) | (SIMD_SHORT_AS_ULONGLONG(a) << 48)

#define SIMD_ULL_SET2_EPI16(a, b) \
    SIMD_SHORT_AS_ULONGLONG(a) | (SIMD_SHORT_AS_ULONGLONG(b) << 16) | \
    (SIMD_SHORT_AS_ULONGLONG(a) << 32) | (SIMD_SHORT_AS_ULONGLONG(b) << 48)

#define SIMD_ULL_SETR_EPI16(a, b, c, d) \
    SIMD_SHORT_AS_ULONGLONG(a) | (SIMD_SHORT_AS_ULONGLONG(b) << 16) | \
    (SIMD_SHORT_AS_ULONGLONG(c) << 32) | (SIMD_SHORT_AS_ULONGLONG(d) << 48)

#define SIMD_ULL_SET1_EPI32(a) \
    SIMD_INT_AS_ULONGLONG(a) | (SIMD_INT_AS_ULONGLONG(a) << 32)

#define SIMD_ULL_SET2_EPI32(a, b) \
    SIMD_INT_AS_ULONGLONG(a) | (SIMD_INT_AS_ULONGLONG(b) << 32)

#else

#error This platform is unsupported!

#endif

#if defined(SIMD_SSE41_ENABLE)

#if defined(SIMD_INIT_AS_CHAR)

#define SIMD_MM_SET1_EPI8(a) \
    {SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
    SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
    SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
    SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a)}

#define SIMD_MM_SET2_EPI8(a0, a1) \
    {SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
    SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
    SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
    SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1)}

#define SIMD_MM_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af) \
    {SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a2), SIMD_AS_CHAR(a3), \
    SIMD_AS_CHAR(a4), SIMD_AS_CHAR(a5), SIMD_AS_CHAR(a6), SIMD_AS_CHAR(a7), \
    SIMD_AS_CHAR(a8), SIMD_AS_CHAR(a9), SIMD_AS_CHAR(aa), SIMD_AS_CHAR(ab), \
    SIMD_AS_CHAR(ac), SIMD_AS_CHAR(ad), SIMD_AS_CHAR(ae), SIMD_AS_CHAR(af)}

#define SIMD_MM_SET1_EPI16(a) \
    {SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), \
    SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a)}

#define SIMD_MM_SET2_EPI16(a0, a1) \
    {SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), \
    SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1)}

#define SIMD_MM_SETR_EPI16(a0, a1, a2, a3, a4, a5, a6, a7) \
    {SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a2), SIMD_AS_2CHARS(a3), \
    SIMD_AS_2CHARS(a4), SIMD_AS_2CHARS(a5), SIMD_AS_2CHARS(a6), SIMD_AS_2CHARS(a7)}

#define SIMD_MM_SET1_EPI32(a) \
    {SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a)}

#define SIMD_MM_SET2_EPI32(a0, a1) \
    {SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1)}

#define SIMD_MM_SETR_EPI32(a0, a1, a2, a3) \
    {SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), SIMD_AS_4CHARS(a2), SIMD_AS_4CHARS(a3)}

#define SIMD_MM_SET1_EPI64(a) \
    {SIMD_AS_8CHARS(a), SIMD_AS_8CHARS(a)}

#define SIMD_MM_SET2_EPI64(a0, a1) \
    {SIMD_AS_8CHARS(a0), SIMD_AS_8CHARS(a1)}

#define SIMD_MM_SETR_EPI64(a0, a1) \
    {SIMD_AS_8CHARS(a0), SIMD_AS_8CHARS(a1)}

#elif defined(SIMD_INIT_AS_LONGLONG)

#define SIMD_MM_SET1_EPI8(a) \
    {SIMD_LL_SET1_EPI8(a), SIMD_LL_SET1_EPI8(a)}

#define SIMD_MM_SET2_EPI8(a0, a1) \
    {SIMD_LL_SET2_EPI8(a0, a1), SIMD_LL_SET2_EPI8(a0, a1)}

#define SIMD_MM_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af) \
    {SIMD_LL_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7), SIMD_LL_SETR_EPI8(a8, a9, aa, ab, ac, ad, ae, af)}

#define SIMD_MM_SET1_EPI16(a) \
    {SIMD_LL_SET1_EPI16(a), SIMD_LL_SET1_EPI16(a)}

#define SIMD_MM_SET2_EPI16(a0, a1) \
    {SIMD_LL_SET2_EPI16(a0, a1), SIMD_LL_SET2_EPI16(a0, a1)}

#define SIMD_MM_SETR_EPI16(a0, a1, a2, a3, a4, a5, a6, a7) \
    {SIMD_LL_SETR_EPI16(a0, a1, a2, a3), SIMD_LL_SETR_EPI16(a4, a5, a6, a7)}

#define SIMD_MM_SET1_EPI32(a) \
    {SIMD_LL_SET1_EPI32(a), SIMD_LL_SET1_EPI32(a)}

#define SIMD_MM_SET2_EPI32(a0, a1) \
    {SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a0, a1)}

#define SIMD_MM_SETR_EPI32(a0, a1, a2, a3) \
    {SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a2, a3)}

#define SIMD_MM_SET1_EPI64(a) \
    {a, a}

#define SIMD_MM_SET2_EPI64(a0, a1) \
    {a0, a1}

#define SIMD_MM_SETR_EPI64(a0, a1) \
    {a0, a1}

#endif// defined(_MSC_VER) || defined(__GNUC__)

#endif// SIMD_SSE41_ENABLE

#if defined(SIMD_AVX2_ENABLE)

#if defined(SIMD_INIT_AS_CHAR)

#define SIMD_MM256_SET1_EPI8(a) \
	{SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a)}

#define SIMD_MM256_SET2_EPI8(a0, a1) \
	{SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1)}

#define SIMD_MM256_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, ba, bb, bc, bd, be, bf) \
    {SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a2), SIMD_AS_CHAR(a3), \
    SIMD_AS_CHAR(a4), SIMD_AS_CHAR(a5), SIMD_AS_CHAR(a6), SIMD_AS_CHAR(a7), \
    SIMD_AS_CHAR(a8), SIMD_AS_CHAR(a9), SIMD_AS_CHAR(aa), SIMD_AS_CHAR(ab), \
    SIMD_AS_CHAR(ac), SIMD_AS_CHAR(ad), SIMD_AS_CHAR(ae), SIMD_AS_CHAR(af), \
    SIMD_AS_CHAR(b0), SIMD_AS_CHAR(b1), SIMD_AS_CHAR(b2), SIMD_AS_CHAR(b3), \
    SIMD_AS_CHAR(b4), SIMD_AS_CHAR(b5), SIMD_AS_CHAR(b6), SIMD_AS_CHAR(b7), \
    SIMD_AS_CHAR(b8), SIMD_AS_CHAR(b9), SIMD_AS_CHAR(ba), SIMD_AS_CHAR(bb), \
    SIMD_AS_CHAR(bc), SIMD_AS_CHAR(bd), SIMD_AS_CHAR(be), SIMD_AS_CHAR(bf)}

#define SIMD_MM256_SET1_EPI16(a) \
	{SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), \
	SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), \
	SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), \
	SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a)}

#define SIMD_MM256_SET2_EPI16(a0, a1) \
	{SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), \
	SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), \
	SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), \
	SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1)}

#define SIMD_MM256_SETR_EPI16(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af) \
    {SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a2), SIMD_AS_2CHARS(a3), \
    SIMD_AS_2CHARS(a4), SIMD_AS_2CHARS(a5), SIMD_AS_2CHARS(a6), SIMD_AS_2CHARS(a7), \
    SIMD_AS_2CHARS(a8), SIMD_AS_2CHARS(a9), SIMD_AS_2CHARS(aa), SIMD_AS_2CHARS(ab), \
    SIMD_AS_2CHARS(ac), SIMD_AS_2CHARS(ad), SIMD_AS_2CHARS(ae), SIMD_AS_2CHARS(af)}

#define SIMD_MM256_SET1_EPI32(a) \
	{SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), \
	SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a)}

#define SIMD_MM256_SET2_EPI32(a0, a1) \
	{SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), \
	SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1)}

#define SIMD_MM256_SETR_EPI32(a0, a1, a2, a3, a4, a5, a6, a7) \
    {SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), SIMD_AS_4CHARS(a2), SIMD_AS_4CHARS(a3), \
    SIMD_AS_4CHARS(a4), SIMD_AS_4CHARS(a5), SIMD_AS_4CHARS(a6), SIMD_AS_4CHARS(a7)}

#define SIMD_MM256_SET1_EPI64(a) \
	{SIMD_AS_8CHARS(a), SIMD_AS_8CHARS(a), SIMD_AS_8CHARS(a), SIMD_AS_8CHARS(a)}

#define SIMD_MM256_SET2_EPI64(a0, a1) \
	{SIMD_AS_8CHARS(a0), SIMD_AS_8CHARS(a1), SIMD_AS_8CHARS(a0), SIMD_AS_8CHARS(a1)}

#define SIMD_MM256_SETR_EPI64(a0, a1, a2, a3) \
    {SIMD_AS_8CHARS(a0), SIMD_AS_8CHARS(a1), SIMD_AS_8CHARS(a2), SIMD_AS_8CHARS(a3)}

#elif defined(SIMD_INIT_AS_LONGLONG)

#define SIMD_MM256_SET1_EPI8(a) \
    {SIMD_LL_SET1_EPI8(a), SIMD_LL_SET1_EPI8(a), \
    SIMD_LL_SET1_EPI8(a), SIMD_LL_SET1_EPI8(a)}

#define SIMD_MM256_SET2_EPI8(a0, a1) \
    {SIMD_LL_SET2_EPI8(a0, a1), SIMD_LL_SET2_EPI8(a0, a1), \
    SIMD_LL_SET2_EPI8(a0, a1), SIMD_LL_SET2_EPI8(a0, a1)}

#define SIMD_MM256_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, ba, bb, bc, bd, be, bf) \
    {SIMD_LL_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7), SIMD_LL_SETR_EPI8(a8, a9, aa, ab, ac, ad, ae, af), \
    SIMD_LL_SETR_EPI8(b0, b1, b2, b3, b4, b5, b6, b7), SIMD_LL_SETR_EPI8(b8, b9, ba, bb, bc, bd, be, bf)}

#define SIMD_MM256_SET1_EPI16(a) \
    {SIMD_LL_SET1_EPI16(a), SIMD_LL_SET1_EPI16(a), \
    SIMD_LL_SET1_EPI16(a), SIMD_LL_SET1_EPI16(a)}

#define SIMD_MM256_SET2_EPI16(a0, a1) \
    {SIMD_LL_SET2_EPI16(a0, a1), SIMD_LL_SET2_EPI16(a0, a1), \
    SIMD_LL_SET2_EPI16(a0, a1), SIMD_LL_SET2_EPI16(a0, a1)}

#define SIMD_MM256_SETR_EPI16(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af) \
    {SIMD_LL_SETR_EPI16(a0, a1, a2, a3), SIMD_LL_SETR_EPI16(a4, a5, a6, a7), \
    SIMD_LL_SETR_EPI16(a8, a9, aa, ab), SIMD_LL_SETR_EPI16(ac, ad, ae, af)}

#define SIMD_MM256_SET1_EPI32(a) \
    {SIMD_LL_SET1_EPI32(a), SIMD_LL_SET1_EPI32(a), \
    SIMD_LL_SET1_EPI32(a), SIMD_LL_SET1_EPI32(a)}

#define SIMD_MM256_SET2_EPI32(a0, a1) \
    {SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a0, a1), \
    SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a0, a1)}

#define SIMD_MM256_SETR_EPI32(a0, a1, a2, a3, a4, a5, a6, a7) \
    {SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a2, a3), \
    SIMD_LL_SET2_EPI32(a4, a5), SIMD_LL_SET2_EPI32(a6, a7)}

#define SIMD_MM256_SET1_EPI64(a) \
    {a, a, a, a}

#define SIMD_MM256_SET2_EPI64(a0, a1) \
    {a0, a1, a0, a1}

#define SIMD_MM256_SETR_EPI64(a0, a1, a2, a3) \
    {a0, a1, a2, a3}

#endif

#endif// SIMD_AVX2_ENABLE

#if defined(SIMD_AVX512BW_ENABLE)

#if defined(SIMD_INIT_AS_CHAR)

#define SIMD_MM512_SET1_EPI8(a) \
	{SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), \
	SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a), SIMD_AS_CHAR(a)}

#define SIMD_MM512_SET2_EPI8(a0, a1) \
	{SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), \
	SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1)}

#define SIMD_MM512_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, ba, bb, bc, bd, be, bf, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, ca, cb, cc, cd, ce, cf, d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, da, db, dc, dd, de, df) \
    {SIMD_AS_CHAR(a0), SIMD_AS_CHAR(a1), SIMD_AS_CHAR(a2), SIMD_AS_CHAR(a3), SIMD_AS_CHAR(a4), SIMD_AS_CHAR(a5), SIMD_AS_CHAR(a6), SIMD_AS_CHAR(a7), \
    SIMD_AS_CHAR(a8), SIMD_AS_CHAR(a9), SIMD_AS_CHAR(aa), SIMD_AS_CHAR(ab), SIMD_AS_CHAR(ac), SIMD_AS_CHAR(ad), SIMD_AS_CHAR(ae), SIMD_AS_CHAR(af), \
    SIMD_AS_CHAR(b0), SIMD_AS_CHAR(b1), SIMD_AS_CHAR(b2), SIMD_AS_CHAR(b3), SIMD_AS_CHAR(b4), SIMD_AS_CHAR(b5), SIMD_AS_CHAR(b6), SIMD_AS_CHAR(b7), \
    SIMD_AS_CHAR(b8), SIMD_AS_CHAR(b9), SIMD_AS_CHAR(ba), SIMD_AS_CHAR(bb), SIMD_AS_CHAR(bc), SIMD_AS_CHAR(bd), SIMD_AS_CHAR(be), SIMD_AS_CHAR(bf), \
	SIMD_AS_CHAR(c0), SIMD_AS_CHAR(c1), SIMD_AS_CHAR(c2), SIMD_AS_CHAR(c3), SIMD_AS_CHAR(c4), SIMD_AS_CHAR(c5), SIMD_AS_CHAR(c6), SIMD_AS_CHAR(c7), \
	SIMD_AS_CHAR(c8), SIMD_AS_CHAR(c9), SIMD_AS_CHAR(ca), SIMD_AS_CHAR(cb), SIMD_AS_CHAR(cc), SIMD_AS_CHAR(cd), SIMD_AS_CHAR(ce), SIMD_AS_CHAR(cf), \
	SIMD_AS_CHAR(d0), SIMD_AS_CHAR(d1), SIMD_AS_CHAR(d2), SIMD_AS_CHAR(d3), SIMD_AS_CHAR(d4), SIMD_AS_CHAR(d5), SIMD_AS_CHAR(d6), SIMD_AS_CHAR(d7), \
	SIMD_AS_CHAR(d8), SIMD_AS_CHAR(d9), SIMD_AS_CHAR(da), SIMD_AS_CHAR(db), SIMD_AS_CHAR(dc), SIMD_AS_CHAR(dd), SIMD_AS_CHAR(de), SIMD_AS_CHAR(df)}

#define SIMD_MM512_SET1_EPI16(a) \
	{SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), \
	SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), \
	SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), \
	SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a), SIMD_AS_2CHARS(a)}

#define SIMD_MM512_SET2_EPI16(a0, a1) \
	{SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), \
	SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), \
	SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), \
	SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1)}

#define SIMD_MM512_SETR_EPI16(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, ba, bb, bc, bd, be, bf) \
    {SIMD_AS_2CHARS(a0), SIMD_AS_2CHARS(a1), SIMD_AS_2CHARS(a2), SIMD_AS_2CHARS(a3), SIMD_AS_2CHARS(a4), SIMD_AS_2CHARS(a5), SIMD_AS_2CHARS(a6), SIMD_AS_2CHARS(a7), \
    SIMD_AS_2CHARS(a8), SIMD_AS_2CHARS(a9), SIMD_AS_2CHARS(aa), SIMD_AS_2CHARS(ab), SIMD_AS_2CHARS(ac), SIMD_AS_2CHARS(ad), SIMD_AS_2CHARS(ae), SIMD_AS_2CHARS(af), \
	SIMD_AS_2CHARS(b0), SIMD_AS_2CHARS(b1), SIMD_AS_2CHARS(b2), SIMD_AS_2CHARS(b3), SIMD_AS_2CHARS(b4), SIMD_AS_2CHARS(b5), SIMD_AS_2CHARS(b6), SIMD_AS_2CHARS(b7), \
    SIMD_AS_2CHARS(b8), SIMD_AS_2CHARS(b9), SIMD_AS_2CHARS(ba), SIMD_AS_2CHARS(bb), SIMD_AS_2CHARS(bc), SIMD_AS_2CHARS(bd), SIMD_AS_2CHARS(be), SIMD_AS_2CHARS(bf)}

#define SIMD_MM512_SET1_EPI32(a) \
	{SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), \
	SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a), SIMD_AS_4CHARS(a)}

#define SIMD_MM512_SET2_EPI32(a0, a1) \
	{SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), \
	SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1)}

#define SIMD_MM512_SETR_EPI32(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af) \
    {SIMD_AS_4CHARS(a0), SIMD_AS_4CHARS(a1), SIMD_AS_4CHARS(a2), SIMD_AS_4CHARS(a3), SIMD_AS_4CHARS(a4), SIMD_AS_4CHARS(a5), SIMD_AS_4CHARS(a6), SIMD_AS_4CHARS(a7), \
	SIMD_AS_4CHARS(a8), SIMD_AS_4CHARS(a9), SIMD_AS_4CHARS(aa), SIMD_AS_4CHARS(ab), SIMD_AS_4CHARS(ac), SIMD_AS_4CHARS(ad), SIMD_AS_4CHARS(ae), SIMD_AS_4CHARS(af)}

#define SIMD_MM512_SET1_EPI64(a) \
	{SIMD_AS_8CHARS(a), SIMD_AS_8CHARS(a), SIMD_AS_8CHARS(a), SIMD_AS_8CHARS(a), SIMD_AS_8CHARS(a), SIMD_AS_8CHARS(a), SIMD_AS_8CHARS(a), SIMD_AS_8CHARS(a)}

#define SIMD_MM512_SET2_EPI64(a0, a1) \
	{SIMD_AS_8CHARS(a0), SIMD_AS_8CHARS(a1), SIMD_AS_8CHARS(a0), SIMD_AS_8CHARS(a1), SIMD_AS_8CHARS(a0), SIMD_AS_8CHARS(a1), SIMD_AS_8CHARS(a0), SIMD_AS_8CHARS(a1)}

#define SIMD_MM512_SETR_EPI64(a0, a1, a2, a3, a4, a5, a6, a7) \
    {SIMD_AS_8CHARS(a0), SIMD_AS_8CHARS(a1), SIMD_AS_8CHARS(a2), SIMD_AS_8CHARS(a3), SIMD_AS_8CHARS(a4), SIMD_AS_8CHARS(a5), SIMD_AS_8CHARS(a6), SIMD_AS_8CHARS(a7)}

#elif defined(SIMD_INIT_AS_LONGLONG)

#define SIMD_MM512_SET1_EPI8(a) \
    {SIMD_LL_SET1_EPI8(a), SIMD_LL_SET1_EPI8(a), SIMD_LL_SET1_EPI8(a), SIMD_LL_SET1_EPI8(a), \
    SIMD_LL_SET1_EPI8(a), SIMD_LL_SET1_EPI8(a), SIMD_LL_SET1_EPI8(a), SIMD_LL_SET1_EPI8(a)}

#define SIMD_MM512_SET2_EPI8(a0, a1) \
    {SIMD_LL_SET2_EPI8(a0, a1), SIMD_LL_SET2_EPI8(a0, a1), SIMD_LL_SET2_EPI8(a0, a1), SIMD_LL_SET2_EPI8(a0, a1), \
    SIMD_LL_SET2_EPI8(a0, a1), SIMD_LL_SET2_EPI8(a0, a1), SIMD_LL_SET2_EPI8(a0, a1), SIMD_LL_SET2_EPI8(a0, a1)}

#define SIMD_MM512_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, ba, bb, bc, bd, be, bf, c0, c1, c2, c3, c4, c5, c6, c7, c8, c9, ca, cb, cc, cd, ce, cf, d0, d1, d2, d3, d4, d5, d6, d7, d8, d9, da, db, dc, dd, de, df) \
    {SIMD_LL_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7), SIMD_LL_SETR_EPI8(a8, a9, aa, ab, ac, ad, ae, af), \
    SIMD_LL_SETR_EPI8(b0, b1, b2, b3, b4, b5, b6, b7), SIMD_LL_SETR_EPI8(b8, b9, ba, bb, bc, bd, be, bf), \
	SIMD_LL_SETR_EPI8(c0, c1, c2, c3, c4, c5, c6, c7), SIMD_LL_SETR_EPI8(c8, c9, ca, cb, cc, cd, ce, cf), \
    SIMD_LL_SETR_EPI8(d0, d1, d2, d3, d4, d5, d6, d7), SIMD_LL_SETR_EPI8(d8, d9, da, db, dc, dd, de, df)}

#define SIMD_MM512_SET1_EPI16(a) \
    {SIMD_LL_SET1_EPI16(a), SIMD_LL_SET1_EPI16(a), SIMD_LL_SET1_EPI16(a), SIMD_LL_SET1_EPI16(a), \
    SIMD_LL_SET1_EPI16(a), SIMD_LL_SET1_EPI16(a), SIMD_LL_SET1_EPI16(a), SIMD_LL_SET1_EPI16(a)}

#define SIMD_MM512_SET2_EPI16(a0, a1) \
    {SIMD_LL_SET2_EPI16(a0, a1), SIMD_LL_SET2_EPI16(a0, a1), SIMD_LL_SET2_EPI16(a0, a1), SIMD_LL_SET2_EPI16(a0, a1), \
    SIMD_LL_SET2_EPI16(a0, a1), SIMD_LL_SET2_EPI16(a0, a1), SIMD_LL_SET2_EPI16(a0, a1), SIMD_LL_SET2_EPI16(a0, a1)}

#define SIMD_MM512_SETR_EPI16(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af, b0, b1, b2, b3, b4, b5, b6, b7, b8, b9, ba, bb, bc, bd, be, bf) \
    {SIMD_LL_SETR_EPI16(a0, a1, a2, a3), SIMD_LL_SETR_EPI16(a4, a5, a6, a7), SIMD_LL_SETR_EPI16(a8, a9, aa, ab), SIMD_LL_SETR_EPI16(ac, ad, ae, af), \
	 SIMD_LL_SETR_EPI16(b0, b1, b2, b3), SIMD_LL_SETR_EPI16(b4, b5, b6, b7), SIMD_LL_SETR_EPI16(b8, b9, ba, bb), SIMD_LL_SETR_EPI16(bc, bd, be, bf)}

#define SIMD_MM512_SET1_EPI32(a) \
    {SIMD_LL_SET1_EPI32(a), SIMD_LL_SET1_EPI32(a), SIMD_LL_SET1_EPI32(a), SIMD_LL_SET1_EPI32(a), \
    SIMD_LL_SET1_EPI32(a), SIMD_LL_SET1_EPI32(a), SIMD_LL_SET1_EPI32(a), SIMD_LL_SET1_EPI32(a)}

#define SIMD_MM512_SET2_EPI32(a0, a1) \
    {SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a0, a1), \
    SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a0, a1)}

#define SIMD_MM512_SETR_EPI32(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af) \
    {SIMD_LL_SET2_EPI32(a0, a1), SIMD_LL_SET2_EPI32(a2, a3), SIMD_LL_SET2_EPI32(a4, a5), SIMD_LL_SET2_EPI32(a6, a7), \
    SIMD_LL_SET2_EPI32(a8, a9), SIMD_LL_SET2_EPI32(aa, ab), SIMD_LL_SET2_EPI32(ac, ad), SIMD_LL_SET2_EPI32(ae, af)}

#define SIMD_MM512_SET1_EPI64(a) \
    {a, a, a, a, a, a, a, a}

#define SIMD_MM512_SET2_EPI64(a0, a1) \
    {a0, a1, a0, a1, a0, a1, a0, a1}

#define SIMD_MM512_SETR_EPI64(a0, a1, a2, a3, a4, a5, a6, a7) \
    {a0, a1, a2, a3, a4, a5, a6, a7}

#endif// defined(_MSC_VER) || defined(__GNUC__)

#endif//defined(SIMD_AVX512BW_ENABLE)

#if defined(SIMD_NEON_ENABLE) && defined(__GNUC__)

#define SIMD_VEC_SET1_EPI8(a) \
    {a, a, a, a, a, a, a, a, a, a, a, a, a, a, a, a}

#define SIMD_VEC_SET2_EPI8(a0, a1) \
    {a0, a1, a0, a1, a0, a1, a0, a1, a0, a1, a0, a1, a0, a1, a0, a1}

#define SIMD_VEC_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af) \
    {a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af}

#define SIMD_VEC_SET1_EPI16(a) \
    {a, a, a, a, a, a, a, a}

#define SIMD_VEC_SET2_EPI16(a0, a1) \
    {a0, a1, a0, a1, a0, a1, a0, a1}

#define SIMD_VEC_SETR_EPI16(a0, a1, a2, a3, a4, a5, a6, a7) \
    {a0, a1, a2, a3, a4, a5, a6, a7}

#define SIMD_VEC_SET1_EPI32(a) \
    {a, a, a, a}

#define SIMD_VEC_SET2_EPI32(a0, a1) \
    {a0, a1, a0, a1}

#define SIMD_VEC_SETR_EPI32(a0, a1, a2, a3) \
    {a0, a1, a2, a3}

#define SIMD_VEC_SET1_EPI64(a) \
    {a, a}

#define SIMD_VEC_SET2_EPI64(a0, a1) \
    {a0, a1}

#define SIMD_VEC_SETR_EPI64(a0, a1) \
    {a0, a1}

#define SIMD_VEC_SET1_PS(a) \
    {a, a, a, a}

#define SIMD_VEC_SET2_PS(a0, a1) \
    {a0, a1, a0, a1}

#define SIMD_VEC_SETR_PS(a0, a1, a2, a3) \
    {a0, a1, a2, a3}

#define SIMD_VEC_SET1_PI8(a) \
    {a, a, a, a, a, a, a, a}

#define SIMD_VEC_SET2_PI8(a0, a1) \
    {a0, a1, a0, a1, a0, a1, a0, a1}

#define SIMD_VEC_SETR_PI8(a0, a1, a2, a3, a4, a5, a6, a7) \
    {a0, a1, a2, a3, a4, a5, a6, a7}

#define SIMD_VEC_SET1_PI16(a) \
    {a, a, a, a}

#define SIMD_VEC_SET2_PI16(a0, a1) \
    {a0, a1, a0, a1}

#define SIMD_VEC_SETR_PI16(a0, a1, a2, a3) \
    {a0, a1, a2, a3}

#define SIMD_VEC_SET1_PI32(a) \
    {a, a}

#define SIMD_VEC_SETR_PI32(a0, a1) \
    {a0, a1}

#define SIMD_VEC_SETR_PI64(a) \
    {a}

#endif//defined(SIMD_NEON_ENABLE) && defined(__GNUC__)

#if defined(_MSC_VER) && defined(SIMD_NEON_ENABLE)

#define SIMD_VEC_SET1_EPI8(a) \
    {SIMD_ULL_SET1_EPI8(a), SIMD_ULL_SET1_EPI8(a)}

#define SIMD_VEC_SET2_EPI8(a0, a1) \
    {SIMD_ULL_SET2_EPI8(a0, a1), SIMD_ULL_SET2_EPI8(a0, a1)}

#define SIMD_VEC_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7, a8, a9, aa, ab, ac, ad, ae, af) \
    {SIMD_ULL_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7), SIMD_ULL_SETR_EPI8(a8, a9, aa, ab, ac, ad, ae, af)}

#define SIMD_VEC_SET1_EPI16(a) \
    {SIMD_ULL_SET1_EPI16(a), SIMD_ULL_SET1_EPI16(a)}

#define SIMD_VEC_SET2_EPI16(a0, a1) \
    {SIMD_ULL_SET2_EPI16(a0, a1), SIMD_ULL_SET2_EPI16(a0, a1)}

#define SIMD_VEC_SETR_EPI16(a0, a1, a2, a3, a4, a5, a6, a7) \
    {SIMD_ULL_SETR_EPI16(a0, a1, a2, a3), SIMD_ULL_SETR_EPI16(a4, a5, a6, a7)}

#define SIMD_VEC_SET1_EPI32(a) \
    {SIMD_ULL_SET1_EPI32(a), SIMD_ULL_SET1_EPI32(a)}

#define SIMD_VEC_SET2_EPI32(a0, a1) \
    {SIMD_ULL_SET2_EPI32(a0, a1), SIMD_ULL_SET2_EPI32(a0, a1)}

#define SIMD_VEC_SETR_EPI32(a0, a1, a2, a3) \
    {SIMD_ULL_SET2_EPI32(a0, a1), SIMD_ULL_SET2_EPI32(a2, a3)}

#define SIMD_VEC_SET1_EPI64(a) \
    {a, a}

#define SIMD_VEC_SET2_EPI64(a0, a1) \
    {a0, a1}

#define SIMD_VEC_SETR_EPI64(a0, a1) \
    {a0, a1}

#define SIMD_VEC_SET1_PI8(a) \
    {SIMD_ULL_SET1_EPI8(a)}

#define SIMD_VEC_SET2_PI8(a0, a1) \
    {SIMD_ULL_SET2_EPI8(a0, a1)}

#define SIMD_VEC_SETR_PI8(a0, a1, a2, a3, a4, a5, a6, a7) \
    {SIMD_ULL_SETR_EPI8(a0, a1, a2, a3, a4, a5, a6, a7)}

#define SIMD_VEC_SET1_PI16(a) \
    {SIMD_ULL_SET1_EPI16(a)}

#define SIMD_VEC_SET2_PI16(a0, a1) \
    {SIMD_ULL_SET2_EPI16(a0, a1)}

#define SIMD_VEC_SETR_PI16(a0, a1, a2, a3) \
    {SIMD_ULL_SETR_EPI16(a0, a1, a2, a3)}

#define SIMD_VEC_SET1_PI32(a) \
    {SIMD_ULL_SET1_EPI32(a)}

#define SIMD_VEC_SETR_PI32(a0, a1, a2, a3) \
    {SIMD_ULL_SET2_EPI32(a0, a1)}

#define SIMD_VEC_SETR_PI64(a0) \
    {a0}

#endif//defined(_MSC_VER) && defined(SIMD_NEON_ENABLE)
}

#endif//__SimdInit_h__
