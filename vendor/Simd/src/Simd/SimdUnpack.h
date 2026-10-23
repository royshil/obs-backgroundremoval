/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2026 Yermalayeu Ihar.
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
#ifndef __SimdUnpack_h__
#define __SimdUnpack_h__

#include "Simd/SimdDefs.h"
#include "Simd/SimdConst.h"

namespace Simd
{
#ifdef SIMD_SSE41_ENABLE
    namespace Sse41
    {
        template <int part> SIMD_INLINE __m128i UnpackI8(__m128i a);

        template <> SIMD_INLINE __m128i UnpackI8<0>(__m128i a)
        {
            return _mm_cvtepi8_epi16(a);
        }

        template <> SIMD_INLINE __m128i UnpackI8<1>(__m128i a)
        {
            return _mm_cvtepi8_epi16(_mm_srli_si128(a, 8));
        }

        template <int part> SIMD_INLINE __m128i UnpackI16(__m128i a);

        template <> SIMD_INLINE __m128i UnpackI16<0>(__m128i a)
        {
            return _mm_cvtepi16_epi32(a);
        }

        template <> SIMD_INLINE __m128i UnpackI16<1>(__m128i a)
        {
            return _mm_cvtepi16_epi32(_mm_srli_si128(a, 8));
        }

        template <int part> SIMD_INLINE __m128i UnpackU8(__m128i a, __m128i b = K_ZERO);

        template <> SIMD_INLINE __m128i UnpackU8<0>(__m128i a, __m128i b)
        {
            return _mm_unpacklo_epi8(a, b);
        }

        template <> SIMD_INLINE __m128i UnpackU8<1>(__m128i a, __m128i b)
        {
            return _mm_unpackhi_epi8(a, b);
        }

        template<int part> SIMD_INLINE __m128i SubUnpackedU8(__m128i a, __m128i b)
        {
            return _mm_maddubs_epi16(UnpackU8<part>(a, b), K8_01_FF);
        }

        template <int part> SIMD_INLINE __m128i UnpackU16(__m128i a, __m128i b = K_ZERO);

        template <> SIMD_INLINE __m128i UnpackU16<0>(__m128i a, __m128i b)
        {
            return _mm_unpacklo_epi16(a, b);
        }

        template <> SIMD_INLINE __m128i UnpackU16<1>(__m128i a, __m128i b)
        {
            return _mm_unpackhi_epi16(a, b);
        }

        template <int part> SIMD_INLINE __m128i UnpackU32(__m128i a, __m128i b = K_ZERO);

        template <> SIMD_INLINE __m128i UnpackU32<0>(__m128i a, __m128i b)
        {
            return _mm_unpacklo_epi32(a, b);
        }

        template <> SIMD_INLINE __m128i UnpackU32<1>(__m128i a, __m128i b)
        {
            return _mm_unpackhi_epi32(a, b);
        }
    }
#endif// SIMD_SSE41_ENABLE

#ifdef SIMD_AVX2_ENABLE
    namespace Avx2
    {
        template <int part> SIMD_INLINE __m256i UnpackU8(__m256i a, __m256i b = K_ZERO);

        template <> SIMD_INLINE __m256i UnpackU8<0>(__m256i a, __m256i b)
        {
            return _mm256_unpacklo_epi8(a, b);
        }

        template <> SIMD_INLINE __m256i UnpackU8<1>(__m256i a, __m256i b)
        {
            return _mm256_unpackhi_epi8(a, b);
        }

        template<int part> SIMD_INLINE __m256i SubUnpackedU8(__m256i a, __m256i b)
        {
            return _mm256_maddubs_epi16(UnpackU8<part>(a, b), K8_01_FF);
        }

        template <int part> SIMD_INLINE __m256i UnpackU16(__m256i a, __m256i b = K_ZERO);

        template <> SIMD_INLINE __m256i UnpackU16<0>(__m256i a, __m256i b)
        {
            return _mm256_unpacklo_epi16(a, b);
        }

        template <> SIMD_INLINE __m256i UnpackU16<1>(__m256i a, __m256i b)
        {
            return _mm256_unpackhi_epi16(a, b);
        }

        template <int part> SIMD_INLINE __m256i UnpackU32(__m256i a, __m256i b = K_ZERO);

        template <> SIMD_INLINE __m256i UnpackU32<0>(__m256i a, __m256i b)
        {
            return _mm256_unpacklo_epi32(a, b);
        }

        template <> SIMD_INLINE __m256i UnpackU32<1>(__m256i a, __m256i b)
        {
            return _mm256_unpackhi_epi32(a, b);
        }
    }
#endif// SIMD_AVX2_ENABLE

#ifdef SIMD_AVX512BW_ENABLE
    namespace Avx512bw
    {
        template <int part> SIMD_INLINE __m512i UnpackU8(__m512i a, __m512i b = K_ZERO);

        template <> SIMD_INLINE __m512i UnpackU8<0>(__m512i a, __m512i b)
        {
            return _mm512_unpacklo_epi8(a, b);
        }

        template <> SIMD_INLINE __m512i UnpackU8<1>(__m512i a, __m512i b)
        {
            return _mm512_unpackhi_epi8(a, b);
        }

        template <int part> SIMD_INLINE __m512i UnpackU16(__m512i a, __m512i b = K_ZERO);

        template <> SIMD_INLINE __m512i UnpackU16<0>(__m512i a, __m512i b)
        {
            return _mm512_unpacklo_epi16(a, b);
        }

        template <> SIMD_INLINE __m512i UnpackU16<1>(__m512i a, __m512i b)
        {
            return _mm512_unpackhi_epi16(a, b);
        }

        template <int part> SIMD_INLINE __m512i UnpackU32(__m512i a, __m512i b = K_ZERO);

        template <> SIMD_INLINE __m512i UnpackU32<0>(__m512i a, __m512i b)
        {
            return _mm512_unpacklo_epi32(a, b);
        }

        template <> SIMD_INLINE __m512i UnpackU32<1>(__m512i a, __m512i b)
        {
            return _mm512_unpackhi_epi32(a, b);
        }

        SIMD_INLINE __m512i UnpackHalfU8(__m256i a, __m256i b = Avx2::K_ZERO)
        {
            return _mm512_unpacklo_epi8(_mm512_castsi256_si512(a), _mm512_castsi256_si512(b));
        }

        template<int part> SIMD_INLINE __m512i SubUnpackedU8(__m512i a, __m512i b)
        {
            return _mm512_maddubs_epi16(UnpackU8<part>(a, b), K8_01_FF);
        }
    }
#endif //SIMD_AVX512BW_ENABLE

#ifdef SIMD_SVE_ENABLE
    namespace Sve
    {
        template <int part> SIMD_INLINE svuint16_t UnpackU8(svuint8_t a);

        template <> SIMD_INLINE svuint16_t UnpackU8<0>(svuint8_t a)
        {
            return svunpklo_u16(a);
        }

        template <> SIMD_INLINE svuint16_t UnpackU8<1>(svuint8_t a)
        {
            return svunpkhi_u16(a);
        }

        //-------------------------------------------------------------------------------------------------

        template <int part> SIMD_INLINE svuint32_t UnpackU16(svuint16_t a);

        template <> SIMD_INLINE svuint32_t UnpackU16<0>(svuint16_t a)
        {
            return svunpklo_u32(a);
        }

        template <> SIMD_INLINE svuint32_t UnpackU16<1>(svuint16_t a)
        {
            return svunpkhi_u32(a);
        }
    }
#endif
}
#endif
