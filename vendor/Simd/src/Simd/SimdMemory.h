/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2026 Yermalayeu Ihar.
*               2016-2016 Sintegrial Technologies,
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
#ifndef __SimdMemory_h__
#define __SimdMemory_h__

#include "Simd/SimdDefs.h"
#include "Simd/SimdMath.h"

#if defined(SIMD_ALLOCATE_ERROR_MESSAGE)
#include <iostream>
#endif
#include <memory>

namespace Simd
{
    SIMD_INLINE size_t DivHi(size_t value, size_t divider)
    {
        return (value + divider - 1) / divider;
    }

    SIMD_INLINE size_t Pow2Hi(size_t value)
    {
        size_t pow2 = 1;
        for (; pow2 < value; pow2 *= 2);
        return pow2;
    }

    SIMD_INLINE size_t Pow2Divider(size_t value)
    {
        size_t divider = 1;
        for (; value%2 == 0; value = value / 2)
            divider *= 2;
        return divider;
    }

    SIMD_INLINE size_t AlignHiAny(size_t size, size_t align)
    {
        return (size + align - 1) / align * align;
    }

    SIMD_INLINE size_t AlignLoAny(size_t size, size_t align)
    {
        return size / align * align;
    }

    SIMD_INLINE size_t AlignHi(size_t size, size_t align)
    {
        return (size + align - 1) & ~(align - 1);
    }

    SIMD_INLINE void * AlignHi(const void * ptr, size_t align)
    {
        return (void *)((((size_t)ptr) + align - 1) & ~(align - 1));
    }

    SIMD_INLINE size_t AlignLo(size_t size, size_t align)
    {
        return size & ~(align - 1);
    }

    SIMD_INLINE void * AlignLo(const void * ptr, size_t align)
    {
        return (void *)(((size_t)ptr) & ~(align - 1));
    }

    SIMD_INLINE bool Aligned(size_t size, size_t align)
    {
        return size == AlignLo(size, align);
    }

    SIMD_INLINE bool AlignedAny(size_t size, size_t align)
    {
        return size == AlignLoAny(size, align);
    }

    SIMD_INLINE bool Aligned(const void * ptr, size_t align)
    {
        return ptr == AlignLo(ptr, align);
    }

    //-------------------------------------------------------------------------------------------------

    template<class T> T* Allocate(uint8_t*& buffer, size_t size, size_t align = SIMD_ALIGN)
    {
        if (size)
        {
            T* ptr = (T*)buffer;
            buffer = buffer + AlignHi(size * sizeof(T), align);
            return ptr;
        }
        else
            return NULL;
    }

    SIMD_INLINE void SetGap(uint8_t*& buffer, size_t size = SIMD_ALIGN)
    {
        memset(buffer, 0, size);
        buffer += size;
    }

    //-------------------------------------------------------------------------------------------------

#ifdef SIMD_NO_MANS_LAND
    const uint8_t NO_MANS_LAND_WATERMARK = 0x55;
#endif

    SIMD_INLINE void* Allocate(size_t size, size_t align = SIMD_ALIGN)
    {
#ifdef SIMD_NO_MANS_LAND
        size += 2 * SIMD_NO_MANS_LAND;
#endif
        void* ptr = NULL;
#if defined(_MSC_VER) 
        ptr = _aligned_malloc(size, align);
#elif defined(__MINGW32__) || defined(__MINGW64__)
        ptr = __mingw_aligned_malloc(size, align);
#elif defined(__GNUC__)
        align = AlignHi(align, sizeof(void*));
        size = AlignHi(size, align);
        int result = ::posix_memalign(&ptr, align, size);
        if (result != 0)
            ptr = NULL;
#else
        ptr = malloc(size);
#endif
#ifdef SIMD_ALLOCATE_ERROR_MESSAGE
        if (ptr == NULL)
            std::cout << "The function posix_memalign can't allocate " << size << " bytes with align " << align << " !" << std::endl << std::flush;
#endif
#ifdef SIMD_ALLOCATE_ASSERT
        assert(ptr);
#endif
#ifdef SIMD_NO_MANS_LAND
        if (ptr)
        {
#if !defined(NDEBUG) && SIMD_NO_MANS_LAND >= 16
            * (size_t*)ptr = size - 2 * SIMD_NO_MANS_LAND;
            memset((char*)ptr + sizeof(size_t), NO_MANS_LAND_WATERMARK, SIMD_NO_MANS_LAND - sizeof(size_t));
            memset((char*)ptr + size - SIMD_NO_MANS_LAND, NO_MANS_LAND_WATERMARK, SIMD_NO_MANS_LAND);
#endif
            ptr = (char*)ptr + SIMD_NO_MANS_LAND;
        }
#endif
        return ptr;
    }

