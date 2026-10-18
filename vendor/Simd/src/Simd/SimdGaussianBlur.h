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
#ifndef __SimdGaussianBlur_h__
#define __SimdGaussianBlur_h__

#include "Simd/SimdArray.h"
#include "Simd/SimdMath.h"
#include "Simd/SimdCopy.h"

namespace Simd
{
    struct BlurParam
    {
        size_t width;
        size_t height;
        size_t channels;
        float sigma;
        float epsilon;
        size_t align;

        BlurParam(size_t w, size_t h, size_t c, const float* s, const float * e, size_t a);
        bool Valid() const;
    };

    class GaussianBlur : Deletable
    {
    public:
        GaussianBlur(const BlurParam& param);

        virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride) = 0;

    protected:
        BlurParam _param;
    };

    namespace Base
    {
        template<int channels> SIMD_INLINE void PadCols(const uint8_t* src, size_t half, size_t size, uint8_t* dst)
        {
            for (size_t x = 0; x < half; x += 1, dst += channels)
                Base::CopyPixel<channels>(src, dst);
            memcpy(dst, src, size), dst += size, src += size - channels;
            for (size_t x = 0; x < half; x += 1, dst += channels)
                Base::CopyPixel<channels>(src, dst);
        }

        //---------------------------------------------------------------------

        struct AlgDefault
        {
            size_t half, kernel, edge, start, size, stride, nose, body;
            Array32f weight;
        };

        typedef void (*BlurDefaultPtr)(const BlurParam& p, const AlgDefault& a, const uint8_t* src, 
            size_t srcStride, uint8_t* cols, float* rows, uint8_t* dst, size_t dstStride);

        class GaussianBlurDefault : public Simd::GaussianBlur
        {
        public:
            GaussianBlurDefault(const BlurParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);

        protected:
            AlgDefault _alg;
            Array8u _cols;
            Array32f _rows;
            BlurDefaultPtr _blur;
        };

        void * GaussianBlurInit(size_t width, size_t height, size_t channels, const float* sigma, const float* epsilon);
    }

#ifdef SIMD_SSE41_ENABLE    
    namespace Sse41
    {
        class GaussianBlurDefault : public Base::GaussianBlurDefault
        {
        public:
            GaussianBlurDefault(const BlurParam& param);
        };

        void* GaussianBlurInit(size_t width, size_t height, size_t channels, const float* sigma, const float* epsilon);
    }
#endif //SIMD_SSE41_ENABLE

#ifdef SIMD_AVX2_ENABLE    
    namespace Avx2
    {
        class GaussianBlurDefault : public Sse41::GaussianBlurDefault
        {
        public:
            GaussianBlurDefault(const BlurParam& param);
        };

        void* GaussianBlurInit(size_t width, size_t height, size_t channels, const float* sigma, const float* epsilon);
    }
#endif //SIMD_AVX2_ENABLE

#ifdef SIMD_AVX512BW_ENABLE    
    namespace Avx512bw
    {
        class GaussianBlurDefault : public Avx2::GaussianBlurDefault
        {
        public:
            GaussianBlurDefault(const BlurParam& param);
        };

        void* GaussianBlurInit(size_t width, size_t height, size_t channels, const float* sigma, const float* epsilon);
    }
#endif //SIMD_AVX512BW_ENABLE

#ifdef SIMD_NEON_ENABLE    
    namespace Neon
    {
        class GaussianBlurDefault : public Base::GaussianBlurDefault
        {
        public:
            GaussianBlurDefault(const BlurParam& param);
        };

        void* GaussianBlurInit(size_t width, size_t height, size_t channels, const float* sigma, const float* epsilon);
    }
#endif //SIMD_NEON_ENABLE

#ifdef SIMD_SVE2_ENABLE    
    namespace Sve2
    {
#ifdef SIMD_NEON_ENABLE
        class GaussianBlurDefault : public Neon::GaussianBlurDefault
#else
        class GaussianBlurDefault : public Base::GaussianBlurDefault
#endif
        {
        public:
            GaussianBlurDefault(const BlurParam& param);
        };

        void* GaussianBlurInit(size_t width, size_t height, size_t channels, const float* sigma, const float* epsilon);
    }
#endif //SIMD_SVE2_ENABLE
}
#endif//__SimdGaussianBlur_h__
