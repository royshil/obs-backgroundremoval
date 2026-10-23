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
#ifndef __SimdLog_h__
#define __SimdLog_h__

#include "Simd/SimdConst.h"
#include "Simd/SimdArray.h"

#ifdef SIMD_LOG_ENABLE
#include <iostream>
#include <iomanip>

namespace Simd
{
    template<class T> SIMD_INLINE void Log(const T * data, size_t size, const std::string & name, int prec = 3)
    {
        std::cout << name.c_str() << " = { ";
        for (size_t i = 0; i < size; i++)
        {
            std::cout << int(data[i]) << " ";
        }
        std::cout << "} " << std::endl << std::flush;
    }

    template<> SIMD_INLINE void Log<float>(const float * data, size_t size, const std::string & name, int prec)
    {
        std::cout << name.c_str() << " = { " << std::setprecision(prec) << std::fixed;
        for (size_t i = 0; i < size; i++)
        {
            std::cout << data[i] << " ";
        }
        std::cout << "} " << std::endl << std::flush;
    }

    template<class T> SIMD_INLINE void Log(const Array<T> & array, const std::string & name, int prec = 3)
    {
        Log<T>(array.data, array.size, name);
    }

#ifdef SIMD_SSE41_ENABLE
    namespace Sse41
    {
        SIMD_INLINE void Log(const __m128 & value, const std::string & name)
        {
            float buffer[F];
            _mm_storeu_ps(buffer, value);
            Simd::Log<float>(buffer, F, name);
        }        
        
        template<class T> SIMD_INLINE void Log(const __m128i & value, const std::string & name)
        {
            const size_t n = sizeof(__m128i) / sizeof(T);
            T buffer[n];
            _mm_storeu_si128((__m128i*)buffer, value);
            Simd::Log<T>(buffer, n, name);
        }
    }
#endif 

#ifdef SIMD_AVX2_ENABLE
    namespace Avx2
    {
        SIMD_INLINE void Log(const __m256 & value, const std::string & name)
        {
            float buffer[F];
            _mm256_storeu_ps(buffer, value);
            Simd::Log<float>(buffer, F, name);
        }

        template<class T> SIMD_INLINE void Log(const __m256i & value, const std::string & name)
        {
            const size_t n = sizeof(__m256i) / sizeof(T);
            T buffer[n];
            _mm256_storeu_si256((__m256i*)buffer, value);
            Simd::Log<T>(buffer, n, name);
        }
    }
#endif 

#ifdef SIMD_AVX512BW_ENABLE
    namespace Avx512bw
    {
        SIMD_INLINE void Log(const __m512 & value, const std::string & name)
        {
            float buffer[F];
            _mm512_storeu_ps(buffer, value);
            Simd::Log<float>(buffer, F, name);
        }

        template<class T> SIMD_INLINE void Log(const __m512i& value, const std::string& name)
        {
            const size_t n = sizeof(__m512i) / sizeof(T);
            T buffer[n];
            _mm512_storeu_si512((__m512i*)buffer, value);
            Simd::Log<T>(buffer, n, name);
        }
    }
#endif

#ifdef SIMD_AMXBF16_ENABLE
    namespace AmxBf16
    {
        using Avx512bw::Log;
    }
#endif

#ifdef SIMD_NEON_ENABLE
    namespace Neon
    {
        SIMD_INLINE void Log8u(const uint8x16_t & value, const std::string & name)
        {
            uint8_t buffer[16];
            vst1q_u8(buffer, value);
            Simd::Log(buffer, 16, name);
        }

        SIMD_INLINE void Log8u(const uint8x8_t& value, const std::string& name)
        {
            uint8_t buffer[8];
            vst1_u8(buffer, value);
            Simd::Log(buffer, 8, name);
        }

        SIMD_INLINE void Log16u(const uint16x8_t & value, const std::string & name)
        {
            uint16_t buffer[8];
            vst1q_u16(buffer, value);
            Simd::Log(buffer, 8, name);
        }

        SIMD_INLINE void Log16u(const uint16x4_t& value, const std::string& name)
        {
            uint16_t buffer[4];
            vst1_u16(buffer, value);
            Simd::Log(buffer, 4, name);
        }

        SIMD_INLINE void Log16i(const int16x8_t & value, const std::string & name)
        {
            int16_t buffer[8];
            vst1q_s16(buffer, value);
            Simd::Log(buffer, 8, name);
        }

        SIMD_INLINE void Log16i(const int16x4_t& value, const std::string& name)
        {
            int16_t buffer[4];
            vst1_s16(buffer, value);
            Simd::Log(buffer, 4, name);
        }

        SIMD_INLINE void Log32u(const uint32x4_t & value, const std::string & name)
        {
            uint32_t buffer[4];
            vst1q_u32(buffer, value);
            Simd::Log(buffer, 4, name);
        }

        SIMD_INLINE void Log32u(const uint32x2_t& value, const std::string& name)
        {
            uint32_t buffer[2];
            vst1_u32(buffer, value);
            Simd::Log(buffer, 2, name);
        }

        SIMD_INLINE void Log32i(const int32x4_t & value, const std::string & name)
        {
            int32_t buffer[4];
            vst1q_s32(buffer, value);
            Simd::Log(buffer, 4, name);
        }

        SIMD_INLINE void Log32i(const int32x2_t& value, const std::string& name)
        {
            int32_t buffer[2];
            vst1_s32(buffer, value);
            Simd::Log(buffer, 2, name);
        }

        SIMD_INLINE void Log32f(const float32x4_t & value, const std::string & name)
        {
            float buffer[4];
            vst1q_f32(buffer, value);
            std::cout << name << " = { ";
            for (int i = 0; i < 4; i++)
                std::cout << buffer[i] << " ";
            std::cout << "} " << std::endl;
        }

        SIMD_INLINE void Log32f(const float32x2_t& value, const std::string& name)
        {
            float buffer[2];
            vst1_f32(buffer, value);
            std::cout << name << " = { ";
            for (int i = 0; i < 2; i++)
                std::cout << buffer[i] << " ";
            std::cout << "} " << std::endl;
        }
    }
#endif
}

#define SIMD_LOG(value) Log(value, #value)
#define SIMD_LOG1(value) Log<uint8_t>(value, #value)
#define SIMD_LOG2(value) Log<int16_t>(value, #value)
#define SIMD_LOG4(value) Log<int32_t>(value, #value)

#define SIMD_LOG_ERROR(message) \
{ \
    std::stringstream ss; \
    ss << std::endl << " In function " << SIMD_FUNCTION << ":" << std::endl; \
    ss << " In file " << __FILE__ << ":" << __LINE__ << ":" << std::endl; \
    ss << " Error: " << message << std::endl << std::endl; \
    std::cerr << ss.str() << std::flush; \
}

#else

#define SIMD_LOG(value)
#define SIMD_LOG1(value)
#define SIMD_LOG2(value)
#define SIMD_LOG4(value)

#define SIMD_LOG_ERROR(message)

#endif 

#endif
