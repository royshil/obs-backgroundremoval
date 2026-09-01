/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2026 Yermalayeu Ihar,
*               2014-2015 Antonenka Mikhail,
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
#ifndef __SimdConst_h__
#define __SimdConst_h__

#include "Simd/SimdInit.h"

namespace Simd
{
    const size_t HISTOGRAM_SIZE = UCHAR_MAX + 1;

    namespace Base
    {
        const int LINEAR_SHIFT = 4;
        const int LINEAR_ROUND_TERM = 1 << (LINEAR_SHIFT - 1);

        const int BILINEAR_SHIFT = LINEAR_SHIFT * 2;
        const int BILINEAR_ROUND_TERM = 1 << (BILINEAR_SHIFT - 1);

        const int FRACTION_RANGE = 1 << LINEAR_SHIFT;
        const double FRACTION_ROUND_TERM = 0.5 / FRACTION_RANGE;

        const float KF_255_DIV_6 = 255.0f / 6.0f;

        const int BGR_TO_GRAY_AVERAGING_SHIFT = 14;
        const int BGR_TO_GRAY_ROUND_TERM = 1 << (BGR_TO_GRAY_AVERAGING_SHIFT - 1);
        const int BLUE_TO_GRAY_WEIGHT = int(0.114*(1 << BGR_TO_GRAY_AVERAGING_SHIFT) + 0.5);
        const int GREEN_TO_GRAY_WEIGHT = int(0.587*(1 << BGR_TO_GRAY_AVERAGING_SHIFT) + 0.5);
        const int RED_TO_GRAY_WEIGHT = int(0.299*(1 << BGR_TO_GRAY_AVERAGING_SHIFT) + 0.5);

