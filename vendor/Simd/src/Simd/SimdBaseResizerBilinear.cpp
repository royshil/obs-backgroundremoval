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
#include "Simd/SimdMemory.h"
#include "Simd/SimdResizer.h"
#include "Simd/SimdCopy.h"
#include "Simd/SimdBFloat16.h"

namespace Simd
{
    namespace Base
    {
        ResizerByteBilinear::ResizerByteBilinear(const ResParam & param)
            : Resizer(param)
        {
            _ay.Resize(_param.dstH);
            _iy.Resize(_param.dstH);
            EstimateIndexAlpha(_param.srcH, _param.dstH, 1, _iy.data, _ay.data);
        }        
        
        void ResizerByteBilinear::EstimateIndexAlpha(size_t srcSize, size_t dstSize, size_t channels, int32_t * indices, int32_t * alphas)
        {
            float scale = (float)srcSize / dstSize;

            for (size_t i = 0; i < dstSize; ++i)
            {
                float alpha = (float)((i + 0.5f)*scale - 0.5f);
                ptrdiff_t index = (ptrdiff_t)::floor(alpha);
                alpha -= index;

                if (index < 0)
                {
                    index = 0;
                    alpha = 0;
                }

                if (index >(ptrdiff_t)srcSize - 2)
                {
                    index = srcSize - 2;
                    alpha = 1;
                }

                for (size_t c = 0; c < channels; c++)
                {
                    size_t offset = i * channels + c;
                    indices[offset] = (int32_t)(channels*index + c);
                    alphas[offset] = (int32_t)(alpha * FRACTION_RANGE + 0.5f);
                }
            }
        }        

        void ResizerByteBilinear::Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride)
        {
            size_t cn =  _param.channels;
            size_t rs = _param.dstW * cn;
            if (_ax.data == 0)
            {
                _ax.Resize(rs);
                _ix.Resize(rs);
                EstimateIndexAlpha(_param.srcW, _param.dstW, cn, _ix.data, _ax.data);
                _bx[0].Resize(rs);
                _bx[1].Resize(rs);
            }
            int32_t * pbx[2] = { _bx[0].data, _bx[1].data };
            int32_t prev = -2;
            for (size_t dy = 0; dy < _param.dstH; dy++, dst += dstStride)
            {
                int32_t fy = _ay[dy];
                int32_t sy = _iy[dy];
                int32_t k = 0;

                if (sy == prev)
                    k = 2;
                else if (sy == prev + 1)
                {
                    Swap(pbx[0], pbx[1]);
                    k = 1;
                }

                prev = sy;

                for (; k < 2; k++)
                {
                    int32_t * pb = pbx[k];
                    const uint8_t * ps = src + (sy + k)*srcStride;
                    for (size_t dx = 0; dx < rs; dx++)
                    {
                        int32_t sx = _ix[dx];
                        int32_t fx = _ax[dx];
                        int32_t t = ps[sx];
                        pb[dx] = (t << LINEAR_SHIFT) + (ps[sx + cn] - t)*fx;
                    }
                }

                if (fy == 0)
                    for (size_t dx = 0; dx < rs; dx++)
                        dst[dx] = ((pbx[0][dx] << LINEAR_SHIFT) + BILINEAR_ROUND_TERM) >> BILINEAR_SHIFT;
                else if (fy == FRACTION_RANGE)
                    for (size_t dx = 0; dx < rs; dx++)
                        dst[dx] = ((pbx[1][dx] << LINEAR_SHIFT) + BILINEAR_ROUND_TERM) >> BILINEAR_SHIFT;
                else
                {
                    for (size_t dx = 0; dx < rs; dx++)
                    {
                        int32_t t = pbx[0][dx];
                        dst[dx] = ((t << LINEAR_SHIFT) + (pbx[1][dx] - t)*fy + BILINEAR_ROUND_TERM) >> BILINEAR_SHIFT;
                    }
                }
            }
        }

        //-------------------------------------------------------------------------------------------------

