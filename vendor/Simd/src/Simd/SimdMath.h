/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2026 Yermalayeu Ihar,
*               2018-2019 Radchenko Andrey.
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
#ifndef __SimdMath_h__
#define __SimdMath_h__

#include "Simd/SimdDefs.h"
#include "Simd/SimdConst.h"

namespace Simd
{
    template <class T> SIMD_INLINE void Swap(T & a, T & b)
    {
        T t = a;
        a = b;
        b = t;
    }

    template <class T> SIMD_INLINE T Min(T a, T b)
    {
        return a < b ? a : b;
    }

    template <class T> SIMD_INLINE T Max(T a, T b)
    {
        return a > b ? a : b;
    }

    template <class T> SIMD_INLINE T Abs(T a)
    {
        return a < 0 ? -a : a;
    }

    template <class T> SIMD_INLINE T RestrictRange(T value, T min, T max)
    {
        return Max(min, Min(max, value));
    }

    template <class T> SIMD_INLINE T Square(T a)
    {
        return a*a;
    }

#ifndef SIMD_ROUND
#define SIMD_ROUND
    SIMD_INLINE int Round(double value)
    {
#if defined(SIMD_X64_ENABLE) && !defined(SIMD_SSE41_DISABLE)
        __m128d _value = _mm_set_sd(value);
        return _mm_cvtsd_si32(_value);
#else
        return (int)(value + (value >= 0.0 ? 0.5 : -0.5));
#endif
    }

    SIMD_INLINE int Round(float value)
    {
#if defined(SIMD_X64_ENABLE) && !defined(SIMD_SSE41_DISABLE)
        __m128 _value = _mm_set_ss(value);
        return _mm_cvtss_si32(_value);
#else
        return (int)(value + (value >= 0.0f ? 0.5f : -0.5f));
#endif
    }
#endif

    namespace Base
    {
        SIMD_INLINE float Not(float f)
        {
            int i = ~(int&)f;
            return (float&)i;
        }

        SIMD_INLINE int Min(int a, int b)
        {
            return a < b ? a : b;
        }

        SIMD_INLINE int Max(int a, int b)
        {
            return a > b ? a : b;
        }

        SIMD_INLINE int RestrictRange(int value, int min = 0, int max = 255)
        {
            return Max(min, Min(max, value));
        }

        SIMD_INLINE int Square(int a)
        {
            return a*a;
        }

        SIMD_INLINE int SquaredDifference(int a, int b)
        {
            return Square(a - b);
        }

        SIMD_INLINE int AbsDifference(int a, int b)
        {
            return a > b ? a - b : b - a;
        }

        SIMD_INLINE int Average(int a, int b)
        {
            return (a + b + 1) >> 1;
        }

        SIMD_INLINE int Average(int a, int b, int c, int d)
        {
            return (a + b + c + d + 2) >> 2;
        }

        SIMD_INLINE void SortU8(int & a, int & b)
        {
            int d = a - b;
            int m = ~(d >> 8);
            b += d & m;
            a -= d & m;
        }

        SIMD_INLINE int AbsDifferenceU8(int a, int b)
        {
            int d = a - b;
            int m = d >> 8;
            return (d & ~m) | (-d & m);
        }

        SIMD_INLINE int MaxU8(int a, int b)
        {
            int d = a - b;
            int m = ~(d >> 8);
            return b + (d & m);
        }

        SIMD_INLINE int MinU8(int a, int b)
        {
            int d = a - b;
            int m = ~(d >> 8);
            return a - (d & m);
        }

        SIMD_INLINE int SaturatedSubtractionU8(int a, int b)
        {
            int d = a - b;
            int m = ~(d >> 8);
            return (d & m);
        }

        template <bool compensation> SIMD_INLINE int DivideBy16(int value);

        template <> SIMD_INLINE int DivideBy16<true>(int value)
        {
            return (value + 8) >> 4;
        }

        template <> SIMD_INLINE int DivideBy16<false>(int value)
        {
            return value >> 4;
        }

        template <bool compensation> SIMD_INLINE int GaussianBlur3x3(const uint8_t *s0, const uint8_t *s1, const uint8_t *s2, size_t x0, size_t x1, size_t x2)
        {
            return DivideBy16<compensation>(s0[x0] + 2 * s0[x1] + s0[x2] + (s1[x0] + 2 * s1[x1] + s1[x2]) * 2 + s2[x0] + 2 * s2[x1] + s2[x2]);
        }

        SIMD_INLINE float DerivativeSigmoid(float function)
        {
            return (1.0f - function)*function;
        }

        SIMD_INLINE float DerivativeTanh(float function)
        {
            return (1.0f - function*function);
        }

        SIMD_INLINE void UpdateWeights(const float * x, size_t offset, float a, float b, float * d, float * w)
        {
            float _d = a*d[offset] + b*x[offset];
            d[offset] = _d;
            w[offset] += _d;
        }

        SIMD_INLINE void AdaptiveGradientUpdate(const float * delta, size_t offset, float norm, float alpha, float epsilon, float * gradient, float * weight)
        {
            float d = delta[offset] * norm;
            gradient[offset] += d*d;
            weight[offset] -= alpha * d / ::sqrt(gradient[offset] + epsilon);
        }
    }

#ifdef SIMD_SSE41_ENABLE
    namespace Sse41
    {
        SIMD_INLINE __m128 Abs(__m128 value)
        {
            return _mm_andnot_ps(_mm_set1_ps(-0.0f), value);
        }

        SIMD_INLINE __m128 Neg(__m128 value)
        {
            return _mm_sub_ps(_mm_setzero_ps(), value);
        }

        SIMD_INLINE __m128 Square(__m128 value)
        {
            return _mm_mul_ps(value, value);
        }

        SIMD_INLINE __m128 Not(__m128 value)
        {
            return _mm_andnot_ps(value, _mm_castsi128_ps(K_INV_ZERO));
        }

        template<bool fast> __m128 Rcp(__m128 value);

        template<> SIMD_INLINE __m128 Rcp<true>(__m128 value)
        {
            return _mm_rcp_ps(value);
        }

        template<> SIMD_INLINE __m128 Rcp<false>(__m128 value)
        {
            return _mm_div_ps(_mm_set1_ps(1.0f), value);
        }

        template<bool fast> __m128 Sqrt(__m128 value);

        template<> SIMD_INLINE __m128 Sqrt<false>(__m128 value)
        {
            return _mm_sqrt_ps(value);
        }

        template<> SIMD_INLINE __m128 Sqrt<true>(__m128 value)
        {
            return _mm_mul_ps(_mm_rsqrt_ps(_mm_max_ps(value, _mm_set1_ps(0.00000001f))), value);
        }

        SIMD_INLINE __m128i Combine(__m128i mask, __m128i positive, __m128i negative)
        {
            return _mm_blendv_epi8(negative, positive, mask);
        }

        SIMD_INLINE __m128 Combine(__m128 mask, __m128 positive, __m128 negative)
        {
            return _mm_blendv_ps(negative, positive, mask);
        }

        template <bool abs> __m128i ConditionalAbs(__m128i a);

        template <> SIMD_INLINE __m128i ConditionalAbs<true>(__m128i a)
        {
            return _mm_abs_epi16(a);
        }

        template <> SIMD_INLINE __m128i ConditionalAbs<false>(__m128i a)
        {
            return a;
        }

