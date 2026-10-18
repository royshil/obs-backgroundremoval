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
#ifndef __SimdResizer_h__
#define __SimdResizer_h__

#include "Simd/SimdArray.h"
#include "Simd/SimdMath.h"
#include "Simd/SimdBase.h"

#define SIMD_RESIZER_BICUBIC_BITS 7 // 7, 11

namespace Simd
{
    struct ResParam
    {
        SimdResizeChannelType type;
        SimdResizeMethodType method;
        size_t srcW, srcH, dstW, dstH, channels, align;

        ResParam(size_t srcW, size_t srcH, size_t dstW, size_t dstH, size_t channels, SimdResizeChannelType type, SimdResizeMethodType method, size_t align)
        {
            this->type = type;
            this->method = method;
            this->srcW = srcW;
            this->srcH = srcH;
            this->dstW = dstW;
            this->dstH = dstH;
            this->channels = channels;
            this->align = align;
        }

        bool IsNearest() const
        {
            return method == SimdResizeMethodNearest || method == SimdResizeMethodNearestPytorch || srcW == 1 || srcH == 1;
        }

        bool IsByteBilinear() const
        {
            return type == SimdResizeChannelByte && (method == SimdResizeMethodBilinear || 
                (method == SimdResizeMethodBicubic && (srcW == 2 || srcH == 2)));
        }

        bool IsByteBilinearOpenCv() const
        {
            return type == SimdResizeChannelByte && (method == SimdResizeMethodBilinearOpenCv);
        }

        bool IsShortBilinear() const
        {
            return type == SimdResizeChannelShort && method == SimdResizeMethodBilinear;
        }

        bool IsFloatBilinear() const
        {
            return type == SimdResizeChannelFloat && 
                (method == SimdResizeMethodBilinear || method == SimdResizeMethodBilinearCaffe || method == SimdResizeMethodBilinearPytorch);
        }

        bool IsBf16Bilinear() const
        {
            return type == SimdResizeChannelBf16 &&
                (method == SimdResizeMethodBilinear || method == SimdResizeMethodBilinearCaffe || method == SimdResizeMethodBilinearPytorch);
        }

        bool IsByteBicubic() const
        {
            return type == SimdResizeChannelByte && method == SimdResizeMethodBicubic;
        }

        bool IsByteArea1x1() const
        {
            return type == SimdResizeChannelByte && (method == SimdResizeMethodArea || (method == SimdResizeMethodAreaFast && 
                (DivHi(srcW, 2) < dstW || DivHi(srcH, 2) < dstH)));
        }

        bool IsByteArea2x2() const
        {
            return type == SimdResizeChannelByte && method == SimdResizeMethodAreaFast &&
                DivHi(srcW, 2) >= dstW && DivHi(srcH, 2) >= dstH;
        }

        size_t ChannelSize() const
        {
            static const size_t sizes[4] = { 1, 2, 4, 2 };
            return sizes[(int)type];
        }

        size_t PixelSize() const
        {
            return ChannelSize() * channels;
        }
    };

    //-------------------------------------------------------------------------------------------------

    class Resizer : Deletable
    {
    public:
        Resizer(const ResParam & param)
            : _param(param)
            , _threads(Base::GetThreadNumber())
        {
        }

        virtual void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride) = 0;

