/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2017 Yermalayeu Ihar.
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
#include "Simd/SimdDefs.h"

namespace Simd
{
    namespace Base
    {
        void Copy(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t pixelSize, uint8_t * dst, size_t dstStride)
        {
            size_t rowSize = width*pixelSize;
            for (size_t row = 0; row < height; ++row)
            {
                memcpy(dst, src, rowSize);
                src += srcStride;
                dst += dstStride;
            }
        }

        void CopyFrame(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t pixelSize,
            size_t frameLeft, size_t frameTop, size_t frameRight, size_t frameBottom, uint8_t * dst, size_t dstStride)
        {
            if (frameTop > frameBottom || frameBottom > height || frameLeft > frameRight || frameRight > width)
                return;

            if (frameTop > 0)
            {
                size_t srcOffset = 0;
                size_t dstOffset = 0;
                size_t size = width*pixelSize;
                for (size_t row = 0; row < frameTop; ++row)
                {
                    memcpy(dst + dstOffset, src + srcOffset, size);
                    srcOffset += srcStride;
                    dstOffset += dstStride;
                }
            }
            if (frameBottom < height)
            {
                size_t srcOffset = frameBottom*srcStride;
                size_t dstOffset = frameBottom*dstStride;
                size_t size = width*pixelSize;
                for (size_t row = frameBottom; row < height; ++row)
                {
                    memcpy(dst + dstOffset, src + srcOffset, size);
                    srcOffset += srcStride;
                    dstOffset += dstStride;
                }
            }
            if (frameLeft > 0)
            {
                size_t srcOffset = frameTop*srcStride;
                size_t dstOffset = frameTop*dstStride;
                size_t size = frameLeft*pixelSize;
                for (size_t row = frameTop; row < frameBottom; ++row)
                {
                    memcpy(dst + dstOffset, src + srcOffset, size);
                    srcOffset += srcStride;
                    dstOffset += dstStride;
                }
            }
            if (frameRight < width)
            {
                size_t srcOffset = frameTop*srcStride + frameRight*pixelSize;
                size_t dstOffset = frameTop*dstStride + frameRight*pixelSize;
                size_t size = (width - frameRight)*pixelSize;
                for (size_t row = frameTop; row < frameBottom; ++row)
                {
                    memcpy(dst + dstOffset, src + srcOffset, size);
                    srcOffset += srcStride;
                    dstOffset += dstStride;
                }
            }
        }
    }
}
