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
#ifndef __SimdArray_h__
#define __SimdArray_h__

#include "Simd/SimdMemory.h"
#include "Simd/SimdMath.h"

namespace Simd
{
    template <class T> struct Array
    {
        T * const data;
        size_t const size;

        SIMD_INLINE Array(size_t size_ = 0, bool clear = false, size_t align = SIMD_ALIGN)
            : data(0)
            , size(0)
        {
            Resize(size_, clear);
        }

        SIMD_INLINE ~Array()
        {
            if (data)
                Simd::Free(data);
        }

        SIMD_INLINE void Resize(size_t size_, bool clear = false, size_t align = SIMD_ALIGN)
        {
            if (size_ != size)
            {
                if (data)
                {
                    Simd::Free(data);
                    *(T**)&data = 0;
                }
                *(size_t*)&size = size_;
                if (size_)
                    *(T**)&data = (T*)Simd::Allocate(RawSize(), align);
            }
            if (clear)
                Clear();
        }

        SIMD_INLINE void Assign(const T * src, size_t size_)
        {
            Resize(size_, src == NULL);
            if(src)
                memcpy(data, src, RawSize());
        }

        SIMD_INLINE void Clear()
        {
            memset(data, 0, RawSize());
        }

        SIMD_INLINE void Swap(const Array & array)
        {
            Simd::Swap((T*&)data, (T*&)(array.data));
            Simd::Swap((size_t&)size, (size_t&)(array.size));
        }

        SIMD_INLINE T & operator[] (size_t i)
        {
            return data[i];
        }

        SIMD_INLINE const T & operator[] (size_t i) const
        {
            return data[i];
        }

        SIMD_INLINE size_t RawSize() const
        {
            return size * sizeof(T);
        }

        SIMD_INLINE bool Empty() const
        {
            return data == NULL;
        }

        SIMD_INLINE T * Release()
        {
            uint8_t* released = data;
            *(T**)&data = NULL;
            *(size_t*)&size = 0;
            return released;
        }
    };

    typedef Array<int8_t> Array8i;
    typedef Array<uint8_t> Array8u;
    typedef Array<int16_t> Array16i;
    typedef Array<uint16_t> Array16u;
    typedef Array<int32_t> Array32i;
    typedef Array<uint32_t> Array32u;
    typedef Array<float> Array32f;

    typedef Array<uint8_t*> Array8up;
    typedef Array<const uint8_t*> Array8ucp;

    typedef Array<uint16_t*> Array16up;
    typedef Array<const uint16_t*> Array16ucp;

#if defined(__GNUC__) && __GNUC__ >= 6
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wignored-attributes"
#endif

#ifdef SIMD_SSE41_ENABLE
    namespace Sse41
    {
        typedef Array<__m128> Array128f;
        typedef Array<__m128i> Array128i;
    }
#endif

#ifdef SIMD_AVX2_ENABLE
    namespace Avx2
    {
        typedef Array<__m256> Array256f;
    }
#endif

#ifdef SIMD_AVX512BW_ENABLE
    namespace Avx512bw
    {
        typedef Array<__m512> Array512f;
    }
#endif

#ifdef SIMD_NEON_ENABLE
    namespace Neon
    {
        typedef Array<float32x4_t> Array128f;
    }
#endif

#if defined(__GNUC__) && __GNUC__ >= 6
#pragma GCC diagnostic pop
#endif
}

#endif
