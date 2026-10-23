/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2025 Yermalayeu Ihar.
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
#ifndef __SimdCopy_h__
#define __SimdCopy_h__

#include "Simd/SimdDefs.h"

namespace Simd
{
    namespace Base
    {
        template<size_t N> SIMD_INLINE void CopyPixel(const uint8_t* src, uint8_t* dst)
        {
            for (size_t i = 0; i < N; ++i)
                dst[i] = src[i];
        }

        template<> SIMD_INLINE void CopyPixel<1>(const uint8_t* src, uint8_t* dst)
        {
            dst[0] = src[0];
        }

        template<> SIMD_INLINE void CopyPixel<2>(const uint8_t* src, uint8_t* dst)
        {
            ((uint16_t*)dst)[0] = ((uint16_t*)src)[0];
        }

        template<> SIMD_INLINE void CopyPixel<3>(const uint8_t* src, uint8_t* dst)
        {
            ((uint16_t*)dst)[0] = ((uint16_t*)src)[0];
            dst[2] = src[2];
        }

        template<> SIMD_INLINE void CopyPixel<4>(const uint8_t* src, uint8_t* dst)
        {
            ((uint32_t*)dst)[0] = ((uint32_t*)src)[0];
        }

        template<> SIMD_INLINE void CopyPixel<6>(const uint8_t* src, uint8_t* dst)
        {
            ((uint32_t*)dst)[0] = ((uint32_t*)src)[0];
            ((uint16_t*)dst)[2] = ((uint16_t*)src)[2];
        }

        template<> SIMD_INLINE void CopyPixel<8>(const uint8_t* src, uint8_t* dst)
        {
            ((uint64_t*)dst)[0] = ((uint64_t*)src)[0];
        }

        template<> SIMD_INLINE void CopyPixel<12>(const uint8_t* src, uint8_t* dst)
        {
            ((uint64_t*)dst)[0] = ((uint64_t*)src)[0];
            ((uint32_t*)dst)[2] = ((uint32_t*)src)[2];
        }
    }

#ifdef SIMD_SSE41_ENABLE
    namespace Sse41
    {
        SIMD_INLINE void Copy(const uint8_t* src, uint8_t* dst)
        {
            _mm_storeu_si128((__m128i*)dst, _mm_loadu_si128((__m128i*)src));
        }

        SIMD_INLINE void Copy(const uint16_t* src, uint16_t* dst)
        {
            _mm_storeu_si128((__m128i*)dst, _mm_loadu_si128((__m128i*)src));
        }
    }
#endif

#ifdef SIMD_AVX2_ENABLE
    namespace Avx2
    {
        SIMD_INLINE void Copy(const uint8_t* src, uint8_t* dst)
        {
            _mm256_storeu_si256((__m256i*)dst, _mm256_loadu_si256((__m256i*)src));
        }

        SIMD_INLINE void Copy(const uint16_t* src, uint16_t* dst)
        {
            _mm256_storeu_si256((__m256i*)dst, _mm256_loadu_si256((__m256i*)src));
        }
    }
#endif

#ifdef SIMD_AVX512BW_ENABLE    
    namespace Avx512bw
    {
        SIMD_INLINE void Copy(const uint16_t* src, uint16_t* dst, __mmask32 srcMask = __mmask32(-1), __mmask32 dstMask = __mmask32(-1))
        {
            _mm512_mask_storeu_epi16(dst, dstMask, _mm512_maskz_loadu_epi16(srcMask, src));
        }

        SIMD_INLINE void Copy(const uint16_t* src, size_t size32, __mmask32 tail, uint16_t* dst)
        {
            size_t i = 0;
            for (; i < size32; i += 32)
                _mm512_storeu_si512(dst + i, _mm512_loadu_si512(src + i));
            if (tail)
                _mm512_mask_storeu_epi16(dst + i, tail, _mm512_maskz_loadu_epi16(tail, src + i));
        }

        SIMD_INLINE void Copy(const uint16_t* src, size_t size, uint16_t* dst)
        {
            size_t tail = size & 31;
            Copy(src, size & (~31), tail ? __mmask32(-1) >> (32 - tail) : 0, dst);
        } 

        //-------------------------------------------------------------------------------------------------

        SIMD_INLINE void Copy(const uint8_t* src, uint8_t* dst, __mmask64 srcMask = __mmask64(-1), __mmask64 dstMask = __mmask64(-1))
        {
            _mm512_mask_storeu_epi8(dst, dstMask, _mm512_maskz_loadu_epi8(srcMask, src));
        }

        SIMD_INLINE void Copy(const uint8_t* src, size_t size64, __mmask64 tail, uint8_t* dst)
        {
            size_t i = 0;
            for (; i < size64; i += 64)
                _mm512_storeu_si512(dst + i, _mm512_loadu_si512(src + i));
            if (tail)
                _mm512_mask_storeu_epi8(dst + i, tail, _mm512_maskz_loadu_epi8(tail, src + i));
        }

        SIMD_INLINE void Copy(const uint8_t* src, size_t size, uint8_t* dst)
        {
            size_t tail = size & 63;
            Copy(src, size & (~63), tail ? __mmask64(-1) >> (64 - tail) : 0, dst);
        }
    }
#endif
}

#endif