        template <bool condition> SIMD_INLINE __m128 Masked(const __m128& value, const __m128& mask);

        template <> SIMD_INLINE __m128 Masked<false>(const __m128& value, const __m128& mask)
        {
            return value;
        }

        template <> SIMD_INLINE __m128 Masked<true>(const __m128& value, const __m128& mask)
        {
            return _mm_and_ps(value, mask);
        }

        SIMD_INLINE void Max2x3s(const float* src, size_t stride, float* dst)
        {
            __m128 z = _mm_setzero_ps();
            __m128 s0 = _mm_loadl_pi(z, (__m64*)src);
            __m128 s1 = _mm_loadl_pi(z, (__m64*)(src + stride));
            __m128 s2 = _mm_loadl_pi(z, (__m64*)(src + 2 * stride));
            __m128 m = _mm_max_ps(_mm_max_ps(s0, s1), s2);
            return _mm_store_ss(dst, _mm_max_ss(m, _mm_shuffle_ps(m, m, 1)));
        }

        SIMD_INLINE void Max2x2s(const float* src, size_t stride, float* dst)
        {
            __m128 z = _mm_setzero_ps();
            __m128 s0 = _mm_loadl_pi(z, (__m64*)src);
            __m128 s1 = _mm_loadl_pi(z, (__m64*)(src + stride));
            __m128 m = _mm_max_ps(s0, s1);
            return _mm_store_ss(dst, _mm_max_ss(m, _mm_shuffle_ps(m, m, 1)));
        }

        SIMD_INLINE __m128i RightNotZero8i(ptrdiff_t count)
        {
            static const int8_t mask[DA] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
            return _mm_loadu_si128((__m128i*)(mask + Simd::RestrictRange<ptrdiff_t>(count, 0, A)));
        }

        SIMD_INLINE __m128i LeftNotZero8i(ptrdiff_t count)
        {
            static const int8_t mask[DA] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
            return _mm_loadu_si128((__m128i*)(mask + A - Simd::RestrictRange<ptrdiff_t>(count, 0, A)));
        }

        SIMD_INLINE __m128 RightNotZero32f(ptrdiff_t count)
        {
            const int32_t mask[DF] = { 0, 0, 0, 0, -1, -1, -1, -1 };
            return _mm_loadu_ps((float*)(mask + Simd::RestrictRange<ptrdiff_t>(count, 0, F)));
        }

        SIMD_INLINE __m128 LeftNotZero32f(ptrdiff_t count)
        {
            const int32_t mask[DF] = { -1, -1, -1, -1, 0, 0, 0, 0 };
            return _mm_loadu_ps((float*)(mask + F - Simd::RestrictRange<ptrdiff_t>(count, 0, F)));
        }

        SIMD_INLINE __m128i SaturateI16ToU8(__m128i value)
        {
            return _mm_min_epi16(K16_00FF, _mm_max_epi16(value, K_ZERO));
        }

        SIMD_INLINE __m128i MaxI16(__m128i a, __m128i b, __m128i c)
        {
            return _mm_max_epi16(a, _mm_max_epi16(b, c));
        }

        SIMD_INLINE __m128i MinI16(__m128i a, __m128i b, __m128i c)
        {
            return _mm_min_epi16(a, _mm_min_epi16(b, c));
        }

        SIMD_INLINE void SortU8(__m128i& a, __m128i& b)
        {
            __m128i t = a;
            a = _mm_min_epu8(t, b);
            b = _mm_max_epu8(t, b);
        }

        SIMD_INLINE __m128i ShiftLeft(__m128i a, size_t shift)
        {
            __m128i t = a;
            if (shift & 8)
                t = _mm_slli_si128(t, 8);
            if (shift & 4)
                t = _mm_slli_si128(t, 4);
            if (shift & 2)
                t = _mm_slli_si128(t, 2);
            if (shift & 1)
                t = _mm_slli_si128(t, 1);
            return t;
        }

        SIMD_INLINE __m128i ShiftRight(__m128i a, size_t shift)
        {
            __m128i t = a;
            if (shift & 8)
                t = _mm_srli_si128(t, 8);
            if (shift & 4)
                t = _mm_srli_si128(t, 4);
            if (shift & 2)
                t = _mm_srli_si128(t, 2);
            if (shift & 1)
                t = _mm_srli_si128(t, 1);
            return t;
        }

        template<int shift> SIMD_INLINE __m128 Alignr(const __m128 & s0, const __m128 & s4)
        {
            return _mm_castsi128_ps(_mm_alignr_epi8(_mm_castps_si128(s4), _mm_castps_si128(s0), shift * 4));
        }

        SIMD_INLINE __m128i HorizontalSum32(__m128i a)
        {
            return _mm_add_epi64(_mm_unpacklo_epi32(a, K_ZERO), _mm_unpackhi_epi32(a, K_ZERO));
        }

        SIMD_INLINE __m128i AbsDifferenceU8(__m128i a, __m128i b)
        {
            return _mm_sub_epi8(_mm_max_epu8(a, b), _mm_min_epu8(a, b));
        }

        SIMD_INLINE __m128i AbsDifferenceI16(__m128i a, __m128i b)
        {
            return _mm_sub_epi16(_mm_max_epi16(a, b), _mm_min_epi16(a, b));
        }

        SIMD_INLINE __m128i MulU8(__m128i a, __m128i b)
        {
            __m128i lo = _mm_mullo_epi16(_mm_unpacklo_epi8(a, K_ZERO), _mm_unpacklo_epi8(b, K_ZERO));
            __m128i hi = _mm_mullo_epi16(_mm_unpackhi_epi8(a, K_ZERO), _mm_unpackhi_epi8(b, K_ZERO));
            return _mm_packus_epi16(lo, hi);
        }

        SIMD_INLINE __m128i BinomialSum16(const __m128i& a, const __m128i& b, const __m128i& c)
        {
            return _mm_add_epi16(_mm_add_epi16(a, c), _mm_add_epi16(b, b));
        }

        SIMD_INLINE __m128i BinomialSum16(const __m128i& a, const __m128i& b, const __m128i& c, const __m128i& d)
        {
            return _mm_add_epi16(_mm_add_epi16(a, d), _mm_mullo_epi16(_mm_add_epi16(b, c), K16_0003));
        }

        template <int index> __m128i U8To16(__m128i a);

        template <> SIMD_INLINE __m128i U8To16<0>(__m128i a)
        {
            return _mm_and_si128(a, K16_00FF);
        }

        template <> SIMD_INLINE __m128i U8To16<1>(__m128i a)
        {
            return _mm_and_si128(_mm_srli_si128(a, 1), K16_00FF);
        }

        SIMD_INLINE __m128i DivideBy16(__m128i value)
        {
            return _mm_srli_epi16(_mm_add_epi16(value, K16_0008), 4);
        }