        ResizerByteBilinearOpenCv::ResizerByteBilinearOpenCv(const ResParam& param)
            : Resizer(param)
        {
            _iy.Resize(_param.dstH);
            _ay.Resize(_param.dstH * 2);
            EstimateIndexAlpha(_param.srcH, _param.dstH, 1, _iy.data, _ay.data, LINEAR_Y_RANGE);
        }
        void ResizerByteBilinearOpenCv::EstimateIndexAlpha(size_t srcSize, size_t dstSize, size_t channels, int32_t* indices, int16_t* alphas, int range)
        {
            float scale = float(srcSize) / float(dstSize);

            for (int i = 0; i < (int)dstSize; ++i)
            {
                float alpha = (float)((i + 0.5f) * scale - 0.5f);
                int32_t index = (int32_t)::floor(alpha);
                alpha -= index;

                if (index < 0)
                {
                    index = 0;
                    alpha = 0;
                }

                if (index > (int32_t)srcSize - 2)
                {
                    index = (int32_t)srcSize - 2;
                    alpha = 1;
                }

                for (size_t c = 0; c < channels; c++)
                {
                    size_t offset = i * channels + c;
                    indices[offset] = (int32_t)(channels * index + c);
                    int16_t ialpha = (int16_t)Round(alpha * range);
                    alphas[offset * 2 + 0] = range - ialpha;
                    alphas[offset * 2 + 1] = ialpha;
                }
            }
        }