    SIMD_INLINE void Free(void * ptr)
    {
#ifdef SIMD_NO_MANS_LAND
        if (ptr)
        {
            ptr = (char*)ptr - SIMD_NO_MANS_LAND;
#if !defined(NDEBUG) && SIMD_NO_MANS_LAND >= 16
            size_t size = *(size_t*)ptr;
            char* nose = (char*)ptr + sizeof(size_t), *tail = (char*)ptr + SIMD_NO_MANS_LAND + size;
            for (size_t i = 0, n = SIMD_NO_MANS_LAND - sizeof(size_t); i < n; ++i)
                assert(nose[i] == NO_MANS_LAND_WATERMARK);
            for (size_t i = 0, n = SIMD_NO_MANS_LAND; i < n; ++i)
                assert(tail[i] == NO_MANS_LAND_WATERMARK);
#endif  
        }
#endif
#if defined(_MSC_VER) 
        _aligned_free(ptr);
#elif defined(__MINGW32__) || defined(__MINGW64__)
        return __mingw_aligned_free(ptr);
#else
        free(ptr);
#endif
    }

    //-------------------------------------------------------------------------------------------------

    struct Deletable
    {
        virtual ~Deletable() {}
    };

    //-------------------------------------------------------------------------------------------------

#if defined(SIMD_CPP_2011_ENABLE)
    template<class T> using Holder = std::unique_ptr<T>;
#else
    template <class T> class Holder
    {
        T* _ptr;

    public:
        Holder(T* ptr)
            : _ptr(ptr)
        {
        }

        ~Holder()
        {
            if (_ptr)
                delete _ptr;
        }

        T& operator * ()
        {
            return *_ptr;
        }

        const T& operator * () const
        {
            return *_ptr;
        }

        T* operator -> ()
        {
            return _ptr;
        }

        const T* operator -> () const
        {
            return _ptr;
        }

        operator bool() const 
        {
            return _ptr != NULL;
        }
    };
#endif

    //-------------------------------------------------------------------------------------------------

#ifdef SIMD_SSE41_ENABLE
    namespace Sse41
    {
        SIMD_INLINE bool Aligned(size_t size, size_t align = sizeof(__m128))
        {
            return Simd::Aligned(size, align);
        }

        SIMD_INLINE bool Aligned(const void* ptr, size_t align = sizeof(__m128))
        {
            return Simd::Aligned(ptr, align);
        }
    }
#endif

#ifdef SIMD_AVX2_ENABLE
    namespace Avx2
    {
        SIMD_INLINE bool Aligned(size_t size, size_t align = sizeof(__m256))
        {
            return Simd::Aligned(size, align);
        }

        SIMD_INLINE bool Aligned(const void* ptr, size_t align = sizeof(__m256))
        {
            return Simd::Aligned(ptr, align);
        }
    }
#endif

#ifdef SIMD_AVX512BW_ENABLE
    namespace Avx512bw
    {
        SIMD_INLINE bool Aligned(size_t size, size_t align = sizeof(__m512))
        {
            return Simd::Aligned(size, align);
        }

        SIMD_INLINE bool Aligned(const void * ptr, size_t align = sizeof(__m512))
        {
            return Simd::Aligned(ptr, align);
        }
    }
#endif

#ifdef SIMD_AVX512VNNI_ENABLE
    namespace Avx512vnni
    {
        using Avx512bw::Aligned;
    }
#endif

#ifdef SIMD_AMXBF16_ENABLE
    namespace AmxBf16
    {
        using Avx512bw::Aligned;
    }
#endif

#ifdef SIMD_NEON_ENABLE
    namespace Neon
    {
        SIMD_INLINE bool Aligned(size_t size, size_t align = sizeof(uint8x16_t))
        {
            return Simd::Aligned(size, align);
        }

        SIMD_INLINE bool Aligned(const void * ptr, size_t align = sizeof(uint8x16_t))
        {
            return Simd::Aligned(ptr, align);
        }
    }
#endif

#ifdef SIMD_HVX_ENABLE
    namespace Hvx
    {
        SIMD_INLINE bool Aligned(size_t size, size_t align = sizeof(HVX_Vector))
        {
            return Simd::Aligned(size, align);
        }

        SIMD_INLINE bool Aligned(const void * ptr, size_t align = sizeof(HVX_Vector))
        {
            return Simd::Aligned(ptr, align);
        }
    }
#endif
}

#endif