        template <int index> SIMD_INLINE __m128 Broadcast(__m128 a)
        {
            return _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(a), index * 0x55));
        }

        template<int imm> SIMD_INLINE __m128i Shuffle32i(__m128i lo, __m128i hi)
        {
            return _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(lo), _mm_castsi128_ps(hi), imm));
        }

        template<int imm> SIMD_INLINE __m128 Shuffle32f(__m128 a)
        {
            return _mm_castsi128_ps(_mm_shuffle_epi32(_mm_castps_si128(a), imm));
        }

        SIMD_INLINE __m128i Average16(const __m128i& a, const __m128i& b, const __m128i& c, const __m128i& d)
        {
            return _mm_srli_epi16(_mm_add_epi16(_mm_add_epi16(_mm_add_epi16(a, b), _mm_add_epi16(c, d)), K16_0002), 2);
        }

        SIMD_INLINE __m128i Merge16(const __m128i& even, __m128i odd)
        {
            return _mm_or_si128(_mm_slli_si128(odd, 1), even);
        }

        SIMD_INLINE int TestZ(__m128 value)
        {
            return _mm_testz_si128(_mm_castps_si128(value), K_INV_ZERO);
        }

        SIMD_INLINE int TestZ(__m128i value)
        {
            return _mm_testz_si128(value, K_INV_ZERO);
        }

        SIMD_INLINE void MinVal32f(__m128 src, float& dst)
        {
            src = _mm_min_ps(src, Shuffle32f<0x0E>(src));
            src = _mm_min_ss(src, Shuffle32f<0x01>(src));
            _mm_store_ss(&dst, src);
        }

        SIMD_INLINE int MaxVal32i(__m128i src)
        {
            src = _mm_max_epi32(src, _mm_shuffle_epi32(src, 0x0E));
            src = _mm_max_epi32(src, _mm_shuffle_epi32(src, 0x01));
            return _mm_extract_epi32(src, 0);
        }

        SIMD_INLINE int MinVal32i(__m128i src)
        {
            src = _mm_min_epi32(src, _mm_shuffle_epi32(src, 0x0E));
            src = _mm_min_epi32(src, _mm_shuffle_epi32(src, 0x01));
            return _mm_extract_epi32(src, 0);
        }
    }
#endif// SIMD_SSE41_ENABLE

#ifdef SIMD_AVX2_ENABLE
    namespace Avx2
    {
        SIMD_INLINE __m256 Square(__m256 value)
        {
            return _mm256_mul_ps(value, value);
        }

        template<bool fast> __m256 Rcp(__m256 value);

        template<> SIMD_INLINE __m256 Rcp<true>(__m256 value)
        {
            return _mm256_rcp_ps(value);
        }

        template<> SIMD_INLINE __m256 Rcp<false>(__m256 value)
        {
            return _mm256_div_ps(_mm256_set1_ps(1.0f), value);
        }

        template<bool fast> __m256 Sqrt(__m256 value);

        template<> SIMD_INLINE __m256 Sqrt<false>(__m256 value)
        {
            return _mm256_sqrt_ps(value);
        }

        template<> SIMD_INLINE __m256 Sqrt<true>(__m256 value)
        {
            return _mm256_mul_ps(_mm256_rsqrt_ps(_mm256_max_ps(value, _mm256_set1_ps(0.00000001f))), value);
        }

        SIMD_INLINE __m256 RightNotZero32f(ptrdiff_t count)
        {
            const int32_t mask[DF] = { 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1 };
            return _mm256_loadu_ps((float*)(mask + Simd::RestrictRange<ptrdiff_t>(count, 0, F)));
        }

        SIMD_INLINE __m256 LeftNotZero32f(ptrdiff_t count)
        {
            const int32_t mask[DF] = { -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0 };
            return _mm256_loadu_ps((float*)(mask + F - Simd::RestrictRange<ptrdiff_t>(count, 0, F)));
        }

        SIMD_INLINE __m256i RightNotZero32i(ptrdiff_t count)
        {
            const int32_t mask[DF] = { 0, 0, 0, 0, 0, 0, 0, 0, -1, -1, -1, -1, -1, -1, -1, -1 };
            return _mm256_loadu_si256((__m256i*)(mask + Simd::RestrictRange<ptrdiff_t>(count, 0, F)));
        }

        SIMD_INLINE __m256i LeftNotZero32i(ptrdiff_t count)
        {
            const int32_t mask[DF] = { -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0 };
            return _mm256_loadu_si256((__m256i*)(mask + F - Simd::RestrictRange<ptrdiff_t>(count, 0, F)));
        }

        SIMD_INLINE __m256 PermutedHorizontalAdd(__m256 a, __m256 b)
        {
            return _mm256_hadd_ps(_mm256_permute2f128_ps(a, b, 0x20), _mm256_permute2f128_ps(a, b, 0x31));
        }

        SIMD_INLINE void Add8ExtractedSums(const __m256* src, float* dst)
        {
            __m256 lo = PermutedHorizontalAdd(PermutedHorizontalAdd(src[0], src[1]), PermutedHorizontalAdd(src[2], src[3]));
            __m256 hi = PermutedHorizontalAdd(PermutedHorizontalAdd(src[4], src[5]), PermutedHorizontalAdd(src[6], src[7]));
            _mm256_storeu_ps(dst, _mm256_add_ps(_mm256_loadu_ps(dst), PermutedHorizontalAdd(lo, hi)));
        }

        template <bool condition> SIMD_INLINE __m256 Masked(const __m256& value, const __m256& mask);

        template <> SIMD_INLINE __m256 Masked<false>(const __m256& value, const __m256& mask)
        {
            return value;
        }

        template <> SIMD_INLINE __m256 Masked<true>(const __m256& value, const __m256& mask)
        {
            return _mm256_and_ps(value, mask);
        }

        SIMD_INLINE void MinVal32f(__m256 src, float& dst)
        {
            Sse41::MinVal32f(_mm_min_ps(_mm256_castps256_ps128(src), _mm256_extractf128_ps(src, 1)), dst);
        }

        SIMD_INLINE int MaxVal32i(__m256i src)
        {
            return Sse41::MaxVal32i(_mm_max_epi32(_mm256_castsi256_si128(src), _mm256_extracti128_si256(src, 1)));
        }

        SIMD_INLINE int MinVal32i(__m256i src)
        {
            return Sse41::MinVal32i(_mm_min_epi32(_mm256_castsi256_si128(src), _mm256_extracti128_si256(src, 1)));
        }

        SIMD_INLINE __m256 Not(__m256 value)
        {
            return _mm256_andnot_ps(value, _mm256_castsi256_ps(K_INV_ZERO));
        }

        SIMD_INLINE __m256i SaturateI16ToU8(__m256i value)
        {
            return _mm256_min_epi16(K16_00FF, _mm256_max_epi16(value, K_ZERO));
        }

        SIMD_INLINE __m256i MaxI16(__m256i a, __m256i b, __m256i c)
        {
            return _mm256_max_epi16(a, _mm256_max_epi16(b, c));
        }

        SIMD_INLINE __m256i MinI16(__m256i a, __m256i b, __m256i c)
        {
            return _mm256_min_epi16(a, _mm256_min_epi16(b, c));
        }

        SIMD_INLINE void SortU8(__m256i & a, __m256i & b)
        {
            __m256i t = a;
            a = _mm256_min_epu8(t, b);
            b = _mm256_max_epu8(t, b);
        }

        SIMD_INLINE __m256i HorizontalSum32(__m256i a)
        {
            return _mm256_add_epi64(_mm256_unpacklo_epi32(a, K_ZERO), _mm256_unpackhi_epi32(a, K_ZERO));
        }

        SIMD_INLINE __m256i AbsDifferenceU8(__m256i a, __m256i b)
        {
            return _mm256_sub_epi8(_mm256_max_epu8(a, b), _mm256_min_epu8(a, b));
        }

        SIMD_INLINE __m256i AbsDifferenceI16(__m256i a, __m256i b)
        {
            return _mm256_sub_epi16(_mm256_max_epi16(a, b), _mm256_min_epi16(a, b));
        }

        SIMD_INLINE __m256i MulU8(__m256i a, __m256i b)
        {
            __m256i lo = _mm256_mullo_epi16(_mm256_unpacklo_epi8(a, K_ZERO), _mm256_unpacklo_epi8(b, K_ZERO));
            __m256i hi = _mm256_mullo_epi16(_mm256_unpackhi_epi8(a, K_ZERO), _mm256_unpackhi_epi8(b, K_ZERO));
            return _mm256_packus_epi16(lo, hi);
        }

        SIMD_INLINE __m256i BinomialSum16(const __m256i & a, const __m256i & b, const __m256i & c)
        {
            return _mm256_add_epi16(_mm256_add_epi16(a, c), _mm256_add_epi16(b, b));
        }

        template <bool abs> __m256i ConditionalAbs(__m256i a);

        template <> SIMD_INLINE __m256i ConditionalAbs<true>(__m256i a)
        {
            return _mm256_abs_epi16(a);
        }

        template <> SIMD_INLINE __m256i ConditionalAbs<false>(__m256i a)
        {
            return a;
        }

        template <int index> __m256i U8To16(__m256i a);

        template <> SIMD_INLINE __m256i U8To16<0>(__m256i a)
        {
            return _mm256_and_si256(a, K16_00FF);
        }

        template <> SIMD_INLINE __m256i U8To16<1>(__m256i a)
        {
            return _mm256_and_si256(_mm256_srli_si256(a, 1), K16_00FF);
        }

        template<int shift> SIMD_INLINE __m256 Alignr(const __m256 & s0, const __m256 & s4)
        {
            return _mm256_castsi256_ps(_mm256_alignr_epi8(_mm256_castps_si256(s4), _mm256_castps_si256(s0), shift * 4));
        }

        template<int imm> SIMD_INLINE __m256i Shuffle32i(__m256i lo, __m256i hi)
        {
            return _mm256_castps_si256(_mm256_shuffle_ps(_mm256_castsi256_ps(lo), _mm256_castsi256_ps(hi), imm));
        }

        template<int imm> SIMD_INLINE __m256i Shuffle64i(__m256i lo, __m256i hi)
        {
            return _mm256_castpd_si256(_mm256_shuffle_pd(_mm256_castsi256_pd(lo), _mm256_castsi256_pd(hi), imm));
        }

        template<int imm> SIMD_INLINE __m256 Permute4x64(__m256 a)
        {
            return _mm256_castsi256_ps(_mm256_permute4x64_epi64(_mm256_castps_si256(a), imm));
        }

        template<int imm> SIMD_INLINE __m256 Shuffle32f(__m256 a)
        {
            return _mm256_castsi256_ps(_mm256_shuffle_epi32(_mm256_castps_si256(a), imm));
        }

        template <int index> SIMD_INLINE __m256 Broadcast(__m256 a)
        {
            return _mm256_castsi256_ps(_mm256_shuffle_epi32(_mm256_castps_si256(a), index * 0x55));
        }

        SIMD_INLINE __m256i Average16(const __m256i & a, const __m256i & b, const __m256i & c, const __m256i & d)
        {
            return _mm256_srli_epi16(_mm256_add_epi16(_mm256_add_epi16(_mm256_add_epi16(a, b), _mm256_add_epi16(c, d)), K16_0002), 2);
        }

        SIMD_INLINE __m256i Merge16(const __m256i & even, __m256i odd)
        {
            return _mm256_or_si256(_mm256_slli_si256(odd, 1), even);
        }

        SIMD_INLINE const __m256i Shuffle(const __m256i & value, const __m256i & shuffle)
        {
            return _mm256_or_si256(_mm256_shuffle_epi8(value, _mm256_add_epi8(shuffle, K8_SHUFFLE_0)),
                _mm256_shuffle_epi8(_mm256_permute4x64_epi64(value, 0x4E), _mm256_add_epi8(shuffle, K8_SHUFFLE_1)));
        }

        template <int part> SIMD_INLINE __m256i Cvt8uTo16i(__m256i a)
        {
            return _mm256_cvtepu8_epi16(_mm256_extractf128_si256(a, part));
        }

        template <int part> SIMD_INLINE __m256i Cvt8iTo16i(__m256i a)
        {
            return _mm256_cvtepi8_epi16(_mm256_extractf128_si256(a, part));
        }

        SIMD_INLINE __m256i PermutedHadd16i(__m256i a, __m256i b)
        {
            return _mm256_permute4x64_epi64(_mm256_hadd_epi16(a, b), 0xD8);
        }

        SIMD_INLINE __m256i PermutedHadd32i(__m256i a, __m256i b)
        {
            return _mm256_permute4x64_epi64(_mm256_hadd_epi32(a, b), 0xD8);
        }
    }