        void ResizerByteBilinearOpenCv::Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride)
        {
            size_t cn = _param.channels;
            size_t rs = _param.dstW * cn;
            if (_ax.data == 0)
            {
                _ix.Resize(rs);
                _ax.Resize(rs * 2);
                EstimateIndexAlpha(_param.srcW, _param.dstW, cn, _ix.data, _ax.data, LINEAR_X_RANGE);
                _bx[0].Resize(rs);
                _bx[1].Resize(rs);
            }
            int16_t* pbx[2] = { _bx[0].data, _bx[1].data };
            int32_t prev = -2;
            for (size_t dy = 0; dy < _param.dstH; dy++, dst += dstStride)
            {
                int32_t sy = _iy[dy];
                int32_t fy0 = _ay[dy * 2 + 0], fy1 = _ay[dy * 2 + 1];
                int32_t k = 0;

                if (sy == prev)
                    k = 2;
                else if (sy == prev + 1)
                {
                    Swap(pbx[0], pbx[1]);
                    k = 1;
                }

                prev = sy;

                for (; k < 2; k++)
                {
                    int16_t* pb = pbx[k];
                    const uint8_t* ps = src + (sy + k) * srcStride;
                    for (size_t dx = 0; dx < rs; dx++)
                    {
                        int32_t sx = _ix[dx];
                        int32_t fx0 = _ax[dx * 2 + 0], fx1 = _ax[dx * 2 + 1];
                        pb[dx] = (ps[sx] * fx0 + ps[sx + cn] * fx1) >> LINEAR_X_RSHIFT;
                    }
                }

                if (fy0 == 0)
                    for (size_t dx = 0; dx < rs; dx++)
                        dst[dx] = dst[dx] = (((pbx[1][dx] * fy1) >> 16) + LINEAR_Y_ROUND) >> LINEAR_Y_RSHIFT;
                else if (fy1 == 0)
                    for (size_t dx = 0; dx < rs; dx++)
                        dst[dx] = dst[dx] = (((pbx[0][dx] * fy0) >> 16) + LINEAR_Y_ROUND) >> LINEAR_Y_RSHIFT;
                else
                {
                    for (size_t dx = 0; dx < rs; dx++)
                    {
                        dst[dx] = (((pbx[0][dx] * fy0) >> 16) + ((pbx[1][dx] * fy1) >> 16) + LINEAR_Y_ROUND) >> LINEAR_Y_RSHIFT;
                    }
                }
            }
        }

        //-------------------------------------------------------------------------------------------------

        ResizerShortBilinear::ResizerShortBilinear(const ResParam& param)
            : Resizer(param)
        {
            _ay.Resize(_param.dstH, false, _param.align);
            _iy.Resize(_param.dstH, false, _param.align);
            EstimateIndexAlpha(_param.srcH, _param.dstH, 1, _iy.data, _ay.data);
            size_t rs = _param.dstW * _param.channels + _param.align;
            _ax.Resize(rs, false, _param.align);
            _ix.Resize(rs, false, _param.align);
            EstimateIndexAlpha(_param.srcW, _param.dstW, _param.channels, _ix.data, _ax.data);
            _bx[0].Resize(rs, false, _param.align);
            _bx[1].Resize(rs, false, _param.align);
        }

        void ResizerShortBilinear::EstimateIndexAlpha(size_t srcSize, size_t dstSize, size_t channels, int32_t* indices, float* alphas)
        {
            float scale = (float)srcSize / dstSize;
            for (size_t i = 0; i < dstSize; ++i)
            {
                float alpha = (float)((i + 0.5f) * scale - 0.5f);
                ptrdiff_t index = (ptrdiff_t)::floor(alpha);
                alpha -= index;
                if (index < 0)
                {
                    index = 0;
                    alpha = 0;
                }
                if (index > (ptrdiff_t)srcSize - 2)
                {
                    index = srcSize - 2;
                    alpha = 1;
                }
                for (size_t c = 0; c < channels; c++)
                {
                    size_t offset = i * channels + c;
                    indices[offset] = (int32_t)(channels * index + c);
                    alphas[offset] = alpha;
                }
            }
        }

        void ResizerShortBilinear::Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride)
        {
            Run((const uint16_t*)src, srcStride / sizeof(uint16_t), (uint16_t*)dst, dstStride / sizeof(uint16_t));
        }

        template<size_t N> void ResizerShortBilinear::RunB(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride)
        {
            size_t rs = _param.dstW * N;
            float* pbx[2] = { _bx[0].data, _bx[1].data };
            int32_t prev = -2;
            for (size_t dy = 0; dy < _param.dstH; dy++, dst += dstStride)
            {
                float fy1 = _ay[dy];
                float fy0 = 1.0f - fy1;
                int32_t sy = _iy[dy];
                int32_t k = 0;
                if (sy == prev)
                    k = 2;
                else if (sy == prev + 1)
                {
                    Swap(pbx[0], pbx[1]);
                    k = 1;
                }
                prev = sy;
                for (; k < 2; k++)
                {
                    float* pb = pbx[k];
                    const uint16_t* ps = src + (sy + k) * srcStride;
                    for (size_t dx = 0; dx < rs; dx++)
                    {
                        int32_t sx = _ix[dx];
                        float fx = _ax[dx];
                        pb[dx] = ps[sx] * (1.0f - fx) + ps[sx + N] * fx;
                    }
                }
                for (size_t dx = 0; dx < rs; dx++)
                    dst[dx] = Round(pbx[0][dx] * fy0 + pbx[1][dx] * fy1);
            }
        }

        template<size_t N> void ResizerShortBilinear::RunS(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride)
        {
            size_t rs = _param.dstW * N;
            for (size_t dy = 0; dy < _param.dstH; dy++, dst += dstStride)
            {
                float fy1 = _ay[dy];
                float fy0 = 1.0f - fy1;
                int32_t sy = _iy[dy];
                const uint16_t* ps0 = src + (sy + 0) * srcStride;
                const uint16_t* ps1 = src + (sy + 1) * srcStride;
                for (size_t dx = 0; dx < rs; dx++)
                {
                    int32_t sx = _ix[dx];
                    float fx1 = _ax[dx];
                    float fx0 = 1.0f - fx1;
                    float r0 = ps0[sx] * fx0 + ps0[sx + N] * fx1;
                    float r1 = ps1[sx] * fx0 + ps1[sx + N] * fx1;
                    dst[dx] = Round(r0 * fy0 + r1 * fy1);
                }
            }
        }

        void ResizerShortBilinear::Run(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride)
        {
            bool sparse = _param.dstH * 2.0 <= _param.srcH;
            switch (_param.channels)
            {
            case 1: sparse ? RunS<1>(src, srcStride, dst, dstStride) : RunB<1>(src, srcStride, dst, dstStride); return;
            case 2: sparse ? RunS<2>(src, srcStride, dst, dstStride) : RunB<2>(src, srcStride, dst, dstStride); return;
            case 3: sparse ? RunS<3>(src, srcStride, dst, dstStride) : RunB<3>(src, srcStride, dst, dstStride); return;
            case 4: sparse ? RunS<4>(src, srcStride, dst, dstStride) : RunB<4>(src, srcStride, dst, dstStride); return;
            default:
                assert(0);
            }
        }

        //-------------------------------------------------------------------------------------------------

        static void EstimateIndexAlpha(const ResParam& param, size_t srcSize, size_t dstSize, size_t channels, size_t iaChannels, int32_t* indices, float* alphas)
        {
            if (param.method == SimdResizeMethodBilinear)
            {
                float scale = (float)srcSize / dstSize;
                for (size_t i = 0; i < dstSize; ++i)
                {
                    float alpha = (float)((i + 0.5f) * scale - 0.5f);
                    ptrdiff_t index = (ptrdiff_t)::floor(alpha);
                    alpha -= index;
                    if (index < 0)
                    {
                        index = 0;
                        alpha = 0;
                    }
                    if (index > (ptrdiff_t)srcSize - 2)
                    {
                        index = srcSize - 2;
                        alpha = 1;
                    }
                    for (size_t c = 0; c < iaChannels; c++)
                    {
                        size_t offset = i * iaChannels + c;
                        indices[offset] = (int32_t)(channels * index + c);
                        alphas[offset] = alpha;
                    }
                }
            }
            else if (param.method == SimdResizeMethodBilinearCaffe)
            {
                float scale = dstSize > 1 ? float(srcSize - 1) / float(dstSize - 1) : 0.0f;
                for (size_t i = 0; i < dstSize; ++i)
                {
                    float alpha = float(i) * scale;
                    ptrdiff_t index = (ptrdiff_t)::floor(alpha);
                    alpha -= index;
                    if (index > (ptrdiff_t)srcSize - 2)
                    {
                        index = srcSize - 2;
                        alpha = 1;
                    }
                    for (size_t c = 0; c < iaChannels; c++)
                    {
                        size_t offset = i * iaChannels + c;
                        indices[offset] = (int32_t)(channels * index + c);
                        alphas[offset] = alpha;
                    }
                }
            }
            else if (param.method == SimdResizeMethodBilinearPytorch)
            {
                float scale = (float)srcSize / dstSize;
                for (size_t i = 0; i < dstSize; ++i)
                {
                    float alpha = float(i) * scale;
                    ptrdiff_t index = (ptrdiff_t)::floor(alpha);
                    alpha -= index;
                    if (index < 0)
                    {
                        index = 0;
                        alpha = 0;
                    }
                    if (index > (ptrdiff_t)srcSize - 2)
                    {
                        index = srcSize - 2;
                        alpha = 1;
                    }
                    for (size_t c = 0; c < iaChannels; c++)
                    {
                        size_t offset = i * iaChannels + c;
                        indices[offset] = (int32_t)(channels * index + c);
                        alphas[offset] = alpha;
                    }
                }
            }
            else
                assert(0);
        }

        //-------------------------------------------------------------------------------------------------

        ResizerFloatBilinear::ResizerFloatBilinear(const ResParam & param)
            : Resizer(param)
        {
            _rowBuf = _param.align < 16 || _param.dstH >= _param.srcH;
#if defined(SIMD_ARM_ENABLE) || defined(SIMD_ARM64_ENABLE)
            _rowBuf = true;
#endif
            _ay.Resize(_param.dstH, false, _param.align);
            _iy.Resize(_param.dstH, false, _param.align);
            EstimateIndexAlpha(_param, _param.srcH, _param.dstH, 1, 1, _iy.data, _ay.data);
            size_t rs = _param.dstW * (_rowBuf ? _param.channels : 1);
            _ax.Resize(rs, false, _param.align);
            _ix.Resize(rs, false, _param.align);
            EstimateIndexAlpha(_param, _param.srcW, _param.dstW, _param.channels, _rowBuf ? _param.channels : 1, _ix.data, _ax.data);
            if (_rowBuf)
            {
                _bx[0].Resize(rs, false, _param.align);
                _bx[1].Resize(rs, false, _param.align);
            }
        }

        void ResizerFloatBilinear::Run(const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride)
        {
            Run((const float*)src, srcStride / sizeof(float), (float*)dst, dstStride / sizeof(float));
        }

        void ResizerFloatBilinear::Run(const float * src, size_t srcStride, float * dst, size_t dstStride)
        {
            size_t cn = _param.channels;
            size_t rs = _param.dstW * cn;
            float * pbx[2] = { _bx[0].data, _bx[1].data };
            int32_t prev = -2;
            for (size_t dy = 0; dy < _param.dstH; dy++, dst += dstStride)
            {
                float fy1 = _ay[dy];
                float fy0 = 1.0f - fy1;
                int32_t sy = _iy[dy];
                int32_t k = 0;

                if (sy == prev)
                    k = 2;
                else if (sy == prev + 1)
                {
                    Swap(pbx[0], pbx[1]);
                    k = 1;
                }

                prev = sy;

                for (; k < 2; k++)
                {
                    float * pb = pbx[k];
                    const float * ps = src + (sy + k)*srcStride;
                    for (size_t dx = 0; dx < rs; dx++)
                    {
                        int32_t sx = _ix[dx];
                        float fx = _ax[dx];
                        pb[dx] = ps[sx]*(1.0f - fx) + ps[sx + cn]*fx;
                    }
                }

                for (size_t dx = 0; dx < rs; dx++)
                    dst[dx] = pbx[0][dx]*fy0 + pbx[1][dx]*fy1;
            }
        }

        //-------------------------------------------------------------------------------------------------

        ResizerBf16Bilinear::ResizerBf16Bilinear(const ResParam& param)
            : Resizer(param)
        {
            _rowBuf = _param.align < 16 || _param.channels < 4 || _param.dstH >= _param.srcH;
            _ay.Resize(_param.dstH, false, _param.align);
            _iy.Resize(_param.dstH, false, _param.align);
            EstimateIndexAlpha(_param, _param.srcH, _param.dstH, 1, 1, _iy.data, _ay.data);
            size_t rs = _param.dstW * (_rowBuf ? _param.channels : 1);
            _ax.Resize(rs, false, _param.align);
            _ix.Resize(rs, false, _param.align);
            EstimateIndexAlpha(_param, _param.srcW, _param.dstW, _param.channels, _rowBuf ? _param.channels : 1, _ix.data, _ax.data);
            if (_rowBuf)
            {
                _bx[0].Resize(rs, false, _param.align);
                _bx[1].Resize(rs, false, _param.align);
            }
        }

        void ResizerBf16Bilinear::Run(const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride)
        {
            Run((const uint16_t*)src, srcStride / sizeof(uint16_t), (uint16_t*)dst, dstStride / sizeof(uint16_t));
        }

        void ResizerBf16Bilinear::Run(const uint16_t* src, size_t srcStride, uint16_t* dst, size_t dstStride)
        {
            size_t cn = _param.channels;
            size_t rs = _param.dstW * cn;
            float* pbx[2] = { _bx[0].data, _bx[1].data };
            int32_t prev = -2;
            for (size_t dy = 0; dy < _param.dstH; dy++, dst += dstStride)
            {
                float fy1 = _ay[dy];
                float fy0 = 1.0f - fy1;
                int32_t sy = _iy[dy];
                int32_t k = 0;

                if (sy == prev)
                    k = 2;
                else if (sy == prev + 1)
                {
                    Swap(pbx[0], pbx[1]);
                    k = 1;
                }

                prev = sy;

                for (; k < 2; k++)
                {
                    float* pb = pbx[k];
                    const uint16_t* ps = src + (sy + k) * srcStride;
                    for (size_t dx = 0; dx < rs; dx++)
                    {
                        int32_t sx = _ix[dx];
                        float fx = _ax[dx];
                        pb[dx] = BFloat16ToFloat32(ps[sx]) * (1.0f - fx) + BFloat16ToFloat32(ps[sx + cn]) * fx;
                    }
                }

                for (size_t dx = 0; dx < rs; dx++)
                    dst[dx] = Float32ToBFloat16(pbx[0][dx] * fy0 + pbx[1][dx] * fy1);
            }
        }
    }
}