    protected:
        ResParam _param;
        size_t _threads;
    };

    //-------------------------------------------------------------------------------------------------

    namespace Base
    {
        class ResizerNearest : public Resizer
        {
            void Resize(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
            template<size_t N> void Resize(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
        protected:
            size_t _pixelSize;
            Array32i _ix, _iy;

            void EstimateIndex(size_t srcSize, size_t dstSize, size_t channelSize, size_t channels, int32_t* indices);

            void EstimateParams();

            virtual void Run(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
        public:
            ResizerNearest(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteBilinear : public Resizer
        {
        protected:
            Array32i _ax, _ix, _ay, _iy, _bx[2];

            void EstimateIndexAlpha(size_t srcSize, size_t dstSize, size_t channels, int32_t * indices, int32_t * alphas);
        public:
            ResizerByteBilinear(const ResParam & param);

            virtual void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        const int LINEAR_X_LSHIFT = 14;
        const int LINEAR_X_RANGE = 1 << LINEAR_X_LSHIFT;
        const int LINEAR_X_RSHIFT = LINEAR_X_LSHIFT - 7;

        const int LINEAR_Y_LSHIFT = 14;
        const int LINEAR_Y_RANGE = 1 << LINEAR_Y_LSHIFT;
        const int LINEAR_Y_RSHIFT = LINEAR_X_LSHIFT - LINEAR_X_RSHIFT + LINEAR_Y_LSHIFT - 16;
        const int LINEAR_Y_ROUND = 1 << (LINEAR_Y_RSHIFT - 1);

        class ResizerByteBilinearOpenCv : public Resizer
        {
        protected:
            Array16i _ax, _ay, _bx[2];
            Array32i _ix, _iy;

            void EstimateIndexAlpha(size_t srcSize, size_t dstSize, size_t channels, int32_t* indices, int16_t* alphas, int range);
        public:
            ResizerByteBilinearOpenCv(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerShortBilinear : public Resizer
        {
        protected:
            Array32i _ix, _iy;
            Array32f _ax, _ay, _bx[2];

            void EstimateIndexAlpha(size_t srcSize, size_t dstSize, size_t channels, int32_t* indices, float* alphas);

            template<size_t N> void RunB(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
            template<size_t N> void RunS(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);

            virtual void Run(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);

        public:
            ResizerShortBilinear(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerFloatBilinear : public Resizer
        {
        protected:
            bool _rowBuf;
            Array32i _ix, _iy;
            Array32f _ax, _ay, _bx[2];

            virtual void Run(const float * src, size_t srcStride, float * dst, size_t dstStride);

        public:
            ResizerFloatBilinear(const ResParam & param);

            virtual void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerBf16Bilinear : public Resizer
        {
        protected:
            bool _rowBuf;
            Array32i _ix, _iy;
            Array32f _ax, _ay, _bx[2];

            virtual void Run(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);

        public:
            ResizerBf16Bilinear(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        const int32_t BICUBIC_RANGE = 1 << SIMD_RESIZER_BICUBIC_BITS;
        const int32_t BICUBIC_SHIFT = SIMD_RESIZER_BICUBIC_BITS * 2;
        const int32_t BICUBIC_ROUND = 1 << (BICUBIC_SHIFT - 1);

        class ResizerByteBicubic : public Resizer
        {
        protected:
            Array32i _ix, _iy, _ax, _ay, _bx[4];
            size_t _xn, _xt, _sxl;

            void EstimateIndexAlpha(size_t sizeS, size_t sizeD, size_t N, Array32i& index, Array32i& alpha);

            void Init(bool sparse);

            template<int N> void RunS(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
            template<int N> void RunB(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteBicubic(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        const int32_t AREA_SHIFT = 22;
        const int32_t AREA_RANGE = 1 << 11;
        const int32_t AREA_ROUND = 1 << 21;

        class ResizerByteArea : public Resizer
        {
        protected:
            Array32i _ax, _ix, _ay, _iy, _by;

            void EstimateParams(size_t srcSize, size_t dstSize, size_t range, int32_t* alpha, int32_t* index);
        public:
            ResizerByteArea(const ResParam& param);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteArea1x1 : public ResizerByteArea
        {
        protected:
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteArea1x1(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };
        
        //-------------------------------------------------------------------------------------------------

        class ResizerByteArea2x2 : public ResizerByteArea
        {
        protected:
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteArea2x2(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        void * ResizerInit(size_t srcX, size_t srcY, size_t dstX, size_t dstY, size_t channels, SimdResizeChannelType type, SimdResizeMethodType method);
    }

#ifdef SIMD_SSE41_ENABLE    
    namespace Sse41
    {
        class ResizerNearest : public Base::ResizerNearest
        {
        protected:
            size_t _blocks, _tails;
            struct IndexShuffle16x1
            {
                int32_t src, dst;
                uint8_t shuffle[A];
            };
            Array<IndexShuffle16x1> _ix16x1;
            Array128i _tail16x1;

            size_t BlockCountMax(size_t align);
            void EstimateParams();
            void Shuffle16x1(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
            void Resize12(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
        public:
            ResizerNearest(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };   

        //-------------------------------------------------------------------------------------------------

        class ResizerByteBilinear : public Base::ResizerByteBilinear
        {
        protected:
            Array8u _ax;
            Array8u _bx[2];
            size_t _blocks;
            struct Idx
            {
                int32_t src, dst;
                uint8_t shuffle[A];
            };
            Array<Idx> _ixg;

            size_t BlockCountMax(size_t align);
            void EstimateParams();
            template<size_t N> void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
            void RunG(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        public:
            ResizerByteBilinear(const ResParam & param);

            virtual void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        }; 

        //-------------------------------------------------------------------------------------------------

        class ResizerByteBilinearOpenCv : public Base::ResizerByteBilinearOpenCv
        {
        protected:
            Array8u _sx;
            size_t _blocks;
            struct Idx
            {
                int32_t src, dst;
                uint8_t shuffle[DA];
            };
            Array<Idx> _ixg;

            size_t BlockCountMax(size_t align);
            void EstimateParams();
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
            void RunG(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteBilinearOpenCv(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerShortBilinear : public Base::ResizerShortBilinear
        {
        protected:
            template<size_t N> void RunB(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
            template<size_t N> void RunS(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);

            virtual void Run(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
        public:
            ResizerShortBilinear(const ResParam& param);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerFloatBilinear : public Base::ResizerFloatBilinear
        {
        protected:
            virtual void Run(const float* src, size_t srcStride, float* dst, size_t dstStride);
        public:
            ResizerFloatBilinear(const ResParam& param);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerBf16Bilinear : public Base::ResizerBf16Bilinear
        {
        protected:
            virtual void Run(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
        public:
            ResizerBf16Bilinear(const ResParam& param);
        };
        
        //-------------------------------------------------------------------------------------------------

        class ResizerByteBicubic : public Base::ResizerByteBicubic
        {
        protected:
            Array8i _ax;

            void EstimateIndexAlphaY();
            void EstimateIndexAlphaX();

            void Init(bool sparse);

            template<int N> void RunS(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
            template<int N> void RunB(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteBicubic(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteArea1x1 : public Base::ResizerByteArea1x1
        {
        protected:
            template<size_t N> void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        public:
            ResizerByteArea1x1(const ResParam & param);

            virtual void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteArea2x2 : public Base::ResizerByteArea2x2
        {
        protected:
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteArea2x2(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        void * ResizerInit(size_t srcX, size_t srcY, size_t dstX, size_t dstY, size_t channels, SimdResizeChannelType type, SimdResizeMethodType method);
    }
#endif

#ifdef SIMD_AVX2_ENABLE    
    namespace Avx2
    {
        class ResizerNearest : public Sse41::ResizerNearest
        {
        protected:
            void EstimateParams();
            void Gather2(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
            void Gather3(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
            void Gather4(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
            void Gather8(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
        public:
            ResizerNearest(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteBilinear : public Sse41::ResizerByteBilinear
        {
        protected:
            struct Idx
            {
                int32_t src, dst;
                uint8_t shuffle[A];
            };
            Array<Idx> _ixg;

            void EstimateParams();
            template<size_t N> void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
            void RunG(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        public:
            ResizerByteBilinear(const ResParam & param);

            virtual void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteBilinearOpenCv : public Sse41::ResizerByteBilinearOpenCv
        {
        protected:
            struct Idx
            {
                int32_t src, dst;
                uint8_t shuffle[DA];
            };
            Array<Idx> _ixg;
            int _fast;

            void EstimateParams();
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
            template<int fast> void RunG(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteBilinearOpenCv(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerShortBilinear : public Sse41::ResizerShortBilinear
        {
        protected:
            template<size_t N> void RunB(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
            template<size_t N> void RunS(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);

            virtual void Run(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
        public:
            ResizerShortBilinear(const ResParam& param);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerFloatBilinear : public Sse41::ResizerFloatBilinear
        {
        protected:
            virtual void Run(const float * src, size_t srcStride, float * dst, size_t dstStride);
        public:
            ResizerFloatBilinear(const ResParam & param);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerBf16Bilinear : public Sse41::ResizerBf16Bilinear
        {
        protected:
            virtual void Run(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
        public:
            ResizerBf16Bilinear(const ResParam& param);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteBicubic : public Sse41::ResizerByteBicubic
        {
        protected:
            template<int N> void RunS(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
            template<int N> void RunB(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteBicubic(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteArea1x1 : public Sse41::ResizerByteArea1x1
        {
        protected:
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteArea1x1(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteArea2x2 : public Sse41::ResizerByteArea2x2
        {
        protected:
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteArea2x2(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        void * ResizerInit(size_t srcX, size_t srcY, size_t dstX, size_t dstY, size_t channels, SimdResizeChannelType type, SimdResizeMethodType method);
    }
#endif 

#ifdef SIMD_AVX512BW_ENABLE    
    namespace Avx512bw
    {
        class ResizerNearest : public Avx2::ResizerNearest
        {
        protected:
            struct IndexShuffle32x2
            {
                int32_t src, dst;
                uint16_t shuffle[HA];
            };
            Array<IndexShuffle32x2> _ix32x2;
            Array<__mmask32> _tail32x2;

            void EstimateParams();
            void Shuffle32x2(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
            void Gather4(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
            void Gather8(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
            void FillConst(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
        public:
            ResizerNearest(const ResParam& param);

            static bool Preferable(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteBilinear : public Avx2::ResizerByteBilinear
        {
        protected:
            template<size_t N> void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
            void RunG(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        public:
            ResizerByteBilinear(const ResParam & param);

            virtual void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteBilinearOpenCv : public Avx2::ResizerByteBilinearOpenCv
        {
        protected:
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
            template<int fast> void RunG(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteBilinearOpenCv(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerShortBilinear : public Avx2::ResizerShortBilinear
        {
        protected:
            template<size_t N> void RunB(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
            template<size_t N> void RunS(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);

            virtual void Run(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
        public:
            ResizerShortBilinear(const ResParam& param);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerFloatBilinear : public Avx2::ResizerFloatBilinear
        {
            bool _fastLoad1, _fastLoad2;
        protected:
            virtual void Run(const float * src, size_t srcStride, float * dst, size_t dstStride);
        public:
            ResizerFloatBilinear(const ResParam & param);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerBf16Bilinear : public Avx2::ResizerBf16Bilinear
        {
        protected:
            virtual void Run(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
        public:
            ResizerBf16Bilinear(const ResParam& param);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteBicubic : public Avx2::ResizerByteBicubic
        {
        protected:
            template<int N> void RunS(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
            template<int N> void RunB(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteBicubic(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteArea1x1 : public Avx2::ResizerByteArea1x1
        {
        protected:
            template<size_t N> void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        public:
            ResizerByteArea1x1(const ResParam & param);

            virtual void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteArea2x2 : public Avx2::ResizerByteArea2x2
        {
        protected:
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteArea2x2(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        void * ResizerInit(size_t srcX, size_t srcY, size_t dstX, size_t dstY, size_t channels, SimdResizeChannelType type, SimdResizeMethodType method);
    }
#endif 

#ifdef SIMD_NEON_ENABLE    
    namespace Neon
    {
        class ResizerByteBilinear : public Base::ResizerByteBilinear
        {
        protected:
            Array8u _ax, _bx[2];
            size_t _blocks;
            struct Idx
            {
                int32_t src, dst;
                uint8_t shuffle[A];
            };
            Array<Idx> _ixg;

            size_t BlockCountMax(size_t align);
            void EstimateParams();
            template<size_t N> void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
            void RunG(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        public:
            ResizerByteBilinear(const ResParam & param);

            virtual void Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);
        };

        class ResizerShortBilinear : public Base::ResizerShortBilinear
        {
        protected:
            template<size_t N> void RunB(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
            template<size_t N> void RunS(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);

            virtual void Run(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
        public:
            ResizerShortBilinear(const ResParam& param);
        };

        class ResizerFloatBilinear : public Base::ResizerFloatBilinear
        {
        protected:
            virtual void Run(const float * src, size_t srcStride, float * dst, size_t dstStride);
        public:
            ResizerFloatBilinear(const ResParam & param);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteArea1x1 : public Base::ResizerByteArea1x1
        {
        protected:
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteArea1x1(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        void * ResizerInit(size_t srcX, size_t srcY, size_t dstX, size_t dstY, size_t channels, SimdResizeChannelType type, SimdResizeMethodType method);
    }
#endif 

#ifdef SIMD_SVE2_ENABLE
    namespace Sve2
    {
        class ResizerNearest : public Base::ResizerNearest
        {
        protected:
            size_t _blocks, _tails;
            struct IndexShuffle8x1
            {
                int32_t src, dst;
                uint8_t shuffle[SIMD_SVE2_VECTOR_SIZE_MAX];
            };
            Array<IndexShuffle8x1> _ix8x1;
            Array<size_t> _tail8x1;

            size_t BlockCountMax(size_t align);
            void EstimateParams();
            void Shuffle8x1(const uint8_t* src, size_t srcStride, size_t dyBeg, size_t dyEnd, uint8_t* dst, size_t dstStride);
        public:
            ResizerNearest(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteArea1x1 : public Base::ResizerByteArea1x1
        {
        protected:
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteArea1x1(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteArea2x2 : public Base::ResizerByteArea2x2
        {
        protected:
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteArea2x2(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteBilinear : public Base::ResizerByteBilinear
        {
        protected:
            Array8u _ax, _bx[2];
            size_t _blocks;
            struct Idx
            {
                int32_t src, dst;
                uint8_t shuffle[SIMD_SVE2_VECTOR_SIZE_MAX];
            };
            Array<Idx> _ixg;

            size_t BlockCountMax(size_t align);
            void EstimateParams();
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
            void RunG(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteBilinear(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteBilinearOpenCv : public Base::ResizerByteBilinearOpenCv
        {
        protected:
            Array8u _sx;

            void EstimateParams();
            template<size_t N> void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteBilinearOpenCv(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerFloatBilinear : public Base::ResizerFloatBilinear
        {
        protected:
            virtual void Run(const float* src, size_t srcStride, float* dst, size_t dstStride);
        public:
            ResizerFloatBilinear(const ResParam& param);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerBf16Bilinear : public Base::ResizerBf16Bilinear
        {
        protected:
            virtual void Run(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride);
        public:
            ResizerBf16Bilinear(const ResParam& param);
        };

        //-------------------------------------------------------------------------------------------------

        class ResizerByteBicubic : public Base::ResizerByteBicubic
        {
        protected:
            template<int N> void RunB(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        public:
            ResizerByteBicubic(const ResParam& param);

            virtual void Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);
        };

        //-------------------------------------------------------------------------------------------------

        void * ResizerInit(size_t srcX, size_t srcY, size_t dstX, size_t dstY, size_t channels, SimdResizeChannelType type, SimdResizeMethodType method);
    }
#endif
}
#endif