#endif// SIMD_AVX2_ENABLE

#ifdef SIMD_AVX512BW_ENABLE
    namespace Avx512bw
    {
        SIMD_INLINE __mmask8 TailMask8(ptrdiff_t tail)
        {
            return tail <= 0 ? __mmask8(0) : (tail >= 8 ? __mmask8(-1) : __mmask8(-1) >> (8 - tail));
        }

        SIMD_INLINE __mmask8 NoseMask8(ptrdiff_t nose)
        {
            return nose <= 0 ? __mmask8(0) : (nose >= 8 ? __mmask8(-1) : __mmask8(-1) << (8 - nose));
        }

        SIMD_INLINE __mmask16 TailMask16(ptrdiff_t tail)
        {
            return tail <= 0 ? __mmask16(0) : (tail >= 16 ? __mmask16(-1) : __mmask16(-1) >> (16 - tail));
        }

        SIMD_INLINE __mmask16 NoseMask16(ptrdiff_t nose)
        {
            return nose <= 0 ? __mmask16(0) : (nose >= 16 ? __mmask16(-1) : __mmask16(-1) << (16 - nose));
        }

        SIMD_INLINE __m512 Cast(const __m512i& value)
        {
#if defined(__clang__)
            return (__m512)value;
#else
            return _mm512_castsi512_ps(value);
#endif
        }

        SIMD_INLINE __m512i Cast(const __m512& value)
        {
#if defined(__clang__)
            return (__m512i)value;
#else
            return _mm512_castps_si512(value);
#endif
        }

        SIMD_INLINE __m512 Square(__m512 value)
        {
            return _mm512_mul_ps(value, value);
        }

        SIMD_INLINE __m512 Or(const __m512& a, const __m512& b)
        {
#if defined(__clang__)
            return (__m512)_mm512_or_epi32((__m512i)a, (__m512i)b);
#else
            return _mm512_castsi512_ps(_mm512_or_epi32(_mm512_castps_si512(a), _mm512_castps_si512(b)));
#endif
        }

        SIMD_INLINE __m512 And(const __m512& a, const __m512& b)
        {
#if defined(__clang__)
            return (__m512)_mm512_and_epi32((__m512i)a, (__m512i)b);
#else
            return _mm512_castsi512_ps(_mm512_and_epi32(_mm512_castps_si512(a), _mm512_castps_si512(b)));
#endif
        }

        SIMD_INLINE __m512 AndMaskZ(const __m512& a, const __m512& b, __mmask16 m)
        {
#if defined(__clang__)
            return (__m512)_mm512_maskz_and_epi32(m, (__m512i)a, (__m512i)b);
#else
            return _mm512_castsi512_ps(_mm512_maskz_and_epi32(m, _mm512_castps_si512(a), _mm512_castps_si512(b)));
#endif
        }

        SIMD_INLINE __m512 AndNot(const __m512& a, const __m512& b)
        {
#if defined(__clang__)
            return (__m512)_mm512_andnot_epi32((__m512i)a, (__m512i)b);
#else
            return _mm512_castsi512_ps(_mm512_andnot_epi32(_mm512_castps_si512(a), _mm512_castps_si512(b)));
#endif
        }

        SIMD_INLINE __m512 Not(__m512 value)
        {
            return _mm512_andnot_ps(value, _mm512_castsi512_ps(K_INV_ZERO));
        }

        SIMD_INLINE __m512 AndNotMaskZ(const __m512& a, const __m512& b, __mmask16 m)
        {
#if defined(__clang__)
            return (__m512)_mm512_maskz_andnot_epi32(m, (__m512i)a, (__m512i)b);
#else
            return _mm512_castsi512_ps(_mm512_maskz_andnot_epi32(m, _mm512_castps_si512(a), _mm512_castps_si512(b)));
#endif
        }

        SIMD_INLINE __m512 Xor(const __m512& a, const __m512& b)
        {
#if defined(__clang__)
            return (__m512)_mm512_xor_epi32((__m512i)a, (__m512i)b);
#else
            return _mm512_castsi512_ps(_mm512_xor_epi32(_mm512_castps_si512(a), _mm512_castps_si512(b)));
#endif
        }

        template<bool fast> __m512 Rcp(__m512 value);

        template<> SIMD_INLINE __m512 Rcp<true>(__m512 value)
        {
            return _mm512_rcp14_ps(value);
        }

        template<> SIMD_INLINE __m512 Rcp<false>(__m512 value)
        {
            return _mm512_div_ps(_mm512_set1_ps(1.0f), value);
        }

        SIMD_INLINE __m512 Rcp14(const __m512& a)
        {
#if defined(_MSC_VER) && _MSC_VER<1922
            return _mm512_maskz_rcp14_ps(_MM_K0_REG, a);
#else
            return _mm512_rcp14_ps(a);
#endif
        }

        SIMD_INLINE __m512 Rsqrt14(const __m512& a)
        {
#if defined(_MSC_VER) && _MSC_VER<1922
            return _mm512_maskz_rsqrt14_ps(_MM_K0_REG, a);
#else
            return _mm512_rsqrt14_ps(a);
#endif
        }

        template<bool mask> SIMD_INLINE __m512 Mask(__m512 a, __mmask16 m);

        template<> SIMD_INLINE __m512 Mask<true>(__m512 a, __mmask16 m)
        {
            return _mm512_maskz_mov_ps(m, a);
        }

        template<> SIMD_INLINE __m512 Mask<false>(__m512 a, __mmask16 m)
        {
            return a;
        }

        template<int shift> SIMD_INLINE __m512 Alignr(const __m512& lo, const __m512& hi)
        {
            return Cast(_mm512_alignr_epi32(Cast(hi), Cast(lo), shift));
        }

        template<> SIMD_INLINE __m512 Alignr<0>(const __m512& lo, const __m512& hi)
        {
            return lo;
        }

        template<> SIMD_INLINE __m512 Alignr<F>(const __m512& lo, const __m512& hi)
        {
            return hi;
        }


        template<int shift> SIMD_INLINE __m512i Alignr(const __m512i& lo, const __m512i& hi)
        {
            return _mm512_alignr_epi32(hi, lo, shift);
        }

        template<> SIMD_INLINE __m512i Alignr<0>(const __m512i& lo, const __m512i& hi)
        {
            return lo;
        }

        template<> SIMD_INLINE __m512i Alignr<F>(const __m512i& lo, const __m512i& hi)
        {
            return hi;
        }

        template<int shift, bool mask> SIMD_INLINE __m512 Alignr(const __m512& lo, const __m512& hi, __mmask16 m)
        {
            return Mask<mask>(Alignr<shift>(lo, hi), m);
        }

        SIMD_INLINE __mmask32 TailMask32(ptrdiff_t tail)
        {
            return tail <= 0 ? __mmask32(0) : (tail >= 32 ? __mmask32(-1) : __mmask32(-1) >> (32 - tail));
        }

        SIMD_INLINE __mmask32 NoseMask32(ptrdiff_t nose)
        {
            return nose <= 0 ? __mmask32(0) : (nose >= 32 ? __mmask32(-1) : __mmask32(-1) << (32 - nose));
        }

        SIMD_INLINE __mmask64 TailMask64(ptrdiff_t tail)
        {
            return tail <= 0 ? __mmask64(0) : (tail >= 64 ? __mmask64(-1) : __mmask64(-1) >> (64 - tail));
        }

        SIMD_INLINE __mmask64 NoseMask64(ptrdiff_t nose)
        {
            return nose <= 0 ? __mmask64(0) : (nose >= 64 ? __mmask64(-1) : __mmask64(-1) << (64 - nose));
        }

#if defined(_MSC_VER) || (defined(__GNUC__) && defined(__LZCNT__))
        SIMD_INLINE size_t FirstNotZero64(__mmask64 mask)
        {
#ifdef SIMD_X64_ENABLE
            return _tzcnt_u64(mask);
#else
            return (__mmask32(mask) ? _tzcnt_u32(__mmask32(mask)) : _tzcnt_u32(__mmask32(mask >> 32)) + 32);
#endif
        }

        SIMD_INLINE size_t LastNotZero64(__mmask64 mask)
        {
#ifdef SIMD_X64_ENABLE
            return 64 - _lzcnt_u64(mask);
#else
            return 64 - (__mmask32(mask >> 32) ? _lzcnt_u32(__mmask32(mask >> 32)) : _lzcnt_u32(__mmask32(mask)) + 32);
#endif
        }
#endif

#if defined(_MSC_VER) || (defined(__GNUC__) && defined(__POPCNT__))
        SIMD_INLINE size_t Popcnt64(__mmask64 mask)
        {
#ifdef SIMD_X64_ENABLE
            return _mm_popcnt_u64(mask);
#else
            return _mm_popcnt_u32(__mmask32(mask)) + _mm_popcnt_u32(__mmask32(mask >> 32));
#endif
        }
#endif

        SIMD_INLINE void SortU8(__m512i & a, __m512i & b)
        {
#if 0
            __m512i t = a;
            a = _mm512_min_epu8(t, b);
            b = _mm512_max_epu8(t, b);
#else
            __m512i d = _mm512_subs_epu8(a, b);
            a = _mm512_sub_epi8(a, d);
            b = _mm512_add_epi8(b, d);
#endif
        }

        SIMD_INLINE __m512i BinomialSum16(const __m512i & a, const __m512i & b, const __m512i & c)
        {
            return _mm512_add_epi16(_mm512_add_epi16(a, c), _mm512_add_epi16(b, b));
        }

        template <int index> __m512i U8To16(__m512i a);

        template <> SIMD_INLINE __m512i U8To16<0>(__m512i a)
        {
            return _mm512_and_si512(a, K16_00FF);
        }

        template <> SIMD_INLINE __m512i U8To16<1>(__m512i a)
        {
            return _mm512_shuffle_epi8(a, K8_SUFFLE_BGRA_TO_G0A0);
        }

        SIMD_INLINE __m512i AbsDifferenceU8(__m512i a, __m512i b)
        {
            return _mm512_sub_epi8(_mm512_max_epu8(a, b), _mm512_min_epu8(a, b));
        }

        SIMD_INLINE __m512i AbsDifferenceI16(__m512i a, __m512i b)
        {
            return _mm512_sub_epi16(_mm512_max_epi16(a, b), _mm512_min_epi16(a, b));
        }

        SIMD_INLINE __m512i Saturate16iTo8u(__m512i value)
        {
            return _mm512_min_epi16(K16_00FF, _mm512_max_epi16(value, K_ZERO));
        }

        SIMD_INLINE __m512i Hadd16(__m512i a, __m512i b)
        {
            __m512i ab0 = _mm512_permutex2var_epi16(a, K16_PERMUTE_FOR_HADD_0, b);
            __m512i ab1 = _mm512_permutex2var_epi16(a, K16_PERMUTE_FOR_HADD_1, b);
            return _mm512_add_epi16(ab0, ab1);
        }

        SIMD_INLINE __m512i Hadd32(__m512i a, __m512i b)
        {
            __m512i ab0 = _mm512_permutex2var_epi32(a, K32_DEINTERLEAVE_0, b);
            __m512i ab1 = _mm512_permutex2var_epi32(a, K32_DEINTERLEAVE_1, b);
            return _mm512_add_epi32(ab0, ab1);
        }

        SIMD_INLINE __m512i Permuted2Pack16iTo8u(__m512i lo, __m512i hi)
        {
            return _mm512_permutexvar_epi32(K32_PERMUTE_FOR_TWO_UNPACK, _mm512_packus_epi16(lo, hi));
        }

        template <bool abs> __m512i ConditionalAbs(__m512i a);

        template <> SIMD_INLINE __m512i ConditionalAbs<true>(__m512i a)
        {
            return _mm512_abs_epi16(a);
        }

        template <> SIMD_INLINE __m512i ConditionalAbs<false>(__m512i a)
        {
            return a;
        }

        SIMD_INLINE __m512i HorizontalSum32(__m512i a)
        {
            return _mm512_add_epi64(_mm512_unpacklo_epi32(a, K_ZERO), _mm512_unpackhi_epi32(a, K_ZERO));
        }

        SIMD_INLINE __m512i SaturateI16ToU8(__m512i value)
        {
            return _mm512_min_epi16(K16_00FF, _mm512_max_epi16(value, K_ZERO));
        }

        SIMD_INLINE __m512i MaxI16(const __m512i a, __m512i b, __m512i c)
        {
            return _mm512_max_epi16(a, _mm512_max_epi16(b, c));
        }

        SIMD_INLINE __m512i MinI16(__m512i a, __m512i b, __m512i c)
        {
            return _mm512_min_epi16(a, _mm512_min_epi16(b, c));
        }

        template<int imm> SIMD_INLINE __m512i Shuffle32i(__m512i lo, __m512i hi)
        {
            return _mm512_castps_si512(_mm512_shuffle_ps(_mm512_castsi512_ps(lo), _mm512_castsi512_ps(hi), imm));
        }

        template<int imm> SIMD_INLINE __m512i Shuffle64i(__m512i lo, __m512i hi)
        {
            return _mm512_castpd_si512(_mm512_shuffle_pd(_mm512_castsi512_pd(lo), _mm512_castsi512_pd(hi), imm));
        }

        template <int index> SIMD_INLINE __m512 Broadcast(__m512 a)
        {
            return _mm512_permute_ps(a, index * 0x55);
        }

        template <int imm> SIMD_INLINE __m512 Shuffle2x(__m512 a)
        {
            return _mm512_castsi512_ps(_mm512_permutex_epi64(_mm512_castps_si512(a), imm));
        }

        SIMD_INLINE __m512i Average16(const __m512i & a, const __m512i & b)
        {
            return _mm512_avg_epu16(a, b);
        }

        SIMD_INLINE __m512i Average16(const __m512i & a, const __m512i & b, const __m512i & c, const __m512i & d)
        {
            return _mm512_srli_epi16(_mm512_add_epi16(_mm512_add_epi16(_mm512_add_epi16(a, b), _mm512_add_epi16(c, d)), K16_0002), 2);
        }

        SIMD_INLINE __m512i Merge16(const __m512i & even, __m512i odd)
        {
            return _mm512_or_si512(_mm512_slli_epi16(odd, 8), even);
        }

        template <int part> SIMD_INLINE __m512i Cvt8uTo16i(__m512i a)
        {
            return _mm512_cvtepu8_epi16(_mm512_extracti64x4_epi64(a, part));
        }

        template <int part> SIMD_INLINE __m512i Cvt8iTo16i(__m512i a)
        {
            return _mm512_cvtepi8_epi16(_mm512_extracti64x4_epi64(a, part));
        }

        SIMD_INLINE void MinVal32f(__m512 src, float& dst)
        {
            Avx2::MinVal32f(_mm256_min_ps(_mm512_extractf32x8_ps(src, 0), _mm512_extractf32x8_ps(src, 1)), dst);
        }
    }