        const int Y_ADJUST = 16;
        const int UV_ADJUST = 128;
        const int YUV_TO_BGR_AVERAGING_SHIFT = 13;
        const int YUV_TO_BGR_ROUND_TERM = 1 << (YUV_TO_BGR_AVERAGING_SHIFT - 1);
        const int Y_TO_RGB_WEIGHT = int(1.164*(1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);
        const int U_TO_BLUE_WEIGHT = int(2.018*(1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);
        const int U_TO_GREEN_WEIGHT = -int(0.391*(1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);
        const int V_TO_GREEN_WEIGHT = -int(0.813*(1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);
        const int V_TO_RED_WEIGHT = int(1.596*(1 << YUV_TO_BGR_AVERAGING_SHIFT) + 0.5);

        const int BGR_TO_YUV_AVERAGING_SHIFT = 14;
        const int BGR_TO_YUV_ROUND_TERM = 1 << (BGR_TO_YUV_AVERAGING_SHIFT - 1);
        const int BLUE_TO_Y_WEIGHT = int(0.098*(1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
        const int GREEN_TO_Y_WEIGHT = int(0.504*(1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
        const int RED_TO_Y_WEIGHT = int(0.257*(1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
        const int BLUE_TO_U_WEIGHT = int(0.439*(1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
        const int GREEN_TO_U_WEIGHT = -int(0.291*(1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
        const int RED_TO_U_WEIGHT = -int(0.148*(1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
        const int BLUE_TO_V_WEIGHT = -int(0.071*(1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
        const int GREEN_TO_V_WEIGHT = -int(0.368*(1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);
        const int RED_TO_V_WEIGHT = int(0.439*(1 << BGR_TO_YUV_AVERAGING_SHIFT) + 0.5);

        const int DIVISION_BY_9_SHIFT = 16;
        const int DIVISION_BY_9_FACTOR = (1 << DIVISION_BY_9_SHIFT) / 9;
    }

#ifdef SIMD_SSE41_ENABLE    
    namespace Sse41
    {
        const size_t F = sizeof(__m128) / sizeof(float);
        const size_t DF = 2 * F;
        const size_t QF = 4 * F;
        const size_t HF = F / 2;

        const size_t A = sizeof(__m128i);
        const size_t DA = 2 * A;
        const size_t QA = 4 * A;
        const size_t OA = 8 * A;
        const size_t HA = A / 2;

        const __m128i K_ZERO = SIMD_MM_SET1_EPI8(0);
        const __m128i K_INV_ZERO = SIMD_MM_SET1_EPI8(0xFF);

        const __m128i K8_01 = SIMD_MM_SET1_EPI8(0x01);
        const __m128i K8_02 = SIMD_MM_SET1_EPI8(0x02);
        const __m128i K8_03 = SIMD_MM_SET1_EPI8(0x03);
        const __m128i K8_04 = SIMD_MM_SET1_EPI8(0x04);
        const __m128i K8_07 = SIMD_MM_SET1_EPI8(0x07);
        const __m128i K8_08 = SIMD_MM_SET1_EPI8(0x08);
        const __m128i K8_0F = SIMD_MM_SET1_EPI8(0x0F);
        const __m128i K8_10 = SIMD_MM_SET1_EPI8(0x10);
        const __m128i K8_20 = SIMD_MM_SET1_EPI8(0x20);
        const __m128i K8_40 = SIMD_MM_SET1_EPI8(0x40);
        const __m128i K8_80 = SIMD_MM_SET1_EPI8(0x80);

        const __m128i K8_01_FF = SIMD_MM_SET2_EPI8(0x01, 0xFF);

        const __m128i K16_0001 = SIMD_MM_SET1_EPI16(0x0001);
        const __m128i K16_0002 = SIMD_MM_SET1_EPI16(0x0002);
        const __m128i K16_0003 = SIMD_MM_SET1_EPI16(0x0003);
        const __m128i K16_0004 = SIMD_MM_SET1_EPI16(0x0004);
        const __m128i K16_0005 = SIMD_MM_SET1_EPI16(0x0005);
        const __m128i K16_0006 = SIMD_MM_SET1_EPI16(0x0006);
        const __m128i K16_0008 = SIMD_MM_SET1_EPI16(0x0008);
        const __m128i K16_0020 = SIMD_MM_SET1_EPI16(0x0020);
        const __m128i K16_0080 = SIMD_MM_SET1_EPI16(0x0080);
        const __m128i K16_00FF = SIMD_MM_SET1_EPI16(0x00FF);
        const __m128i K16_0101 = SIMD_MM_SET1_EPI16(0x0101);
        const __m128i K16_FF00 = SIMD_MM_SET1_EPI16(0xFF00);

        const __m128i K32_00000001 = SIMD_MM_SET1_EPI32(0x00000001);
        const __m128i K32_00000002 = SIMD_MM_SET1_EPI32(0x00000002);
        const __m128i K32_00000004 = SIMD_MM_SET1_EPI32(0x00000004);
        const __m128i K32_00000008 = SIMD_MM_SET1_EPI32(0x00000008);
        const __m128i K32_000000FF = SIMD_MM_SET1_EPI32(0x000000FF);
        const __m128i K32_0000FFFF = SIMD_MM_SET1_EPI32(0x0000FFFF);
        const __m128i K32_00010000 = SIMD_MM_SET1_EPI32(0x00010000);
        const __m128i K32_01000000 = SIMD_MM_SET1_EPI32(0x01000000);
        const __m128i K32_00FF0000 = SIMD_MM_SET1_EPI32(0x00FF0000);
        const __m128i K32_00FFFFFF = SIMD_MM_SET1_EPI32(0x00FFFFFF);
        const __m128i K32_FF000000 = SIMD_MM_SET1_EPI32(0xFF000000);
        const __m128i K32_FFFFFF00 = SIMD_MM_SET1_EPI32(0xFFFFFF00);


        const __m128i K64_00000000FFFFFFFF = SIMD_MM_SET2_EPI32(0xFFFFFFFF, 0);

        const __m128i K16_Y_ADJUST = SIMD_MM_SET1_EPI16(Base::Y_ADJUST);
        const __m128i K16_UV_ADJUST = SIMD_MM_SET1_EPI16(Base::UV_ADJUST);

        const __m128i K16_YRGB_RT = SIMD_MM_SET2_EPI16(Base::Y_TO_RGB_WEIGHT, Base::YUV_TO_BGR_ROUND_TERM);
        const __m128i K16_VR_0 = SIMD_MM_SET2_EPI16(Base::V_TO_RED_WEIGHT, 0);
        const __m128i K16_UG_VG = SIMD_MM_SET2_EPI16(Base::U_TO_GREEN_WEIGHT, Base::V_TO_GREEN_WEIGHT);
        const __m128i K16_UB_0 = SIMD_MM_SET2_EPI16(Base::U_TO_BLUE_WEIGHT, 0);

        const __m128i K16_BY_RY = SIMD_MM_SET2_EPI16(Base::BLUE_TO_Y_WEIGHT, Base::RED_TO_Y_WEIGHT);
        const __m128i K16_GY_RT = SIMD_MM_SET2_EPI16(Base::GREEN_TO_Y_WEIGHT, Base::BGR_TO_YUV_ROUND_TERM);
        const __m128i K16_BU_RU = SIMD_MM_SET2_EPI16(Base::BLUE_TO_U_WEIGHT, Base::RED_TO_U_WEIGHT);
        const __m128i K16_GU_RT = SIMD_MM_SET2_EPI16(Base::GREEN_TO_U_WEIGHT, Base::BGR_TO_YUV_ROUND_TERM);
        const __m128i K16_BV_RV = SIMD_MM_SET2_EPI16(Base::BLUE_TO_V_WEIGHT, Base::RED_TO_V_WEIGHT);
        const __m128i K16_GV_RT = SIMD_MM_SET2_EPI16(Base::GREEN_TO_V_WEIGHT, Base::BGR_TO_YUV_ROUND_TERM);

        const __m128i K16_DIVISION_BY_9_FACTOR = SIMD_MM_SET1_EPI16(Base::DIVISION_BY_9_FACTOR);

        const __m128i K8_SHUFFLE_GRAY_TO_BGR0 = SIMD_MM_SETR_EPI8(0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x3, 0x3, 0x3, 0x4, 0x4, 0x4, 0x5);
        const __m128i K8_SHUFFLE_GRAY_TO_BGR1 = SIMD_MM_SETR_EPI8(0x5, 0x5, 0x6, 0x6, 0x6, 0x7, 0x7, 0x7, 0x8, 0x8, 0x8, 0x9, 0x9, 0x9, 0xA, 0xA);
        const __m128i K8_SHUFFLE_GRAY_TO_BGR2 = SIMD_MM_SETR_EPI8(0xA, 0xB, 0xB, 0xB, 0xC, 0xC, 0xC, 0xD, 0xD, 0xD, 0xE, 0xE, 0xE, 0xF, 0xF, 0xF);

        const __m128i K8_SHUFFLE_BLUE_TO_BGR0 = SIMD_MM_SETR_EPI8(0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1, 0x5);
        const __m128i K8_SHUFFLE_BLUE_TO_BGR1 = SIMD_MM_SETR_EPI8(-1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1);
        const __m128i K8_SHUFFLE_BLUE_TO_BGR2 = SIMD_MM_SETR_EPI8(-1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1, -1);

        const __m128i K8_SHUFFLE_GREEN_TO_BGR0 = SIMD_MM_SETR_EPI8(-1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1);
        const __m128i K8_SHUFFLE_GREEN_TO_BGR1 = SIMD_MM_SETR_EPI8(0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA);
        const __m128i K8_SHUFFLE_GREEN_TO_BGR2 = SIMD_MM_SETR_EPI8(-1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1);

        const __m128i K8_SHUFFLE_RED_TO_BGR0 = SIMD_MM_SETR_EPI8(-1, -1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1);
        const __m128i K8_SHUFFLE_RED_TO_BGR1 = SIMD_MM_SETR_EPI8(-1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1);
        const __m128i K8_SHUFFLE_RED_TO_BGR2 = SIMD_MM_SETR_EPI8(0xA, -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF);

        const __m128i K8_SHUFFLE_BGR0_TO_BLUE = SIMD_MM_SETR_EPI8(0x0, 0x3, 0x6, 0x9, 0xC, 0xF, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
        const __m128i K8_SHUFFLE_BGR1_TO_BLUE = SIMD_MM_SETR_EPI8(-1, -1, -1, -1, -1, -1, 0x2, 0x5, 0x8, 0xB, 0xE, -1, -1, -1, -1, -1);
        const __m128i K8_SHUFFLE_BGR2_TO_BLUE = SIMD_MM_SETR_EPI8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x1, 0x4, 0x7, 0xA, 0xD);

        const __m128i K8_SHUFFLE_BGR0_TO_GREEN = SIMD_MM_SETR_EPI8(0x1, 0x4, 0x7, 0xA, 0xD, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
        const __m128i K8_SHUFFLE_BGR1_TO_GREEN = SIMD_MM_SETR_EPI8(-1, -1, -1, -1, -1, 0x0, 0x3, 0x6, 0x9, 0xC, 0xF, -1, -1, -1, -1, -1);
        const __m128i K8_SHUFFLE_BGR2_TO_GREEN = SIMD_MM_SETR_EPI8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x2, 0x5, 0x8, 0xB, 0xE);

        const __m128i K8_SHUFFLE_BGR0_TO_RED = SIMD_MM_SETR_EPI8(0x2, 0x5, 0x8, 0xB, 0xE, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
        const __m128i K8_SHUFFLE_BGR1_TO_RED = SIMD_MM_SETR_EPI8(-1, -1, -1, -1, -1, 0x1, 0x4, 0x7, 0xA, 0xD, -1, -1, -1, -1, -1, -1);
        const __m128i K8_SHUFFLE_BGR2_TO_RED = SIMD_MM_SETR_EPI8(-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x0, 0x3, 0x6, 0x9, 0xC, 0xF);
    }
#endif

#ifdef SIMD_AVX2_ENABLE    
    namespace Avx2
    {
        const size_t F = sizeof(__m256) / sizeof(float);
        const size_t DF = 2 * F;
        const size_t QF = 4 * F;
        const size_t HF = F / 2;

        const size_t A = sizeof(__m256i);
        const size_t DA = 2 * A;
        const size_t QA = 4 * A;
        const size_t OA = 8 * A;
        const size_t HA = A / 2;

        const __m256i K_ZERO = SIMD_MM256_SET1_EPI8(0);
        const __m256i K_INV_ZERO = SIMD_MM256_SET1_EPI8(0xFF);

        const __m256i K8_01 = SIMD_MM256_SET1_EPI8(0x01);
        const __m256i K8_02 = SIMD_MM256_SET1_EPI8(0x02);
        const __m256i K8_03 = SIMD_MM256_SET1_EPI8(0x03);
        const __m256i K8_04 = SIMD_MM256_SET1_EPI8(0x04);
        const __m256i K8_07 = SIMD_MM256_SET1_EPI8(0x07);
        const __m256i K8_08 = SIMD_MM256_SET1_EPI8(0x08);
        const __m256i K8_0F = SIMD_MM256_SET1_EPI8(0x0F);
        const __m256i K8_10 = SIMD_MM256_SET1_EPI8(0x10);
        const __m256i K8_20 = SIMD_MM256_SET1_EPI8(0x20);
        const __m256i K8_40 = SIMD_MM256_SET1_EPI8(0x40);
        const __m256i K8_80 = SIMD_MM256_SET1_EPI8(0x80);

        const __m256i K8_01_FF = SIMD_MM256_SET2_EPI8(0x01, 0xFF);

        const __m256i K16_0001 = SIMD_MM256_SET1_EPI16(0x0001);
        const __m256i K16_0002 = SIMD_MM256_SET1_EPI16(0x0002);
        const __m256i K16_0003 = SIMD_MM256_SET1_EPI16(0x0003);
        const __m256i K16_0004 = SIMD_MM256_SET1_EPI16(0x0004);
        const __m256i K16_0005 = SIMD_MM256_SET1_EPI16(0x0005);
        const __m256i K16_0006 = SIMD_MM256_SET1_EPI16(0x0006);
        const __m256i K16_0008 = SIMD_MM256_SET1_EPI16(0x0008);
        const __m256i K16_0010 = SIMD_MM256_SET1_EPI16(0x0010);
        const __m256i K16_0018 = SIMD_MM256_SET1_EPI16(0x0018);
        const __m256i K16_0020 = SIMD_MM256_SET1_EPI16(0x0020);
        const __m256i K16_0080 = SIMD_MM256_SET1_EPI16(0x0080);
        const __m256i K16_00FF = SIMD_MM256_SET1_EPI16(0x00FF);
        const __m256i K16_0101 = SIMD_MM256_SET1_EPI16(0x0101);
        const __m256i K16_FF00 = SIMD_MM256_SET1_EPI16(0xFF00);

        const __m256i K32_00000001 = SIMD_MM256_SET1_EPI32(0x00000001);
        const __m256i K32_00000002 = SIMD_MM256_SET1_EPI32(0x00000002);
        const __m256i K32_00000004 = SIMD_MM256_SET1_EPI32(0x00000004);
        const __m256i K32_00000008 = SIMD_MM256_SET1_EPI32(0x00000008);
        const __m256i K32_000000FF = SIMD_MM256_SET1_EPI32(0x000000FF);
        const __m256i K32_0000FFFF = SIMD_MM256_SET1_EPI32(0x0000FFFF);
        const __m256i K32_00010000 = SIMD_MM256_SET1_EPI32(0x00010000);
        const __m256i K32_01000000 = SIMD_MM256_SET1_EPI32(0x01000000);
        const __m256i K32_00FF0000 = SIMD_MM256_SET1_EPI32(0x00FF0000);
        const __m256i K32_FF000000 = SIMD_MM256_SET1_EPI32(0xFF000000);
        const __m256i K32_FFFFFF00 = SIMD_MM256_SET1_EPI32(0xFFFFFF00);

        const __m256i K16_Y_ADJUST = SIMD_MM256_SET1_EPI16(Base::Y_ADJUST);
        const __m256i K16_UV_ADJUST = SIMD_MM256_SET1_EPI16(Base::UV_ADJUST);

        const __m256i K16_YRGB_RT = SIMD_MM256_SET2_EPI16(Base::Y_TO_RGB_WEIGHT, Base::YUV_TO_BGR_ROUND_TERM);
        const __m256i K16_VR_0 = SIMD_MM256_SET2_EPI16(Base::V_TO_RED_WEIGHT, 0);
        const __m256i K16_UG_VG = SIMD_MM256_SET2_EPI16(Base::U_TO_GREEN_WEIGHT, Base::V_TO_GREEN_WEIGHT);
        const __m256i K16_UB_0 = SIMD_MM256_SET2_EPI16(Base::U_TO_BLUE_WEIGHT, 0);

        const __m256i K16_BY_RY = SIMD_MM256_SET2_EPI16(Base::BLUE_TO_Y_WEIGHT, Base::RED_TO_Y_WEIGHT);
        const __m256i K16_GY_RT = SIMD_MM256_SET2_EPI16(Base::GREEN_TO_Y_WEIGHT, Base::BGR_TO_YUV_ROUND_TERM);
        const __m256i K16_BU_RU = SIMD_MM256_SET2_EPI16(Base::BLUE_TO_U_WEIGHT, Base::RED_TO_U_WEIGHT);
        const __m256i K16_GU_RT = SIMD_MM256_SET2_EPI16(Base::GREEN_TO_U_WEIGHT, Base::BGR_TO_YUV_ROUND_TERM);
        const __m256i K16_BV_RV = SIMD_MM256_SET2_EPI16(Base::BLUE_TO_V_WEIGHT, Base::RED_TO_V_WEIGHT);
        const __m256i K16_GV_RT = SIMD_MM256_SET2_EPI16(Base::GREEN_TO_V_WEIGHT, Base::BGR_TO_YUV_ROUND_TERM);

        const __m256i K16_DIVISION_BY_9_FACTOR = SIMD_MM256_SET1_EPI16(Base::DIVISION_BY_9_FACTOR);

        const __m256i K64_00000000FFFFFFFF = SIMD_MM256_SET2_EPI32(0xFFFFFFFF, 0);

        const __m256i K8_SHUFFLE_0 = SIMD_MM256_SETR_EPI8(
            0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70,
            0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0);

        const __m256i K8_SHUFFLE_1 = SIMD_MM256_SETR_EPI8(
            0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0, 0xF0,
            0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70, 0x70);

        const __m256i K8_SHUFFLE_GRAY_TO_BGR0 = SIMD_MM256_SETR_EPI8(
            0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x3, 0x3, 0x3, 0x4, 0x4, 0x4, 0x5,
            0x5, 0x5, 0x6, 0x6, 0x6, 0x7, 0x7, 0x7, 0x8, 0x8, 0x8, 0x9, 0x9, 0x9, 0xA, 0xA);
        const __m256i K8_SHUFFLE_GRAY_TO_BGR1 = SIMD_MM256_SETR_EPI8(
            0x2, 0x3, 0x3, 0x3, 0x4, 0x4, 0x4, 0x5, 0x5, 0x5, 0x6, 0x6, 0x6, 0x7, 0x7, 0x7,
            0x8, 0x8, 0x8, 0x9, 0x9, 0x9, 0xA, 0xA, 0xA, 0xB, 0xB, 0xB, 0xC, 0xC, 0xC, 0xD);
        const __m256i K8_SHUFFLE_GRAY_TO_BGR2 = SIMD_MM256_SETR_EPI8(
            0x5, 0x5, 0x6, 0x6, 0x6, 0x7, 0x7, 0x7, 0x8, 0x8, 0x8, 0x9, 0x9, 0x9, 0xA, 0xA,
            0xA, 0xB, 0xB, 0xB, 0xC, 0xC, 0xC, 0xD, 0xD, 0xD, 0xE, 0xE, 0xE, 0xF, 0xF, 0xF);

        const __m256i K8_SHUFFLE_PERMUTED_BLUE_TO_BGR0 = SIMD_MM256_SETR_EPI8(
            0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1, 0x5,
            -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1);
        const __m256i K8_SHUFFLE_PERMUTED_BLUE_TO_BGR1 = SIMD_MM256_SETR_EPI8(
            -1, 0x3, -1, -1, 0x4, -1, -1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1,
            0x8, -1, -1, 0x9, -1, -1, 0xA, -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD);
        const __m256i K8_SHUFFLE_PERMUTED_BLUE_TO_BGR2 = SIMD_MM256_SETR_EPI8(
            -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1,
            -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1, -1);

        const __m256i K8_SHUFFLE_PERMUTED_GREEN_TO_BGR0 = SIMD_MM256_SETR_EPI8(
            -1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1,
            0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA);
        const __m256i K8_SHUFFLE_PERMUTED_GREEN_TO_BGR1 = SIMD_MM256_SETR_EPI8(
            -1, -1, 0x3, -1, -1, 0x4, -1, -1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1,
            -1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1, -1, 0xB, -1, -1, 0xC, -1, -1);
        const __m256i K8_SHUFFLE_PERMUTED_GREEN_TO_BGR2 = SIMD_MM256_SETR_EPI8(
            0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA,
            -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1);

        const __m256i K8_SHUFFLE_PERMUTED_RED_TO_BGR0 = SIMD_MM256_SETR_EPI8(
            -1, -1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1,
            -1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1);
        const __m256i K8_SHUFFLE_PERMUTED_RED_TO_BGR1 = SIMD_MM256_SETR_EPI8(
            0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1, 0x5, -1, -1, 0x6, -1, -1, 0x7,
            -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1, -1, 0xB, -1, -1, 0xC, -1);
        const __m256i K8_SHUFFLE_PERMUTED_RED_TO_BGR2 = SIMD_MM256_SETR_EPI8(
            -1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1,
            0xA, -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF);

        const __m256i K8_SHUFFLE_BGR0_TO_BLUE = SIMD_MM256_SETR_EPI8(
            0x0, 0x3, 0x6, 0x9, 0xC, 0xF, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, 0x2, 0x5, 0x8, 0xB, 0xE, -1, -1, -1, -1, -1);
        const __m256i K8_SHUFFLE_BGR1_TO_BLUE = SIMD_MM256_SETR_EPI8(
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x1, 0x4, 0x7, 0xA, 0xD,
            0x0, 0x3, 0x6, 0x9, 0xC, 0xF, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
        const __m256i K8_SHUFFLE_BGR2_TO_BLUE = SIMD_MM256_SETR_EPI8(
            -1, -1, -1, -1, -1, -1, 0x2, 0x5, 0x8, 0xB, 0xE, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x1, 0x4, 0x7, 0xA, 0xD);

        const __m256i K8_SHUFFLE_BGR0_TO_GREEN = SIMD_MM256_SETR_EPI8(
            0x1, 0x4, 0x7, 0xA, 0xD, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, 0x0, 0x3, 0x6, 0x9, 0xC, 0xF, -1, -1, -1, -1, -1);
        const __m256i K8_SHUFFLE_BGR1_TO_GREEN = SIMD_MM256_SETR_EPI8(
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x2, 0x5, 0x8, 0xB, 0xE,
            0x1, 0x4, 0x7, 0xA, 0xD, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
        const __m256i K8_SHUFFLE_BGR2_TO_GREEN = SIMD_MM256_SETR_EPI8(
            -1, -1, -1, -1, -1, 0x0, 0x3, 0x6, 0x9, 0xC, 0xF, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x2, 0x5, 0x8, 0xB, 0xE);

        const __m256i K8_SHUFFLE_BGR0_TO_RED = SIMD_MM256_SETR_EPI8(
            0x2, 0x5, 0x8, 0xB, 0xE, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, 0x1, 0x4, 0x7, 0xA, 0xD, -1, -1, -1, -1, -1, -1);
        const __m256i K8_SHUFFLE_BGR1_TO_RED = SIMD_MM256_SETR_EPI8(
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x0, 0x3, 0x6, 0x9, 0xC, 0xF,
            0x2, 0x5, 0x8, 0xB, 0xE, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1);
        const __m256i K8_SHUFFLE_BGR2_TO_RED = SIMD_MM256_SETR_EPI8(
            -1, -1, -1, -1, -1, 0x1, 0x4, 0x7, 0xA, 0xD, -1, -1, -1, -1, -1, -1,
            -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0x0, 0x3, 0x6, 0x9, 0xC, 0xF);

        const __m256i K8_BGR_TO_BGRA_SHUFFLE = SIMD_MM256_SETR_EPI8(
            0x0, 0x1, 0x2, -1, 0x3, 0x4, 0x5, -1, 0x6, 0x7, 0x8, -1, 0x9, 0xA, 0xB, -1,
            0x4, 0x5, 0x6, -1, 0x7, 0x8, 0x9, -1, 0xA, 0xB, 0xC, -1, 0xD, 0xE, 0xF, -1);

        const __m256i K8_RGB_TO_BGRA_SHUFFLE = SIMD_MM256_SETR_EPI8(
            0x2, 0x1, 0x0, -1, 0x5, 0x4, 0x3, -1, 0x8, 0x7, 0x6, -1, 0xB, 0xA, 0x9, -1,
            0x6, 0x5, 0x4, -1, 0x9, 0x8, 0x7, -1, 0xC, 0xB, 0xA, -1, 0xF, 0xE, 0xD, -1);

        const __m256i K32_TWO_UNPACK_PERMUTE = SIMD_MM256_SETR_EPI32(0, 2, 4, 6, 1, 3, 5, 7);

        const __m256i K8_SHUFFLE_BGRA_TO_BGR = SIMD_MM256_SETR_EPI8(
            0x0, 0x1, 0x2, 0x4, 0x5, 0x6, 0x8, 0x9, 0xA, 0xC, 0xD, 0xE, -1, -1, -1, -1,
            0x0, 0x1, 0x2, 0x4, 0x5, 0x6, 0x8, 0x9, 0xA, 0xC, 0xD, 0xE, -1, -1, -1, -1);

        const __m256i K32_PERMUTE_BGRA_TO_BGR = SIMD_MM256_SETR_EPI32(0x0, 0x1, 0x2, 0x4, 0x5, 0x6, -1, -1);
    }
#endif

#ifdef SIMD_AVX512BW_ENABLE    
    namespace Avx512bw
    {
        const size_t F = sizeof(__m512) / sizeof(float);
        const size_t DF = 2 * F;
        const size_t QF = 4 * F;
        const size_t HF = F / 2;

        const size_t A = sizeof(__m512i);
        const size_t DA = 2 * A;
        const size_t QA = 4 * A;
        const size_t HA = A / 2;

        const __m512i K_ZERO = SIMD_MM512_SET1_EPI8(0);
        const __m512i K_INV_ZERO = SIMD_MM512_SET1_EPI8(0xFF);

        const __m512i K8_01 = SIMD_MM512_SET1_EPI8(0x01);
        const __m512i K8_02 = SIMD_MM512_SET1_EPI8(0x02);
        const __m512i K8_03 = SIMD_MM512_SET1_EPI8(0x03);
        const __m512i K8_07 = SIMD_MM512_SET1_EPI8(0x07);
        const __m512i K8_0F = SIMD_MM512_SET1_EPI8(0x0F);
        const __m512i K8_80 = SIMD_MM512_SET1_EPI8(0x80);

        const __m512i K8_01_FF = SIMD_MM512_SET2_EPI8(0x01, 0xFF);

        const __m512i K16_0001 = SIMD_MM512_SET1_EPI16(0x0001);
        const __m512i K16_0002 = SIMD_MM512_SET1_EPI16(0x0002);
        const __m512i K16_0003 = SIMD_MM512_SET1_EPI16(0x0003);
        const __m512i K16_0004 = SIMD_MM512_SET1_EPI16(0x0004);
        const __m512i K16_0005 = SIMD_MM512_SET1_EPI16(0x0005);
        const __m512i K16_0006 = SIMD_MM512_SET1_EPI16(0x0006);
        const __m512i K16_0008 = SIMD_MM512_SET1_EPI16(0x0008);
        const __m512i K16_0010 = SIMD_MM512_SET1_EPI16(0x0010);
        const __m512i K16_0020 = SIMD_MM512_SET1_EPI16(0x0020);
        const __m512i K16_0038 = SIMD_MM512_SET1_EPI16(0x0038);
        const __m512i K16_0080 = SIMD_MM512_SET1_EPI16(0x0080);
        const __m512i K16_00FF = SIMD_MM512_SET1_EPI16(0x00FF);
        const __m512i K16_0101 = SIMD_MM512_SET1_EPI16(0x0101);
        const __m512i K16_FF00 = SIMD_MM512_SET1_EPI16(0xFF00);

        const __m512i K32_00000001 = SIMD_MM512_SET1_EPI32(0x00000001);
        const __m512i K32_000000FF = SIMD_MM512_SET1_EPI32(0x000000FF);
        const __m512i K32_0000FFFF = SIMD_MM512_SET1_EPI32(0x0000FFFF);
        const __m512i K32_00010000 = SIMD_MM512_SET1_EPI32(0x00010000);
        const __m512i K32_00FF0000 = SIMD_MM512_SET1_EPI32(0x00FF0000);
        const __m512i K32_FF000000 = SIMD_MM512_SET1_EPI32(0xFF000000);
        const __m512i K32_FFFFFF00 = SIMD_MM512_SET1_EPI32(0xFFFFFF00);

        const __m512i K16_Y_ADJUST = SIMD_MM512_SET1_EPI16(Base::Y_ADJUST);
        const __m512i K16_UV_ADJUST = SIMD_MM512_SET1_EPI16(Base::UV_ADJUST);

        const __m512i K16_YRGB_RT = SIMD_MM512_SET2_EPI16(Base::Y_TO_RGB_WEIGHT, Base::YUV_TO_BGR_ROUND_TERM);
        const __m512i K16_VR_0 = SIMD_MM512_SET2_EPI16(Base::V_TO_RED_WEIGHT, 0);
        const __m512i K16_UG_VG = SIMD_MM512_SET2_EPI16(Base::U_TO_GREEN_WEIGHT, Base::V_TO_GREEN_WEIGHT);
        const __m512i K16_UB_0 = SIMD_MM512_SET2_EPI16(Base::U_TO_BLUE_WEIGHT, 0);

        const __m512i K16_BY_RY = SIMD_MM512_SET2_EPI16(Base::BLUE_TO_Y_WEIGHT, Base::RED_TO_Y_WEIGHT);
        const __m512i K16_GY_RT = SIMD_MM512_SET2_EPI16(Base::GREEN_TO_Y_WEIGHT, Base::BGR_TO_YUV_ROUND_TERM);
        const __m512i K16_BU_RU = SIMD_MM512_SET2_EPI16(Base::BLUE_TO_U_WEIGHT, Base::RED_TO_U_WEIGHT);
        const __m512i K16_GU_RT = SIMD_MM512_SET2_EPI16(Base::GREEN_TO_U_WEIGHT, Base::BGR_TO_YUV_ROUND_TERM);
        const __m512i K16_BV_RV = SIMD_MM512_SET2_EPI16(Base::BLUE_TO_V_WEIGHT, Base::RED_TO_V_WEIGHT);
        const __m512i K16_GV_RT = SIMD_MM512_SET2_EPI16(Base::GREEN_TO_V_WEIGHT, Base::BGR_TO_YUV_ROUND_TERM);

        const __m512i K16_DIVISION_BY_9_FACTOR = SIMD_MM512_SET1_EPI16(Base::DIVISION_BY_9_FACTOR);

        const __m512i K64_00000000FFFFFFFF = SIMD_MM512_SET2_EPI32(0xFFFFFFFF, 0);

        const __m512i K8_SUFFLE_BGRA_TO_G0A0 = SIMD_MM512_SETR_EPI8(
            0x1, -1, 0x3, -1, 0x5, -1, 0x7, -1, 0x9, -1, 0xB, -1, 0xD, -1, 0xF, -1,
            0x1, -1, 0x3, -1, 0x5, -1, 0x7, -1, 0x9, -1, 0xB, -1, 0xD, -1, 0xF, -1,
            0x1, -1, 0x3, -1, 0x5, -1, 0x7, -1, 0x9, -1, 0xB, -1, 0xD, -1, 0xF, -1,
            0x1, -1, 0x3, -1, 0x5, -1, 0x7, -1, 0x9, -1, 0xB, -1, 0xD, -1, 0xF, -1);

        const __m512i K8_SUFFLE_BGRA_TO_G000 = SIMD_MM512_SETR_EPI8(
            0x1, -1, -1, -1, 0x5, -1, -1, -1, 0x9, -1, -1, -1, 0xD, -1, -1, -1,
            0x1, -1, -1, -1, 0x5, -1, -1, -1, 0x9, -1, -1, -1, 0xD, -1, -1, -1,
            0x1, -1, -1, -1, 0x5, -1, -1, -1, 0x9, -1, -1, -1, 0xD, -1, -1, -1,
            0x1, -1, -1, -1, 0x5, -1, -1, -1, 0x9, -1, -1, -1, 0xD, -1, -1, -1);

        const __m512i K8_SUFFLE_BGRA_TO_A000 = SIMD_MM512_SETR_EPI8(
            0x3, -1, -1, -1, 0x7, -1, -1, -1, 0xB, -1, -1, -1, 0xF, -1, -1, -1,
            0x3, -1, -1, -1, 0x7, -1, -1, -1, 0xB, -1, -1, -1, 0xF, -1, -1, -1,
            0x3, -1, -1, -1, 0x7, -1, -1, -1, 0xB, -1, -1, -1, 0xF, -1, -1, -1,
            0x3, -1, -1, -1, 0x7, -1, -1, -1, 0xB, -1, -1, -1, 0xF, -1, -1, -1);

        const __m512i K8_SUFFLE_BGR_TO_B0R0 = SIMD_MM512_SETR_EPI8(
            0x0, -1, 0x2, -1, 0x3, -1, 0x5, -1, 0x6, -1, 0x8, -1, 0x9, -1, 0xB, -1,
            0x0, -1, 0x2, -1, 0x3, -1, 0x5, -1, 0x6, -1, 0x8, -1, 0x9, -1, 0xB, -1,
            0x0, -1, 0x2, -1, 0x3, -1, 0x5, -1, 0x6, -1, 0x8, -1, 0x9, -1, 0xB, -1,
            0x0, -1, 0x2, -1, 0x3, -1, 0x5, -1, 0x6, -1, 0x8, -1, 0x9, -1, 0xB, -1);

        const __m512i K8_SUFFLE_BGR_TO_G000 = SIMD_MM512_SETR_EPI8(
            0x1, -1, -1, -1, 0x4, -1, -1, -1, 0x7, -1, -1, -1, 0xA, -1, -1, -1,
            0x1, -1, -1, -1, 0x4, -1, -1, -1, 0x7, -1, -1, -1, 0xA, -1, -1, -1,
            0x1, -1, -1, -1, 0x4, -1, -1, -1, 0x7, -1, -1, -1, 0xA, -1, -1, -1,
            0x1, -1, -1, -1, 0x4, -1, -1, -1, 0x7, -1, -1, -1, 0xA, -1, -1, -1);

        const __m512i K8_SUFFLE_BGR_TO_G010 = SIMD_MM512_SETR_EPI8(
            0x1, -1, 0xC, -1, 0x4, -1, 0xD, -1, 0x7, -1, 0xE, -1, 0xA, -1, 0xF, -1,
            0x1, -1, 0xC, -1, 0x4, -1, 0xD, -1, 0x7, -1, 0xE, -1, 0xA, -1, 0xF, -1,
            0x1, -1, 0xC, -1, 0x4, -1, 0xD, -1, 0x7, -1, 0xE, -1, 0xA, -1, 0xF, -1,
            0x1, -1, 0xC, -1, 0x4, -1, 0xD, -1, 0x7, -1, 0xE, -1, 0xA, -1, 0xF, -1);

        const __m512i K8_SHUFFLE_GRAY_TO_BGR0 = SIMD_MM512_SETR_EPI8(
            0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x3, 0x3, 0x3, 0x4, 0x4, 0x4, 0x5,
            0x5, 0x5, 0x6, 0x6, 0x6, 0x7, 0x7, 0x7, 0x8, 0x8, 0x8, 0x9, 0x9, 0x9, 0xA, 0xA,
            0xA, 0xB, 0xB, 0xB, 0xC, 0xC, 0xC, 0xD, 0xD, 0xD, 0xE, 0xE, 0xE, 0xF, 0xF, 0xF,
            0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x3, 0x3, 0x3, 0x4, 0x4, 0x4, 0x5);
        const __m512i K8_SHUFFLE_GRAY_TO_BGR1 = SIMD_MM512_SETR_EPI8(
            0x5, 0x5, 0x6, 0x6, 0x6, 0x7, 0x7, 0x7, 0x8, 0x8, 0x8, 0x9, 0x9, 0x9, 0xA, 0xA,
            0xA, 0xB, 0xB, 0xB, 0xC, 0xC, 0xC, 0xD, 0xD, 0xD, 0xE, 0xE, 0xE, 0xF, 0xF, 0xF,
            0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x3, 0x3, 0x3, 0x4, 0x4, 0x4, 0x5,
            0x5, 0x5, 0x6, 0x6, 0x6, 0x7, 0x7, 0x7, 0x8, 0x8, 0x8, 0x9, 0x9, 0x9, 0xA, 0xA);
        const __m512i K8_SHUFFLE_GRAY_TO_BGR2 = SIMD_MM512_SETR_EPI8(
            0xA, 0xB, 0xB, 0xB, 0xC, 0xC, 0xC, 0xD, 0xD, 0xD, 0xE, 0xE, 0xE, 0xF, 0xF, 0xF,
            0x0, 0x0, 0x0, 0x1, 0x1, 0x1, 0x2, 0x2, 0x2, 0x3, 0x3, 0x3, 0x4, 0x4, 0x4, 0x5,
            0x5, 0x5, 0x6, 0x6, 0x6, 0x7, 0x7, 0x7, 0x8, 0x8, 0x8, 0x9, 0x9, 0x9, 0xA, 0xA,
            0xA, 0xB, 0xB, 0xB, 0xC, 0xC, 0xC, 0xD, 0xD, 0xD, 0xE, 0xE, 0xE, 0xF, 0xF, 0xF);

        const __m512i K16_PERMUTE_FOR_HADD_0 = SIMD_MM512_SETR_EPI16(
            0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E,
            0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E, 0x30, 0x32, 0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E);
        const __m512i K16_PERMUTE_FOR_HADD_1 = SIMD_MM512_SETR_EPI16(
            0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F, 0x11, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1F,
            0x21, 0x23, 0x25, 0x27, 0x29, 0x2B, 0x2D, 0x2F, 0x31, 0x33, 0x35, 0x37, 0x39, 0x3B, 0x3D, 0x3F);

        const __m512i K8_SHUFFLE_BLUE_TO_BGR0 = SIMD_MM512_SETR_EPI8(
            0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1, 0x5,
            -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1,
            -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1, -1,
            0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1, 0x5);
        const __m512i K8_SHUFFLE_BLUE_TO_BGR1 = SIMD_MM512_SETR_EPI8(
            -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1,
            -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1, -1,
            0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1, 0x5,
            -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1);
        const __m512i K8_SHUFFLE_BLUE_TO_BGR2 = SIMD_MM512_SETR_EPI8(
            -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1, -1,
            0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1, 0x5,
            -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA, -1,
            -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1, -1);

        const __m512i K8_SHUFFLE_GREEN_TO_BGR0 = SIMD_MM512_SETR_EPI8(
            -1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1,
            0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA,
            -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1,
            -1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1);
        const __m512i K8_SHUFFLE_GREEN_TO_BGR1 = SIMD_MM512_SETR_EPI8(
            0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA,
            -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1,
            -1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1,
            0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA);
        const __m512i K8_SHUFFLE_GREEN_TO_BGR2 = SIMD_MM512_SETR_EPI8(
            -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1,
            -1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1, -1,
            0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1, 0xA,
            -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF, -1);

        const __m512i K8_SHUFFLE_RED_TO_BGR0 = SIMD_MM512_SETR_EPI8(
            -1, -1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1,
            -1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1,
            0xA, -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF,
            -1, -1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1);
        const __m512i K8_SHUFFLE_RED_TO_BGR1 = SIMD_MM512_SETR_EPI8(
            -1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1,
            0xA, -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF,
            -1, -1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1,
            -1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1);
        const __m512i K8_SHUFFLE_RED_TO_BGR2 = SIMD_MM512_SETR_EPI8(
            0xA, -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF,
            -1, -1, 0x0, -1, -1, 0x1, -1, -1, 0x2, -1, -1, 0x3, -1, -1, 0x4, -1,
            -1, 0x5, -1, -1, 0x6, -1, -1, 0x7, -1, -1, 0x8, -1, -1, 0x9, -1, -1,
            0xA, -1, -1, 0xB, -1, -1, 0xC, -1, -1, 0xD, -1, -1, 0xE, -1, -1, 0xF);

        const __m512i K32_PERMUTE_COLOR_TO_BGR0 = SIMD_MM512_SETR_EPI32(0x0, 0x1, 0x2, 0x3, 0x0, 0x1, 0x2, 0x3, 0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7);
        const __m512i K32_PERMUTE_COLOR_TO_BGR1 = SIMD_MM512_SETR_EPI32(0x4, 0x5, 0x6, 0x7, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0x8, 0x9, 0xA, 0xB);
        const __m512i K32_PERMUTE_COLOR_TO_BGR2 = SIMD_MM512_SETR_EPI32(0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF, 0xC, 0xD, 0xE, 0xF, 0xC, 0xD, 0xE, 0xF);

        const __m512i K16_INTERLEAVE_0 = SIMD_MM512_SETR_EPI16(
            0x00, 0x20, 0x01, 0x21, 0x02, 0x22, 0x03, 0x23, 0x04, 0x24, 0x05, 0x25, 0x06, 0x26, 0x07, 0x27,
            0x08, 0x28, 0x09, 0x29, 0x0A, 0x2A, 0x0B, 0x2B, 0x0C, 0x2C, 0x0D, 0x2D, 0x0E, 0x2E, 0x0F, 0x2F);
        const __m512i K16_INTERLEAVE_1 = SIMD_MM512_SETR_EPI16(
            0x10, 0x30, 0x11, 0x31, 0x12, 0x32, 0x13, 0x33, 0x14, 0x34, 0x15, 0x35, 0x16, 0x36, 0x17, 0x37,
            0x18, 0x38, 0x19, 0x39, 0x1A, 0x3A, 0x1B, 0x3B, 0x1C, 0x3C, 0x1D, 0x3D, 0x1E, 0x3E, 0x1F, 0x3F);

        const __m512i K32_INTERLEAVE_0 = SIMD_MM512_SETR_EPI32(0x00, 0x10, 0x01, 0x11, 0x02, 0x12, 0x03, 0x13, 0x04, 0x14, 0x05, 0x15, 0x06, 0x16, 0x07, 0x17);
        const __m512i K32_INTERLEAVE_1 = SIMD_MM512_SETR_EPI32(0x08, 0x18, 0x09, 0x19, 0x0A, 0x1A, 0x0B, 0x1B, 0x0C, 0x1C, 0x0D, 0x1D, 0x0E, 0x1E, 0x0F, 0x1F);

        const __m512i K32_DEINTERLEAVE_0 = SIMD_MM512_SETR_EPI32(0x00, 0x02, 0x04, 0x06, 0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C, 0x1E);
        const __m512i K32_DEINTERLEAVE_1 = SIMD_MM512_SETR_EPI32(0x01, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F, 0x11, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1F);

        const __m512i K32_PERMUTE_FOR_PACK = SIMD_MM512_SETR_EPI32(0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15);
        const __m512i K32_PERMUTE_FOR_UNPACK = SIMD_MM512_SETR_EPI32(0, 1, 8, 9, 2, 3, 10, 11, 4, 5, 12, 13, 6, 7, 14, 15);

        const __m512i K32_PERMUTE_BGR_TO_BGRA = SIMD_MM512_SETR_EPI32(0x00, 0x01, 0x02, 0x10, 0x03, 0x04, 0x05, 0x11, 0x06, 0x07, 0x08, 0x12, 0x09, 0x0A, 0x0B, 0x13);
        const __m512i K32_PERMUTE_BGR_TO_BGRA_0 = SIMD_MM512_SETR_EPI32(0x00, 0x01, 0x02, -1, 0x03, 0x04, 0x05, -1, 0x06, 0x07, 0x08, -1, 0x09, 0x0A, 0x0B, -1);
        const __m512i K32_PERMUTE_BGR_TO_BGRA_1 = SIMD_MM512_SETR_EPI32(0x0C, 0x0D, 0x0E, -1, 0x0F, 0x10, 0x11, -1, 0x12, 0x13, 0x14, -1, 0x15, 0x16, 0x17, -1);
        const __m512i K32_PERMUTE_BGR_TO_BGRA_2 = SIMD_MM512_SETR_EPI32(0x08, 0x09, 0x0A, -1, 0x0B, 0x0C, 0x0D, -1, 0x0E, 0x0F, 0x10, -1, 0x11, 0x12, 0x13, -1);
        const __m512i K32_PERMUTE_BGR_TO_BGRA_3 = SIMD_MM512_SETR_EPI32(0x04, 0x05, 0x06, -1, 0x07, 0x08, 0x09, -1, 0x0A, 0x0B, 0x0C, -1, 0x0D, 0x0E, 0x0F, -1);

        const __m512i K32_PERMUTE_FOR_TWO_UNPACK = SIMD_MM512_SETR_EPI32(0x0, 0x4, 0x8, 0xC, 0x1, 0x5, 0x9, 0xD, 0x2, 0x6, 0xA, 0xE, 0x3, 0x7, 0xB, 0xF);

        const __m512i K64_INTERLEAVE_0 = SIMD_MM512_SETR_EPI64(0x0, 0x8, 0x01, 0x9, 0x2, 0xa, 0x3, 0xb);
        const __m512i K64_INTERLEAVE_1 = SIMD_MM512_SETR_EPI64(0x4, 0xc, 0x05, 0xd, 0x6, 0xe, 0x7, 0xf);

        const __m512i K64_PERMUTE_FOR_PACK = SIMD_MM512_SETR_EPI64(0, 2, 4, 6, 1, 3, 5, 7);
        const __m512i K64_PERMUTE_FOR_UNPACK = SIMD_MM512_SETR_EPI64(0, 4, 1, 5, 2, 6, 3, 7);
    }
#endif

#ifdef SIMD_AVX512VNNI_ENABLE    
    namespace Avx512vnni
    {
        using namespace Avx512bw;
    }
#endif

#if defined(SIMD_AMXBF16_ENABLE) || (defined(SIMD_AVX512BW_ENABLE) && defined(SIMD_AMX_EMULATE))  
    namespace AmxBf16
    {
        using namespace Avx512bw;
    }
#endif

#ifdef SIMD_NEON_ENABLE    
    namespace Neon
    {
        const size_t A = sizeof(uint8x16_t);
        const size_t DA = 2 * A;
        const size_t QA = 4 * A;
        const size_t OA = 8 * A;
        const size_t HA = A / 2;

        const size_t F = sizeof(float32x4_t) / sizeof(float);
        const size_t DF = 2 * F;
        const size_t QF = 4 * F;
        const size_t HF = F / 2;

        const uint8x16_t K8_00 = SIMD_VEC_SET1_EPI8(0x00);
        const uint8x16_t K8_01 = SIMD_VEC_SET1_EPI8(0x01);
        const uint8x16_t K8_02 = SIMD_VEC_SET1_EPI8(0x02);
        const uint8x16_t K8_03 = SIMD_VEC_SET1_EPI8(0x03);
        const uint8x16_t K8_04 = SIMD_VEC_SET1_EPI8(0x04);
        const uint8x16_t K8_07 = SIMD_VEC_SET1_EPI8(0x07);
        const uint8x16_t K8_08 = SIMD_VEC_SET1_EPI8(0x08);
        const uint8x16_t K8_10 = SIMD_VEC_SET1_EPI8(0x10);
        const uint8x16_t K8_20 = SIMD_VEC_SET1_EPI8(0x20);
        const uint8x16_t K8_40 = SIMD_VEC_SET1_EPI8(0x40);
        const uint8x16_t K8_80 = SIMD_VEC_SET1_EPI8(0x80);
        const uint8x16_t K8_FF = SIMD_VEC_SET1_EPI8(0xFF);

        const uint16x8_t K16_0000 = SIMD_VEC_SET1_EPI16(0x0000);
        const uint16x8_t K16_0001 = SIMD_VEC_SET1_EPI16(0x0001);
        const uint16x8_t K16_0002 = SIMD_VEC_SET1_EPI16(0x0002);
        const uint16x8_t K16_0003 = SIMD_VEC_SET1_EPI16(0x0003);
        const uint16x8_t K16_0004 = SIMD_VEC_SET1_EPI16(0x0004);
        const uint16x8_t K16_0005 = SIMD_VEC_SET1_EPI16(0x0005);
        const uint16x8_t K16_0006 = SIMD_VEC_SET1_EPI16(0x0006);
        const uint16x8_t K16_0008 = SIMD_VEC_SET1_EPI16(0x0008);
        const uint16x8_t K16_0010 = SIMD_VEC_SET1_EPI16(0x0010);
        const uint16x8_t K16_0020 = SIMD_VEC_SET1_EPI16(0x0020);
        const uint16x8_t K16_0080 = SIMD_VEC_SET1_EPI16(0x0080);
        const uint16x8_t K16_00FF = SIMD_VEC_SET1_EPI16(0x00FF);
        const uint16x8_t K16_0101 = SIMD_VEC_SET1_EPI16(0x0101);
        const uint16x8_t K16_0800 = SIMD_VEC_SET1_EPI16(0x0800);
        const uint16x8_t K16_FF00 = SIMD_VEC_SET1_EPI16(0xFF00);

        const uint32x4_t K32_00000000 = SIMD_VEC_SET1_EPI32(0x00000000);
        const uint32x4_t K32_00000001 = SIMD_VEC_SET1_EPI32(0x00000001);
        const uint32x4_t K32_00000002 = SIMD_VEC_SET1_EPI32(0x00000002);
        const uint32x4_t K32_00000003 = SIMD_VEC_SET1_EPI32(0x00000003);
        const uint32x4_t K32_00000004 = SIMD_VEC_SET1_EPI32(0x00000004);
        const uint32x4_t K32_00000005 = SIMD_VEC_SET1_EPI32(0x00000005);
        const uint32x4_t K32_00000008 = SIMD_VEC_SET1_EPI32(0x00000008);
        const uint32x4_t K32_00000010 = SIMD_VEC_SET1_EPI32(0x00000010);
        const uint32x4_t K32_000000FF = SIMD_VEC_SET1_EPI32(0x000000FF);
        const uint32x4_t K32_0000FFFF = SIMD_VEC_SET1_EPI32(0x0000FFFF);
        const uint32x4_t K32_00010000 = SIMD_VEC_SET1_EPI32(0x00010000);
        const uint32x4_t K32_00FF0000 = SIMD_VEC_SET1_EPI32(0x00FF0000);
        const uint32x4_t K32_01000000 = SIMD_VEC_SET1_EPI32(0x01000000);
        const uint32x4_t K32_08080800 = SIMD_VEC_SET1_EPI32(0x08080800);
        const uint32x4_t K32_FF000000 = SIMD_VEC_SET1_EPI32(0xFF000000);
        const uint32x4_t K32_FFFFFF00 = SIMD_VEC_SET1_EPI32(0xFFFFFF00);
        const uint32x4_t K32_FFFFFFFF = SIMD_VEC_SET1_EPI32(0xFFFFFFFF);
        const uint32x4_t K32_0123 = SIMD_VEC_SETR_EPI32(0, 1, 2, 3);

        const uint64x2_t K64_0000000000000000 = SIMD_VEC_SET1_EPI64(0x0000000000000000);

        const uint16x4_t K16_BLUE_TO_GRAY_WEIGHT = SIMD_VEC_SET1_PI16(Base::BLUE_TO_GRAY_WEIGHT);
        const uint16x4_t K16_GREEN_TO_GRAY_WEIGHT = SIMD_VEC_SET1_PI16(Base::GREEN_TO_GRAY_WEIGHT);
        const uint16x4_t K16_RED_TO_GRAY_WEIGHT = SIMD_VEC_SET1_PI16(Base::RED_TO_GRAY_WEIGHT);
        const uint32x4_t K32_BGR_TO_GRAY_ROUND_TERM = SIMD_VEC_SET1_EPI32(Base::BGR_TO_GRAY_ROUND_TERM);

        const int16x8_t K16_Y_ADJUST = SIMD_VEC_SET1_EPI16(Base::Y_ADJUST);
        const int16x8_t K16_UV_ADJUST = SIMD_VEC_SET1_EPI16(Base::UV_ADJUST);

        const int16x4_t K16_BLUE_TO_Y_WEIGHT = SIMD_VEC_SET1_PI16(Base::BLUE_TO_Y_WEIGHT);
        const int16x4_t K16_GREEN_TO_Y_WEIGHT = SIMD_VEC_SET1_PI16(Base::GREEN_TO_Y_WEIGHT);
        const int16x4_t K16_RED_TO_Y_WEIGHT = SIMD_VEC_SET1_PI16(Base::RED_TO_Y_WEIGHT);

        const int16x4_t K16_BLUE_TO_U_WEIGHT = SIMD_VEC_SET1_PI16(Base::BLUE_TO_U_WEIGHT);
        const int16x4_t K16_GREEN_TO_U_WEIGHT = SIMD_VEC_SET1_PI16(Base::GREEN_TO_U_WEIGHT);
        const int16x4_t K16_RED_TO_U_WEIGHT = SIMD_VEC_SET1_PI16(Base::RED_TO_U_WEIGHT);

        const int16x4_t K16_BLUE_TO_V_WEIGHT = SIMD_VEC_SET1_PI16(Base::BLUE_TO_V_WEIGHT);
        const int16x4_t K16_GREEN_TO_V_WEIGHT = SIMD_VEC_SET1_PI16(Base::GREEN_TO_V_WEIGHT);
        const int16x4_t K16_RED_TO_V_WEIGHT = SIMD_VEC_SET1_PI16(Base::RED_TO_V_WEIGHT);

        const int32x4_t K32_BGR_TO_YUV_ROUND_TERM = SIMD_VEC_SET1_EPI32(Base::BGR_TO_YUV_ROUND_TERM);

        const int16x4_t K16_Y_TO_RGB_WEIGHT = SIMD_VEC_SET1_PI16(Base::Y_TO_RGB_WEIGHT);

        const int16x4_t K16_U_TO_BLUE_WEIGHT = SIMD_VEC_SET1_PI16(Base::U_TO_BLUE_WEIGHT);
        const int16x4_t K16_U_TO_GREEN_WEIGHT = SIMD_VEC_SET1_PI16(Base::U_TO_GREEN_WEIGHT);

        const int16x4_t K16_V_TO_GREEN_WEIGHT = SIMD_VEC_SET1_PI16(Base::V_TO_GREEN_WEIGHT);
        const int16x4_t K16_V_TO_RED_WEIGHT = SIMD_VEC_SET1_PI16(Base::V_TO_RED_WEIGHT);

        const int32x4_t K32_YUV_TO_BGR_ROUND_TERM = SIMD_VEC_SET1_EPI32(Base::YUV_TO_BGR_ROUND_TERM);
    }
#endif

#ifdef SIMD_HVX_ENABLE
    namespace Hvx
    {
        const size_t A = 128;
        const size_t DA = 2 * A;
        const size_t QA = 4 * A;
        const size_t OA = 8 * A;
        const size_t HA = A / 2;

        const size_t F = A / sizeof(float);
        const size_t DF = 2 * F;
        const size_t QF = 4 * F;
        const size_t HF = F / 2;
    }
#endif
}
#endif