#endif //SIMD_AVX512BW_ENABLE

#ifdef SIMD_NEON_ENABLE
    namespace Neon
    {
        SIMD_INLINE uint8x16_t ShiftLeft(uint8x16_t value, size_t shift)
        {
            if (shift & 8)
                value = vextq_u8(K8_00, value, 8);
            if (shift & 4)
                value = vextq_u8(K8_00, value, 12);
            if (shift & 2)
                value = vextq_u8(K8_00, value, 14);
            if (shift & 1)
                value = vextq_u8(K8_00, value, 15);
            return value;
        }

        SIMD_INLINE uint8x16_t ShiftRight(uint8x16_t value, size_t shift)
        {
            if (shift & 8)
                value = vextq_u8(value, K8_00, 8);
            if (shift & 4)
                value = vextq_u8(value, K8_00, 4);
            if (shift & 2)
                value = vextq_u8(value, K8_00, 2);
            if (shift & 1)
                value = vextq_u8(value, K8_00, 1);
            return value;
        }

        SIMD_INLINE void SortU8(uint8x16_t & a, uint8x16_t & b)
        {
            uint8x16_t t = a;
            a = vminq_u8(t, b);
            b = vmaxq_u8(t, b);
        }

        SIMD_INLINE uint16x8_t DivideI16By255(uint16x8_t value)
        {
            return vshrq_n_u16(vaddq_u16(vaddq_u16(value, K16_0001), vshrq_n_u16(value, 8)), 8);
        }

        SIMD_INLINE uint16x8_t BinomialSum16(const uint16x8_t & a, const uint16x8_t & b, const uint16x8_t & c)
        {
            return vaddq_u16(vaddq_u16(a, c), vaddq_u16(b, b));
        }

        SIMD_INLINE int16x8_t BinomialSum(const int16x8_t & a, const int16x8_t & b, const int16x8_t & c)
        {
            return vaddq_s16(vaddq_s16(a, c), vaddq_s16(b, b));
        }

        SIMD_INLINE uint16x8_t BinomialSum16(const uint16x8_t & a, const uint16x8_t & b, const uint16x8_t & c, const uint16x8_t & d)
        {
            return vaddq_u16(vaddq_u16(a, d), vmulq_u16(vaddq_u16(b, c), K16_0003));
        }

        SIMD_INLINE uint16x8_t DivideBy16(uint16x8_t value)
        {
            return vshrq_n_u16(vaddq_u16(value, K16_0008), 4);
        }

        template <int part> SIMD_INLINE uint8x8_t Half(uint8x16_t a);

        template <> SIMD_INLINE uint8x8_t Half<0>(uint8x16_t a)
        {
            return vget_low_u8(a);
        }

        template <> SIMD_INLINE uint8x8_t Half<1>(uint8x16_t a)
        {
            return vget_high_u8(a);
        }

#if !defined(_MSC_VER)
        template <int part> SIMD_INLINE int8x8_t Half(int8x16_t a);

        template <> SIMD_INLINE int8x8_t Half<0>(int8x16_t a)
        {
            return vget_low_s8(a);
        }

        template <> SIMD_INLINE int8x8_t Half<1>(int8x16_t a)
        {
            return vget_high_s8(a);
        }

        template <int part> SIMD_INLINE uint16x4_t Half(uint16x8_t a);

        template <> SIMD_INLINE uint16x4_t Half<0>(uint16x8_t a)
        {
            return vget_low_u16(a);
        }

        template <> SIMD_INLINE uint16x4_t Half<1>(uint16x8_t a)
        {
            return vget_high_u16(a);
        }

        template <int part> SIMD_INLINE int16x4_t Half(int16x8_t a);

        template <> SIMD_INLINE int16x4_t Half<0>(int16x8_t a)
        {
            return vget_low_s16(a);
        }

        template <> SIMD_INLINE int16x4_t Half<1>(int16x8_t a)
        {
            return vget_high_s16(a);
        }

        template <int part> SIMD_INLINE uint32x2_t Half(uint32x4_t a);

        template <> SIMD_INLINE uint32x2_t Half<0>(uint32x4_t a)
        {
            return vget_low_u32(a);
        }

        template <> SIMD_INLINE uint32x2_t Half<1>(uint32x4_t a)
        {
            return vget_high_u32(a);
        }

        template <int part> SIMD_INLINE int32x2_t Half(int32x4_t a);

        template <> SIMD_INLINE int32x2_t Half<0>(int32x4_t a)
        {
            return vget_low_s32(a);
        }

        template <> SIMD_INLINE int32x2_t Half<1>(int32x4_t a)
        {
            return vget_high_s32(a);
        }

        template <int part> SIMD_INLINE float32x2_t Half(float32x4_t a);

        template <> SIMD_INLINE float32x2_t Half<0>(float32x4_t a)
        {
            return vget_low_f32(a);
        }

        template <> SIMD_INLINE float32x2_t Half<1>(float32x4_t a)
        {
            return vget_high_f32(a);
        }
#endif

        template <int part> SIMD_INLINE uint16x8_t UnpackU8(uint8x16_t a)
        {
            return vmovl_u8(Half<part>(a));
        }

        template <int part> SIMD_INLINE int16x8_t UnpackU8s(uint8x16_t a)
        {
            return (int16x8_t)vmovl_u8(Half<part>(a));
        }

        template <int part> SIMD_INLINE int16x8_t UnpackI8(int8x16_t a)
        {
            return vmovl_s8(Half<part>(a));
        }

        template <int part> SIMD_INLINE uint32x4_t UnpackU16(uint16x8_t a)
        {
            return vmovl_u16(Half<part>(a));
        }

        template <int part> SIMD_INLINE int32x4_t UnpackI16(int16x8_t a)
        {
            return vmovl_s16(Half<part>(a));
        }

        SIMD_INLINE uint8x16_t PackU16(uint16x8_t lo, uint16x8_t hi)
        {
            return vcombine_u8(vmovn_u16(lo), vmovn_u16(hi));
        }

        SIMD_INLINE uint8x16_t PackSaturatedI16(int16x8_t lo, int16x8_t hi)
        {
            return vcombine_u8(vqmovun_s16(lo), vqmovun_s16(hi));
        }

        SIMD_INLINE uint8x16_t PackSaturatedU16(uint16x8_t lo, uint16x8_t hi)
        {
            return vcombine_u8(vqmovn_u16(lo), vqmovn_u16(hi));
        }

        SIMD_INLINE uint16x8_t PackU32(uint32x4_t lo, uint32x4_t hi)
        {
            return vcombine_u16(vmovn_u32(lo), vmovn_u32(hi));
        }

        SIMD_INLINE int16x8_t PackI32(int32x4_t lo, int32x4_t hi)
        {
            return vcombine_s16(vmovn_s32(lo), vmovn_s32(hi));
        }

        SIMD_INLINE uint8x8x2_t Deinterleave(uint8x16_t value)
        {
            uint8_t buffer[A];
            vst1q_u8(buffer, value);
            return vld2_u8(buffer);
        }

        template <int part> SIMD_INLINE uint8x16_t Stretch2(uint8x16_t a)
        {
            return (uint8x16_t)vmulq_u16(UnpackU8<part>(a), K16_0101);
        }

        template <bool abs> int16x8_t ConditionalAbs(int16x8_t a);

        template <> SIMD_INLINE int16x8_t ConditionalAbs<true>(int16x8_t a)
        {
            return vabdq_s16(a, (int16x8_t)K16_0000);
        }

        template <> SIMD_INLINE int16x8_t ConditionalAbs<false>(int16x8_t a)
        {
            return a;
        }

        SIMD_INLINE int16x8_t SaturateByU8(int16x8_t a)
        {
            return (int16x8_t)vmovl_u8(vqmovun_s16(a));
        }

        template <int iter> SIMD_INLINE float32x4_t Reciprocal(const float32x4_t & a);

        template <> SIMD_INLINE float32x4_t Reciprocal<-1>(const float32x4_t & a)
        {
            float _a[4];
            vst1q_f32(_a, a);
            float r[4] = { 1.0f / _a[0], 1.0f / _a[1], 1.0f / _a[2], 1.0f / _a[3] };
            return vld1q_f32(r);
        };

        template<> SIMD_INLINE float32x4_t Reciprocal<0>(const float32x4_t & a)
        {
            return vrecpeq_f32(a);
        }

        template<> SIMD_INLINE float32x4_t Reciprocal<1>(const float32x4_t & a)
        {
            float32x4_t r = vrecpeq_f32(a);
            return vmulq_f32(vrecpsq_f32(a, r), r);
        }

        template<> SIMD_INLINE float32x4_t Reciprocal<2>(const float32x4_t & a)
        {
            float32x4_t r = vrecpeq_f32(a);
            r = vmulq_f32(vrecpsq_f32(a, r), r);
            return vmulq_f32(vrecpsq_f32(a, r), r);
        }

        template <int iter> SIMD_INLINE float32x4_t Div(const float32x4_t & a, const float32x4_t & b)
        {
            return vmulq_f32(a, Reciprocal<iter>(b));
        }

        template <> SIMD_INLINE float32x4_t Div<-1>(const float32x4_t & a, const float32x4_t & b)
        {
            float _a[4], _b[4];
            vst1q_f32(_a, a);
            vst1q_f32(_b, b);
            float c[4] = { _a[0] / _b[0], _a[1] / _b[1], _a[2] / _b[2], _a[3] / _b[3] };
            return vld1q_f32(c);
        };

        template <int iter> SIMD_INLINE float32x4_t ReciprocalSqrt(const float32x4_t & a);

        template <> SIMD_INLINE float32x4_t ReciprocalSqrt<-1>(const float32x4_t & a)
        {
            float _a[4];
            vst1q_f32(_a, a);
            float r[4] = { 1.0f / ::sqrtf(_a[0]), 1.0f / ::sqrtf(_a[1]), 1.0f / ::sqrtf(_a[2]), 1.0f / ::sqrtf(_a[3]) };
            return vld1q_f32(r);
        }

        template<> SIMD_INLINE float32x4_t ReciprocalSqrt<0>(const float32x4_t & a)
        {
            return vrsqrteq_f32(a);
        }

        template<> SIMD_INLINE float32x4_t ReciprocalSqrt<1>(const float32x4_t & a)
        {
            float32x4_t e = vrsqrteq_f32(a);
            return vmulq_f32(vrsqrtsq_f32(vmulq_f32(e, e), a), e);
        }

        template<> SIMD_INLINE float32x4_t ReciprocalSqrt<2>(const float32x4_t & a)
        {
            float32x4_t e = vrsqrteq_f32(a);
            e = vmulq_f32(vrsqrtsq_f32(vmulq_f32(e, e), a), e);
            return vmulq_f32(vrsqrtsq_f32(vmulq_f32(e, e), a), e);
        }

        template <int iter> SIMD_INLINE float32x4_t Sqrt(const float32x4_t & a)
        {
            return vmulq_f32(a, ReciprocalSqrt<iter>(a));
        }

        template <> SIMD_INLINE float32x4_t Sqrt<-1>(const float32x4_t & a)
        {
            float _a[4];
            vst1q_f32(_a, a);
            float r[4] = { ::sqrtf(_a[0]), ::sqrtf(_a[1]), ::sqrtf(_a[2]), ::sqrtf(_a[3]) };
            return vld1q_f32(r);
        }

        template <int part> SIMD_INLINE int16x8_t Sub(uint8x16_t a, uint8x16_t b)
        {
            return (int16x8_t)vsubl_u8(Half<part>(a), Half<part>(b));
        }

        template <int part> SIMD_INLINE float32x4_t Int16ToFloat(int16x8_t a)
        {
            return vcvtq_f32_s32(UnpackI16<part>(a));
        }

        template <int part> SIMD_INLINE float32x4_t UInt16ToFloat(uint16x8_t a)
        {
            return vcvtq_f32_u32(UnpackU16<part>(a));
        }

        SIMD_INLINE float32x4_t RightNotZero32f(size_t count)
        {
            const int32_t mask[DF] = { 0, 0, 0, 0, -1, -1, -1, -1 };
            return vld1q_f32((float*)(mask + Simd::RestrictRange<ptrdiff_t>(count, 0, F)));
        }

        SIMD_INLINE float32x4_t LeftNotZero32f(ptrdiff_t count)
        {
            const int32_t mask[DF] = { -1, -1, -1, -1, 0, 0, 0, 0 };
            return vld1q_f32((float*)(mask + F - Simd::RestrictRange<ptrdiff_t>(count, 0, F)));
        }

        SIMD_INLINE float32x4_t Not(float32x4_t a)
        {
            return (float32x4_t)vmvnq_u32((uint32x4_t)a);
        }

        SIMD_INLINE float32x4_t And(float32x4_t a, float32x4_t b)
        {
            return (float32x4_t)vandq_u32((uint32x4_t)a, (uint32x4_t)b);
        }

        SIMD_INLINE float32x4_t Or(float32x4_t a, float32x4_t b)
        {
            return (float32x4_t)vorrq_u32((uint32x4_t)a, (uint32x4_t)b);
        }

        template <int index> SIMD_INLINE float32x4_t Broadcast(float32x4_t a)
        {
            return vdupq_lane_f32(Half<index / 2>(a), index & 1);
        }

        SIMD_INLINE uint16x8_t Hadd16u(uint16x8_t a, uint16x8_t b)
        {
            return vcombine_u16(vpadd_u16(Half<0>(a), Half<1>(a)), vpadd_u16(Half<0>(b), Half<1>(b)));
        }

        SIMD_INLINE uint32x4_t Hadd32u(uint32x4_t a, uint32x4_t b)
        {
            return vcombine_u32(vpadd_u32(Half<0>(a), Half<1>(a)), vpadd_u32(Half<0>(b), Half<1>(b)));
        }

        SIMD_INLINE float32x4_t Hadd32f(float32x4_t a, float32x4_t b)
        {
            return vcombine_f32(vpadd_f32(Half<0>(a), Half<1>(a)), vpadd_f32(Half<0>(b), Half<1>(b)));
        }

        template <bool condition> SIMD_INLINE float32x4_t Masked(const float32x4_t & value, const float32x4_t & mask);

        template <> SIMD_INLINE float32x4_t Masked<false>(const float32x4_t & value, const float32x4_t & mask)
        {
            return value;
        }

        template <> SIMD_INLINE float32x4_t Masked<true>(const float32x4_t & value, const float32x4_t & mask)
        {
            return And(value, mask);
        }

        SIMD_INLINE bool TestZ(uint32x4_t a)
        {
            return !(vgetq_lane_u32(a, 0) | vgetq_lane_u32(a, 1) | vgetq_lane_u32(a, 2) | vgetq_lane_u32(a, 3));
        }

        SIMD_INLINE int32x4_t Round(float32x4_t value)
        {
            uint32x4_t sign = vcgtq_f32(value, vdupq_n_f32(0));
            float32x4_t round = vbslq_f32(sign, vdupq_n_f32(0.5f), vdupq_n_f32(-0.5f));
            return vcvtq_s32_f32(vaddq_f32(value, round));
        }

        SIMD_INLINE uint32x4_t RoundPositive(float32x4_t value)
        {
            return vcvtq_u32_f32(vaddq_f32(value, vdupq_n_f32(0.5f)));
        }

        template<bool nofma> float32x4_t Fmadd(float32x4_t a, float32x4_t b, float32x4_t c);

        template <> SIMD_INLINE float32x4_t Fmadd<false>(float32x4_t a, float32x4_t b, float32x4_t c)
        {
            return vmlaq_f32(c, a, b);
        }

        template <> SIMD_INLINE float32x4_t Fmadd<true>(float32x4_t a, float32x4_t b, float32x4_t c)
        {
            return vaddq_f32(vmlaq_f32(vdupq_n_f32(0), a, b), c);
        }

        SIMD_INLINE uint8x16_t Combine(const uint8x8x2_t& a)
        {
            return vcombine_u8(a.val[0], a.val[1]);
        }

        SIMD_INLINE void MinVal32f(float32x4_t src, float& dst)
        {
            float32x2_t half = vpmin_f32(vget_low_f32(src), vget_high_f32(src));
            dst = vget_lane_f32(vpmin_f32(half, half), 0);
        }
    }
#endif
}
#endif
