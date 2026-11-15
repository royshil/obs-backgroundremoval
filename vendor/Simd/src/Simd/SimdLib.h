/*
* Simd Library (http://ermig1979.github.io/Simd).
*
* Copyright (c) 2011-2026 Yermalayeu Ihar,
*               2014-2019 Antonenka Mikhail,
*               2019-2019 Facundo Galan,
*               2024-2024 Sergey Chezhin,
*               2025-2025 Ger Hobbelt,
*               2026-2026 TianWei Lin,
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

#ifndef __SimdLib_h__
#define __SimdLib_h__

#include <stddef.h>

#if defined(_MSC_VER) || defined(__CODEGEARC__)

#define SIMD_INLINE __forceinline

#elif defined(__GNUC__)

#define SIMD_INLINE inline __attribute__ ((always_inline))

#else

#error This platform is unsupported!

#endif

#if defined(__GNUC__) || (defined(_MSC_VER) && (_MSC_VER >= 1600)) || (defined(__CODEGEARC__) && (__CODEGEARC__ >= 1840))
#include <stdint.h>
#else
#  if (_MSC_VER < 1300)
typedef signed char       int8_t;
typedef signed short      int16_t;
typedef signed int        int32_t;
typedef unsigned char     uint8_t;
typedef unsigned short    uint16_t;
typedef unsigned int      uint32_t;
#  else
typedef signed __int8     int8_t;
typedef signed __int16    int16_t;
typedef signed __int32    int32_t;
typedef unsigned __int8   uint8_t;
typedef unsigned __int16  uint16_t;
typedef unsigned __int32  uint32_t;
#  endif
typedef signed __int64    int64_t;
typedef unsigned __int64  uint64_t;
#endif

#if defined(_MSVC_LANG) 
#if _MSVC_LANG >= 201103L
#define SIMD_CPP_2011_ENABLE
#endif

#if _MSVC_LANG >= 201402L
#define SIMD_CPP_2014_ENABLE
#endif

#if _MSVC_LANG >= 201703L
#define SIMD_CPP_2017_ENABLE
#endif

#if _MSVC_LANG >= 202002L
#define SIMD_CPP_2020_ENABLE
#endif

#if _MSVC_LANG >= 202302L
#define SIMD_CPP_2023_ENABLE
#endif
#else
#if __cplusplus >= 201103L
#define SIMD_CPP_2011_ENABLE
#endif

#if __cplusplus >= 201402L
#define SIMD_CPP_2014_ENABLE
#endif

#if __cplusplus >= 201703L
#define SIMD_CPP_2017_ENABLE
#endif

#if __cplusplus >= 202002L
#define SIMD_CPP_2020_ENABLE
#endif

#if __cplusplus >= 202302L
#define SIMD_CPP_2023_ENABLE
#endif
#endif

#if defined(SIMD_CPP_2020_ENABLE)
#define SIMD_CONSTEXPR constexpr
#else
#define SIMD_CONSTEXPR
#endif

#if defined(SIMD_CPP_2014_ENABLE)
#define SIMD_DEPRECATED [[deprecated]]
#define SIMD_DEPRECATED_EX(message) [[deprecated(message)]]
#else
#define SIMD_DEPRECATED
#define SIMD_DEPRECATED_EX(message)
#endif

/*! @ingroup c_types
    Describes the order of color samples in a repeating 2x2 Bayer cell.

    The letters in each name are listed in raster order: top-left, top-right,
    bottom-left, bottom-right. These layouts correspond to the Bayer pixel formats
    used by ::SimdBayerToBgr, ::SimdBayerToBgra, ::SimdBgraToBayer and ::SimdBgrToBayer.
*/
typedef enum
{
    /*! A Bayer layout with the first row G R and the second row B G. */
    SimdBayerLayoutGrbg,
    /*! A Bayer layout with the first row G B and the second row R G. */
    SimdBayerLayoutGbrg,
    /*! A Bayer layout with the first row R G and the second row G B. */
    SimdBayerLayoutRggb,
    /*! A Bayer layout with the first row B G and the second row G R. */
    SimdBayerLayoutBggr,
} SimdBayerLayoutType;

/*! @ingroup c_types
    Describes a boolean value used by the C API.

    Function parameters of this type enable or disable an option, and functions
    returning this type report success/failure or another binary state.
*/
typedef enum
{
    SimdFalse = 0, /*!< False, disabled or failed state. */
    SimdTrue = 1, /*!< True, enabled or successful state. */
} SimdBool;

/*! @ingroup c_types
    Describes comparison predicates used by binarization and conditional operations.

    In expressions such as <tt>Compare(a, b, compareType)</tt>, \a a is typically
    a source, mask or image value, and \a b is the threshold or reference
    value passed to the function.
*/
typedef enum
{
    /*! The predicate is true when a == b. */
    SimdCompareEqual,
    /*! The predicate is true when a != b. */
    SimdCompareNotEqual,
    /*! The predicate is true when a > b. */
    SimdCompareGreater,
    /*! The predicate is true when a >= b. */
    SimdCompareGreaterOrEqual,
    /*! The predicate is true when a < b. */
    SimdCompareLesser,
    /*! The predicate is true when a <= b. */
    SimdCompareLesserOrEqual,
} SimdCompareType;

/*! @ingroup synet_types
    Describes activation functions used by Synet convolution, deconvolution, inner product, merged convolution,
    quantized convolution and quantized add APIs.

    Activations are applied after bias/normalization/addition as documented by the corresponding function.
    Parameters are passed through the \c params or \c actParams arguments of those functions. A NULL parameter
    pointer is valid only for activations that do not use parameters.
*/
typedef enum
{
    /*!
        Identity activation: no transformation is applied.
        \verbatim
        dst[i] = src[i];
        \endverbatim
    */
    SimdConvolutionActivationIdentity = 0,
    /*!
        ReLU activation function. It does not use parameters.
        \verbatim
        dst[i] = Max(0, src[i]);
        \endverbatim
    */
    SimdConvolutionActivationRelu,
    /*!
        Leaky ReLU activation function.
        It has one parameter: negative slope (params[0]).
        \verbatim
        dst[i] = src[i] > 0 ? src[i] : slope*src[i];
        \endverbatim
    */
    SimdConvolutionActivationLeakyRelu,
    /*!
        RestrictRange activation function.
        It clamps the value to the interval [lower, upper], where lower = params[0] and upper = params[1].
        \verbatim
        dst[i] = Min(Max(lower, src[i]), upper);
        \endverbatim
    */
    SimdConvolutionActivationRestrictRange,
    /*!
        PReLU activation function.
        It uses one negative slope per destination channel. For a convolution output with dstC channels and
        spatial size n = dstH*dstW, params[c] is used for channel c.
        \verbatim
        dst[c*n + j] = src[c*n + j] > 0 ? src[c*n + j] : params[c]*src[c*n + j];
        \endverbatim
    */
    SimdConvolutionActivationPrelu,
    /*!
        ELU activation function.
        It has one parameter: alpha (params[0]).
        \verbatim
        dst[i] = src[i] >= 0 ? src[i] : alpha*(Exp(src[i]) - 1);
        \endverbatim
    */
    SimdConvolutionActivationElu,
    /*!
        H-Swish (https://arxiv.org/pdf/1905.02244.pdf) activation function.
        It has two parameters: shift (params[0]) and scale (params[1]). Typical values are shift = 3 and scale = 1/6.
        \verbatim
        dst[i] = Max(Min(src[i], shift) + shift, 0)*scale*src[i];
        \endverbatim
    */
    SimdConvolutionActivationHswish,
    /*!
        Mish (https://arxiv.org/abs/1908.08681) activation function.
        It has one parameter: threshold (params[0]). Values greater than threshold are returned unchanged.
        \verbatim
        dst[i] = src[i] > threshold ? src[i] : src[i]*(1 - 2/(Square(Exp(src[i]) + 1) + 1));
        \endverbatim
    */
    SimdConvolutionActivationMish,
    /*!
        HardSigmoid (https://pytorch.org/docs/stable/generated/torch.nn.Hardsigmoid.html) activation function.
        It has two parameters: scale (params[0]) and shift (params[1]).
        \verbatim
        dst[i] = Max(0, Min(src[i] * scale + shift, 1));
        \endverbatim
    */
    SimdConvolutionActivationHardSigmoid,
    /*!
        Swish (https://en.wikipedia.org/wiki/Swish_function) activation function.
        It has one parameter: slope (params[0]).
        \verbatim
        dst[i] = src[i]/(1 + Exp(-slope*src[i]));
        \endverbatim
    */
    SimdConvolutionActivationSwish,
    /*!
        GELU (https://en.wikipedia.org/wiki/Activation_function) activation function.
        It does not use parameters.
        \verbatim
        dst[i] = src[i] * (1 + erf(src[i]/sqrt(2))) / 2;
        \endverbatim
    */
    SimdConvolutionActivationGelu,
} SimdConvolutionActivationType;

/*! @ingroup c_types
    Describes string descriptions available from ::SimdCpuDesc.

    Unknown values passed to ::SimdCpuDesc return NULL. Returned strings are owned by the library and remain valid
    until the process exits.
*/
typedef enum
{
    SimdCpuDescModel, /*!< CPU brand/model name string. It can be empty when the platform does not expose it. */
} SimdCpuDescType;

/*! @ingroup c_types
    Describes CPU information queries supported by ::SimdCpuInfo.

    Topology and memory queries return counts or byte sizes. SIMD extension queries return 1 only when the
    extension is both supported by the current CPU and enabled in this build of the library; otherwise they return 0.
    Unsupported query values also return 0.
*/
typedef enum
{
    SimdCpuInfoSockets, /*!< Number of CPU sockets. */
    SimdCpuInfoCores, /*!< Number of physical CPU cores. */
    SimdCpuInfoThreads, /*!< Number of logical CPU threads. */
    SimdCpuInfoCacheL1, /*!< Size in bytes of the level 1 data cache. */
    SimdCpuInfoCacheL2, /*!< Size in bytes of the level 2 cache. */
    SimdCpuInfoCacheL3, /*!< Size in bytes of the level 3 cache. */
    SimdCpuInfoRam, /*!< Size in bytes of physical RAM. */
    SimdCpuInfoSse41, /*!< Availability of x86 SSE4.1 code path and required lower SSE levels. */
    SimdCpuInfoAvx2, /*!< Availability of x86 AVX2 code path with AVX and FMA support. */
    SimdCpuInfoAvx512bw, /*!< Availability of x86 AVX-512BW code path with AVX-512F support. */
    SimdCpuInfoAvx512vnni, /*!< Availability of x86 AVX-512VNNI code path. */
    SimdCpuInfoAmxBf16, /*!< Availability of x86 AMX-BF16 code path with AMX-INT8, AVX-512VBMI and AVX-512FP16 support. */
    SimdCpuInfoNeon, /*!< Availability of ARM NEON code path. */
    SimdCpuInfoSve, /*!< Availability of ARM SVE code path. */
    SimdCpuInfoSveSize, /*!< Size in bytes of the ARM SVE/SVE2 vector register; 0 if SVE is unavailable. */
    SimdCpuInfoSve2, /*!< Availability of ARM SVE2 code path. */
    SimdCpuInfoHvx, /*!< Availability of Hexagon HVX code path. */
    SimdCpuInfoCurrentFrequency, /*!< Current frequency in Hz of the CPU core executing the query; 0 if unavailable. */
} SimdCpuInfoType;

/*! @ingroup c_types
    Describes classifier cascade type and capability flags returned by ::SimdDetectionInfo.

    The low bits selected by ::SimdDetectionInfoFeatureMask encode the cascade feature type. Other bits describe
    optional cascade properties. Test flags with bit operations, for example:
    <tt>(flags & SimdDetectionInfoFeatureMask)</tt> and <tt>(flags & SimdDetectionInfoHasTilted)</tt>.
    \note This type is used for implementation of Simd::Detection.
*/
typedef enum
{
    /*! HAAR cascade classifier type, stored in the feature-type bits. */
    SimdDetectionInfoFeatureHaar = 0,
    /*! LBP cascade classifier type, stored in the feature-type bits. */
    SimdDetectionInfoFeatureLbp,
    /*! Mask used to extract the feature type from a ::SimdDetectionInfoFlags value. */
    SimdDetectionInfoFeatureMask = 3,
    /*! Flag set when a HAAR cascade contains tilted features and requires a tilted integral image. */
    SimdDetectionInfoHasTilted = 4,
    /*! Flag set when an LBP cascade can use the 16-bit integer detection path. */
    SimdDetectionInfoCanInt16 = 8,
} SimdDetectionInfoFlags;

/*! @ingroup synet_grid_sample
    Describes interpolation modes used by ::SimdSynetGridSample2dInit.

    The grid tensor stores normalized <tt>(x, y)</tt> coordinates. During ::SimdSynetGridSample2dForward these
    modes define how source pixels around each denormalized coordinate are combined.
*/
typedef enum
{
    /*! Bilinear interpolation from the four neighboring source pixels. */
    SimdGridSampleInterpBilinear = 0,
    /*! Nearest-neighbor interpolation after rounding the denormalized coordinate. */
    SimdGridSampleInterpNearest,
    /*! Bicubic interpolation from a 4x4 neighborhood. */
    SimdGridSampleInterpBicubic,
} SimdGridSampleInterpType;

/*! @ingroup synet_grid_sample
    Describes padding modes used by ::SimdSynetGridSample2dInit.

    Padding is applied when a denormalized grid coordinate falls outside the source image. The border used for
    this test depends on the \c align parameter of ::SimdSynetGridSample2dInit.
*/
typedef enum
{
    /*! Use zero for out-of-bound source samples. */
    SimdGridSamplePaddingZeros = 0,
    /*! Clamp out-of-bound source samples to the nearest border pixel. */
    SimdGridSamplePaddingBorder,
    /*! Reflect out-of-bound source samples across the border before reading the source pixel. */
    SimdGridSamplePaddingReflect,
} SimdGridSamplePaddingType;

/*! @ingroup c_types
    Describes image file formats supported by image I/O functions.

    The type is passed to ::SimdImageSaveToMemory and ::SimdImageSaveToFile to select an encoder.
    Loaders detect the same formats from file content. When ::SimdImageFileUndefined is used for
    saving to a file, the encoder is chosen from the output path extension. When it is used for
    saving to memory, Gray8 input is saved as binary PGM and other supported formats as binary PPM.
*/
typedef enum
{
    /*! Undefined file format; use automatic format choice where the saving function supports it. */
    SimdImageFileUndefined = 0,
    /*! PGM (Portable Graymap) text file format with P2 header. */
    SimdImageFilePgmTxt,
    /*! PGM (Portable Graymap) binary file format with P5 header. */
    SimdImageFilePgmBin,
    /*! PPM (Portable Pixmap) text file format with P3 header. */
    SimdImageFilePpmTxt,
    /*! PPM (Portable Pixmap) binary file format with P6 header. */
    SimdImageFilePpmBin,
    /*! PNG (Portable Network Graphics) image file format. */
    SimdImageFilePng,
    /*! JPEG (Joint Photographic Experts Group) image file format. */
    SimdImageFileJpeg,
    /*! BMP (BitMap Picture) image file format. */
    SimdImageFileBmp,
} SimdImageFileType;

/*! @ingroup c_types
    Describes element-wise binary operations performed by ::SimdOperationBinary8u.

    The function treats every row as <tt>width*channelCount</tt> unsigned 8-bit values. Source and
    destination images must have the same width, height and number of channels; every channel is
    processed independently.
*/
typedef enum
{
    /*! Rounded average of two unsigned values: <tt>dst = (a + b + 1)/2</tt>. */
    SimdOperationBinary8uAverage,
    /*! Bitwise AND: <tt>dst = a & b</tt>. */
    SimdOperationBinary8uAnd,
    /*! Bitwise OR: <tt>dst = a | b</tt>. */
    SimdOperationBinary8uOr,
    /*! Maximum of two unsigned values: <tt>dst = Max(a, b)</tt>. */
    SimdOperationBinary8uMaximum,
    /*! Minimum of two unsigned values: <tt>dst = Min(a, b)</tt>. */
    SimdOperationBinary8uMinimum,
    /*! Saturated subtraction: <tt>dst = Max(a - b, 0)</tt>. */
    SimdOperationBinary8uSaturatedSubtraction,
    /*! Saturated addition: <tt>dst = Min(a + b, 255)</tt>. */
    SimdOperationBinary8uSaturatedAddition,
} SimdOperationBinary8uType;

/*! @ingroup c_types
    Describes element-wise binary operations performed by ::SimdOperationBinary16i.

    The function treats every row as <tt>width</tt> signed 16-bit values. Source and destination
    images must have the same width, height and ::SimdPixelFormatInt16 format. Operations are not
    saturated.
*/
typedef enum
{
    /*! Signed 16-bit addition: <tt>dst = a + b</tt>. */
    SimdOperationBinary16iAddition,
    /*! Signed 16-bit subtraction: <tt>dst = a - b</tt>. */
    SimdOperationBinary16iSubtraction,
} SimdOperationBinary16iType;

/*! @ingroup c_types
    Describes in-memory pixel formats of images.

    This type selects channel order, channel count and element size in image conversion, image I/O,
    integral and arithmetic functions. In particular it is used by ::SimdBayerToBgr,
    ::SimdBayerToBgra, ::SimdBgraToBayer and ::SimdBgrToBayer.

    \note This type corresponds to C++ type Simd::View::Format.
*/
typedef enum
{
    /*! Undefined or not initialized pixel format. */
    SimdPixelFormatNone = 0,
    /*! Single-channel 8-bit gray image. */
    SimdPixelFormatGray8,
    /*! Two 8-bit channels stored as interleaved UV pairs; used for NV12/NV21 chroma planes. */
    SimdPixelFormatUv16,
    /*! Three 8-bit channels in BGR order: blue, green, red. */
    SimdPixelFormatBgr24,
    /*! Four 8-bit channels in BGRA order: blue, green, red, alpha. */
    SimdPixelFormatBgra32,
    /*! Single-channel signed 16-bit integer image. */
    SimdPixelFormatInt16,
    /*! Single-channel signed 32-bit integer image. */
    SimdPixelFormatInt32,
    /*! Single-channel signed 64-bit integer image. */
    SimdPixelFormatInt64,
    /*! Single-channel 32-bit floating point image. */
    SimdPixelFormatFloat,
    /*! Single-channel 64-bit floating point image. */
    SimdPixelFormatDouble,
    /*! Single-channel 8-bit Bayer mosaic with 2x2 tile: G R / B G. */
    SimdPixelFormatBayerGrbg,
    /*! Single-channel 8-bit Bayer mosaic with 2x2 tile: G B / R G. */
    SimdPixelFormatBayerGbrg,
    /*! Single-channel 8-bit Bayer mosaic with 2x2 tile: R G / G B. */
    SimdPixelFormatBayerRggb,
    /*! Single-channel 8-bit Bayer mosaic with 2x2 tile: B G / G R. */
    SimdPixelFormatBayerBggr,
    /*! Three 8-bit channels in HSV order: hue, saturation, value. */
    SimdPixelFormatHsv24,
    /*! Three 8-bit channels in HSL order: hue, saturation, lightness. */
    SimdPixelFormatHsl24,
    /*! Three 8-bit channels in RGB order: red, green, blue. */
    SimdPixelFormatRgb24,
    /*! Four 8-bit channels in RGBA order: red, green, blue, alpha. */
    SimdPixelFormatRgba32,
    /*! Packed UYVY422 format; every four bytes store two pixels as U0, Y0, V0, Y1. */
    SimdPixelFormatUyvy16,
    /*! Four 8-bit channels in ARGB order: alpha, red, green, blue. */
    SimdPixelFormatArgb32,
    /*! Three 8-bit channels in CIELAB order: L, a, b. */
    SimdPixelFormatLab24,
} SimdPixelFormatType;

/*! @ingroup recursive_bilateral_filter
    Describes flags for ::SimdRecursiveBilateralFilterInit.

    Values are bit fields combined with bitwise OR. One bit selects fast or precise processing, one
    field selected by ::SimdRecursiveBilateralFilterDiffMask selects how multi-channel color
    difference is estimated, and ::SimdRecursiveBilateralFilterFmaAvoid disables FMA code paths for
    debugging.
*/
typedef enum
{
    SimdRecursiveBilateralFilterFast = 0, /*!< Fast recursive bilateral filter implementation. */
    SimdRecursiveBilateralFilterPrecise = 1, /*!< Precise recursive bilateral filter implementation. */
    SimdRecursiveBilateralFilterDiffAvg = 0, /*!< Estimate color difference by average channel difference. */
    SimdRecursiveBilateralFilterDiffMax = 2, /*!< Estimate color difference by maximal channel difference. */
    SimdRecursiveBilateralFilterDiffSum = 4, /*!< Estimate color difference by saturated sum of channel differences. */
    SimdRecursiveBilateralFilterDiffMask = 6, /*!< Mask used to extract the color-difference field. */
    SimdRecursiveBilateralFilterFmaAvoid = 8, /*!< Avoid FMA instructions for debugging and bit-exact comparisons. */
} SimdRecursiveBilateralFilterFlags;

/*! @ingroup c_types
    Describes algorithms used by C++ wrapper Simd::ReduceGray to downscale an 8-bit gray image by two.

    All variants produce output size <tt>(srcWidth + 1)/2</tt> by <tt>(srcHeight + 1)/2</tt>. Border
    pixels are replicated when source coordinates fall outside the image.
*/
enum SimdReduceType
{
    SimdReduce2x2, /*!< Uses ::SimdReduceGray2x2: average over a 2x2 source block. */
    SimdReduce3x3, /*!< Uses ::SimdReduceGray3x3: separable Gaussian kernel [1 2 1]. */
    SimdReduce4x4, /*!< Uses ::SimdReduceGray4x4: separable kernel [1 3 3 1]. */
    SimdReduce5x5, /*!< Uses ::SimdReduceGray5x5: separable Gaussian kernel [1 4 6 4 1]. */
};

/*! @ingroup resizing
    Describes the scalar data type of each image channel used by ::SimdResizerInit.

    Strides passed to ::SimdResizerRun are always in bytes. The total pixel size is the channel size
    selected here multiplied by the channel count passed to ::SimdResizerInit.
*/
typedef enum
{
    /*! 8-bit unsigned integer channel; one byte per channel. */
    SimdResizeChannelByte,
    /*! 16-bit integer channel; two bytes per channel. */
    SimdResizeChannelShort,
    /*! 32-bit floating point channel; four bytes per channel. */
    SimdResizeChannelFloat,
    /*! 16-bit BFloat16 (Brain Floating Point) channel; two bytes per channel. */
    SimdResizeChannelBf16,

} SimdResizeChannelType;

/*! @ingroup resizing
    Describes interpolation and coordinate mapping methods used by ::SimdResizerInit.

    Supported method/channel combinations are implementation-dependent: nearest methods support all
    channel types; bilinear supports byte, short, float and BFloat16 channels; Caffe/PyTorch
    bilinear variants support float and BFloat16 channels; OpenCV bilinear, bicubic, area and
    area-fast variants support byte channels.
*/
typedef enum
{
    /*! Nearest-neighbor resize with default Simd coordinate mapping. */
    SimdResizeMethodNearest,
    /*! Nearest-neighbor resize with PyTorch-compatible coordinate mapping. */
    SimdResizeMethodNearestPytorch,
    /*! Bilinear interpolation with default Simd coordinate mapping. */
    SimdResizeMethodBilinear,
    /*! Bilinear interpolation with Caffe-compatible coordinate mapping; used for float and BFloat16 channels. */
    SimdResizeMethodBilinearCaffe,
    /*! Bilinear interpolation with PyTorch-compatible coordinate mapping; used for float and BFloat16 channels. */
    SimdResizeMethodBilinearPytorch,
    /*! Bilinear interpolation with OpenCV-compatible byte arithmetic and coordinate mapping. */
    SimdResizeMethodBilinearOpenCv,
    /*! Bicubic interpolation for byte channels. */
    SimdResizeMethodBicubic,
    /*! Area resize for byte channels. */
    SimdResizeMethodArea,
    /*! Fast area resize for byte-channel downscaling; uses a 2x2 path when the scale permits it. */
    SimdResizeMethodAreaFast,
} SimdResizeMethodType;

/*! @ingroup shifting
    Describes image texture used by ::SimdShiftDetectorInitBuffers to build background and current pyramids.

    The shift detector works with 8-bit gray images. It can compare either raw gray values or a
    gradient texture built from them before searching for the best translation.
*/
typedef enum
{
    /*! Use original 8-bit gray pixels as the texture. */
    SimdShiftDetectorTextureGray,
    /*! Use saturated sum of absolute X and Y gradients as the texture. */
    SimdShiftDetectorTextureGrad,
} SimdShiftDetectorTextureType;

/*! @ingroup shifting
    Describes difference metric used by ::SimdShiftDetectorInitBuffers to compare pyramid regions.

    The detector searches for a translation that minimizes the selected metric. The metric also
    affects the correlation value returned by ::SimdShiftDetectorGetShift.
*/
typedef enum 
{
    /*! Sum of absolute differences between background and current texture pixels. */
    SimdShiftDetectorAbsDifference,
    /*! Sum of squared differences between background and current texture pixels. */
    SimdShiftDetectorSquaredDifference,
} SimdShiftDetectorDifferenceType;

/*! @ingroup synet_types
    Describes Synet calculation compatibility flags.

    Values are grouped into independent bit fields and can be combined with bitwise OR. The FMA
    field controls use of fused multiply-add instructions, the 8-bit integer field controls
    quantized multiplication/range policy, and the BF16/FP16 fields request use of reduced-precision
    formats. Masks are provided to extract each field.

    This type is used in functions such as ::SimdSynetAdd8i, ::SimdSynetScaleLayerForward,
    ::SimdSynetConvert32fTo8u, ::SimdSynetConvert8uTo32f, ::SimdSynetInnerProduct8i,
    ::SimdSynetScale8iInit, ::SimdSynetConvolution16bInit, ::SimdSynetConvolution8iInit,
    ::SimdSynetMergedConvolution16bInit and ::SimdSynetMergedConvolution8iInit.
*/
typedef enum
{
    SimdSynetCompatibilityDefault = 0, /*!< Default compatibility value: use the fastest supported policy for each field. */
    SimdSynetCompatibilityFmaUse = 0, /*!< Allow FMA instructions where implementations support them. */
    SimdSynetCompatibilityFmaNoTail = 1, /*!< Avoid FMA instructions at row tails for stricter reproducibility. */
    SimdSynetCompatibilityFmaAvoid = 2, /*!< Avoid FMA instructions completely. */
    SimdSynetCompatibilityFmaMask = 3, /*!< Mask used to extract FMA policy bits. */
    SimdSynetCompatibility8iPrecise = 0, /*!< Use precise 8-bit integer multiplication (VNNI or 16-bit emulation). */
    SimdSynetCompatibility8iOverflow = 4, /*!< Allow 16-bit intermediate overflow in 8-bit integer multiplication paths. */
    SimdSynetCompatibility8iNarrowed = 8, /*!< Use narrowed input ranges (signed [-90..90], unsigned [0..180]) to avoid 16-bit overflow. */
    SimdSynetCompatibility8iMask = 12, /*!< Mask used to extract 8-bit integer multiplication policy bits. */
    SimdSynetCompatibility16bfAvoid = 0, /*!< Do not request BFloat16 (Brain Floating Point) internal computation. */
    SimdSynetCompatibility16bfHard = 16, /*!< Use BFloat16 internal computation only when hardware support exists. */
    SimdSynetCompatibility16bfSoft = 32, /*!< Use BFloat16 internal computation with software emulation when hardware support is absent. */
    SimdSynetCompatibility16bfMask = 48, /*!< Mask used to extract BFloat16 policy bits. */
    SimdSynetCompatibility16fpAvoid = 0, /*!< Do not request 16-bit floating point (Half Precision) internal computation. */
    SimdSynetCompatibility16fpHard = 64, /*!< Use 16-bit floating point internal computation only when hardware support exists. */
    SimdSynetCompatibility16fpSoft = 128, /*!< Use 16-bit floating point internal computation with software emulation when hardware support is absent. */
    SimdSynetCompatibility16fpMask = 192, /*!< Mask used to extract 16-bit floating point policy bits. */
} SimdSynetCompatibilityType;

/*! @ingroup synet_types
    Describes operation type used by ::SimdSynetEltwiseLayerForward.

    The function combines at least two equally sized FP32 arrays element by element. The weight array
    is used only by ::SimdSynetEltwiseOperationSum.
*/
typedef enum
{
    SimdSynetEltwiseOperationProduct, /*!< Product of corresponding elements from all input arrays. */
    SimdSynetEltwiseOperationSum, /*!< Weighted sum of corresponding elements from all input arrays. */
    SimdSynetEltwiseOperationMax, /*!< Maximum of corresponding elements from all input arrays. */
    SimdSynetEltwiseOperationMin, /*!< Minimum of corresponding elements from all input arrays. */
} SimdSynetEltwiseOperationType;

/*! @ingroup synet_types
    Describes unary operation type used by ::SimdSynetUnaryOperation32f.

    Each operation is applied independently to every 32-bit floating point element of the input array.
*/
typedef enum
{
    /*! Absolute value: <tt>dst = Abs(src)</tt>. */
    SimdSynetUnaryOperation32fAbs,
    /*! Ceiling: <tt>dst = Ceil(src)</tt>. */
    SimdSynetUnaryOperation32fCeil,
    /*! Cosine: <tt>dst = Cos(src)</tt>. */
    SimdSynetUnaryOperation32fCos,
    /*! Error function: <tt>dst = Erf(src)</tt>. */
    SimdSynetUnaryOperation32fErf,
    /*! Exponent: <tt>dst = Exp(src)</tt>. */
    SimdSynetUnaryOperation32fExp,
    /*! Floor: <tt>dst = Floor(src)</tt>. */
    SimdSynetUnaryOperation32fFloor,
    /*! Natural logarithm: <tt>dst = Log(src)</tt>. */
    SimdSynetUnaryOperation32fLog,
    /*! Negation: <tt>dst = -src</tt>. */
    SimdSynetUnaryOperation32fNeg,
    /*! Bitwise NOT applied to the IEEE-754 representation of the source value. */
    SimdSynetUnaryOperation32fNot,
    /*! Reciprocal: <tt>dst = 1/src</tt>. */
    SimdSynetUnaryOperation32fRcp,
    /*! Round to nearest integer value in FP32 representation. */
    SimdSynetUnaryOperation32fRound,
    /*! Reciprocal square root: <tt>dst = 1/Sqrt(src)</tt>. */
    SimdSynetUnaryOperation32fRsqrt,
    /*! Sign function: -1 for negative values, 0 for zero and 1 for positive values. */
    SimdSynetUnaryOperation32fSign,
    /*! Sine: <tt>dst = Sin(src)</tt>. */
    SimdSynetUnaryOperation32fSin,
    /*! Square root: <tt>dst = Sqrt(src)</tt>. */
    SimdSynetUnaryOperation32fSqrt,
    /*! Hyperbolic tangent: <tt>dst = Tanh(src)</tt>. */
    SimdSynetUnaryOperation32fTanh,
    /*! Zeroing: <tt>dst = 0</tt>. */
    SimdSynetUnaryOperation32fZero,
} SimdSynetUnaryOperation32fType;

/*! @ingroup synet_types
    Describes tensor memory layout used by Synet functions.

    Most functions use 4D tensors with dimensions batch (N), channels (C), height (H) and width (W).
    Some shape-based helper functions accept ::SimdTensorFormatUnknown when layout is irrelevant.
*/
typedef enum
{
    SimdTensorFormatUnknown = -1, /*!< Unknown or layout-independent tensor format. */
    SimdTensorFormatNchw, /*!< NCHW layout: offset = ((n*C + c)*H + h)*W + w. */
    SimdTensorFormatNhwc, /*!< NHWC layout: offset = ((n*H + h)*W + w)*C + c. */
} SimdTensorFormatType;

/*! @ingroup synet_types
    Describes tensor element data type used by Synet functions.

    The value defines interpretation and size of each tensor element. Reduced precision values
    ::SimdTensorData16b and ::SimdTensorData16f are stored in 16-bit containers.
*/
typedef enum
{
    SimdTensorDataUnknown = -1, /*!< Unknown tensor data type. */
    SimdTensorData32f, /*!< 32-bit floating point (single precision). */
    SimdTensorData32i, /*!< 32-bit signed integer. */
    SimdTensorData8i, /*!< 8-bit signed integer. */
    SimdTensorData8u, /*!< 8-bit unsigned integer. */
    SimdTensorData64i, /*!< 64-bit signed integer. */
    SimdTensorData64u, /*!< 64-bit unsigned integer. */
    SimdTensorDataBool, /*!< Boolean value stored in one byte. */
    SimdTensorData16b, /*!< 16-bit BFloat16 (Brain Floating Point) stored in uint16_t. */
    SimdTensorData16f, /*!< 16-bit floating point (Half Precision) stored in uint16_t/int16_t. */
} SimdTensorDataType;

/*! @ingroup transform
    Describes image transform type used by ::SimdTransformImage.

    The transform maps an input image to an output image with the same pixel size. ::SimdTransformRotate90,
    ::SimdTransformRotate270, ::SimdTransformTransposeRotate0 and ::SimdTransformTransposeRotate180
    swap output width and height. The other transform types keep the original size.
*/
typedef enum
{
    SimdTransformRotate0 = 0, /*!< Copy without rotation; output size equals input size. */
    SimdTransformRotate90, /*!< Rotate 90 degrees counterclockwise; output width/height are input height/width. */
    SimdTransformRotate180, /*!< Rotate 180 degrees; output size equals input size. */
    SimdTransformRotate270, /*!< Rotate 270 degrees counterclockwise; output width/height are input height/width. */
    SimdTransformTransposeRotate0, /*!< Transpose over the main diagonal; output width/height are input height/width. */
    SimdTransformTransposeRotate90, /*!< Transpose then rotate 90 degrees counterclockwise; equivalent to horizontal mirror. */
    SimdTransformTransposeRotate180, /*!< Transpose then rotate 180 degrees; output width/height are input height/width. */
    SimdTransformTransposeRotate270, /*!< Transpose then rotate 270 degrees counterclockwise; equivalent to vertical mirror. */
} SimdTransformType;

/*! @ingroup warp_affine
    Describes bit flags used by ::SimdWarpAffineInit.

    Values are grouped into independent bit fields for channel type, interpolation method and border
    handling. Combine one value from each field with bitwise OR. Current implementations support
    8-bit channels, nearest or bilinear interpolation, and constant or transparent border handling.
*/
typedef enum
{
    SimdWarpAffineDefault = 0, /*!< Default flags: byte channels, nearest interpolation and constant border. */
    SimdWarpAffineChannelByte = 0, /*!< 8-bit unsigned integer channel type. */
    SimdWarpAffineChannelMask = 1, /*!< Mask used to extract channel type bits. */
    SimdWarpAffineInterpNearest = 0, /*!< Nearest-neighbor source sampling. */
    SimdWarpAffineInterpBilinear = 2, /*!< Bilinear source sampling. */
    SimdWarpAffineInterpMask = 2, /*!< Mask used to extract interpolation method bits. */
    SimdWarpAffineBorderConstant = 0, /*!< Fill out-of-source pixels with the border color, or zero when border is NULL. */
    SimdWarpAffineBorderTransparent = 4, /*!< Leave destination pixels unchanged when their source coordinate is outside the source image. */
    SimdWarpAffineBorderMask = 4, /*!< Mask used to extract border handling bits. */
} SimdWarpAffineFlags;

/*! @ingroup yuv_conversion
    Describes YUV color conversion standard used by YUV/BGR conversion and YUV JPEG saving functions.

    The value selects RGB-to-luma coefficients and limited/full range conversion rules. BT.601,
    BT.709 and BT.2020 use studio range Y [16..235] and U/V [16..240]. T-REC-T.871 uses full
    range [0..255] and JPEG-compatible coefficients.
*/
typedef enum
{
    SimdYuvUnknown = -1, /*!< Unknown or unsupported YUV conversion standard. */
    SimdYuvBt601, /*!< BT.601 limited range conversion: Kr=0.299, Kb=0.114. */
    SimdYuvBt709, /*!< BT.709 limited range conversion: Kr=0.2126, Kb=0.0722. */
    SimdYuvBt2020, /*!< BT.2020 limited range conversion: Kr=0.2627, Kb=0.0593. */
    SimdYuvTrect871, /*!< T-REC-T.871 full range conversion: Kr=0.299, Kb=0.114. */
} SimdYuvType;

/*! @ingroup synet_types
    Describes convolution and deconvolution geometry, tensor types and activation.

    This structure is passed to convolution, deconvolution and merged-convolution initialization
    functions. For convolution, destination spatial size must satisfy:
    \verbatim
    dstH = (srcH + padY + padH - (dilationY*(kernelY - 1) + 1)) / strideY + 1
    dstW = (srcW + padX + padW - (dilationX*(kernelX - 1) + 1)) / strideX + 1
    \endverbatim
    For deconvolution, destination spatial size must satisfy:
    \verbatim
    dstH = strideY*(srcH - 1) + dilationY*(kernelY - 1) + 1 - padY - padH
    dstW = strideX*(srcW - 1) + dilationX*(kernelX - 1) + 1 - padX - padW
    \endverbatim
    The weight tensor contains <tt>kernelY*kernelX*srcC*dstC/group</tt> elements. A depthwise
    convolution is represented by <tt>group == srcC == dstC</tt>.
*/
typedef struct SimdConvolutionParameters
{
    /*!
        Number of input tensor channels (C dimension of the source tensor).
    */
    size_t srcC;
    /*!
        Input tensor height (H dimension of the source tensor).
    */
    size_t srcH;
    /*!
        Input tensor width (W dimension of the source tensor).
    */
    size_t srcW;
    /*!
        Input tensor element type.
    */
    SimdTensorDataType srcT;
    /*!
        Input tensor memory format. Convolution implementations support NCHW or NHWC.
    */
    SimdTensorFormatType srcF;
    /*!
        Number of output tensor channels (C dimension of the destination tensor).
    */
    size_t dstC;
    /*!
        Output tensor height (H dimension of the destination tensor).
    */
    size_t dstH;
    /*!
        Output tensor width (W dimension of the destination tensor).
    */
    size_t dstW;
    /*!
        Output tensor element type.
    */
    SimdTensorDataType dstT;
    /*!
        Output tensor memory format. It normally has to match srcF.
    */
    SimdTensorFormatType dstF;
    /*!
        Kernel height.
    */
    size_t kernelY;
    /*!
        Kernel width.
    */
    size_t kernelX;
    /*!
        Dilation along the Y axis. A value of 1 means adjacent kernel rows.
    */
    size_t dilationY;
    /*!
        Dilation along the X axis. A value of 1 means adjacent kernel columns.
    */
    size_t dilationX;
    /*!
        Stride along the Y axis.
    */
    size_t strideY;
    /*!
        Stride along the X axis.
    */
    size_t strideX;
    /*!
        Zero padding before the input image along the Y axis (top padding).
    */
    size_t padY;
    /*!
        Zero padding before the input image along the X axis (left padding).
    */
    size_t padX;
    /*!
        Zero padding after the input image along the Y axis (bottom padding).
    */
    size_t padH;
    /*!
        Zero padding after the input image along the X axis (right padding).
    */
    size_t padW;
    /*!
        Number of convolution groups. srcC and dstC are split by this value.
    */
    size_t group;
    /*!
        Activation function applied after convolution/deconvolution accumulation and bias addition.
    */
    SimdConvolutionActivationType activation;
} SimdConvolutionParameters;

#if defined(_WIN32) && !defined(SIMD_STATIC)
#  ifdef SIMD_EXPORTS
#    define SIMD_API __declspec(dllexport)
#  else
#    define SIMD_API __declspec(dllimport)
#  endif
#elif defined(__GNUC__) && defined(SIMD_HIDE_INTERNAL)
#    define SIMD_API __attribute__ ((visibility ("default")))
#else
#    define SIMD_API
#endif

#ifdef __cplusplus
extern "C"
{
#endif

    /*! @ingroup info

        \fn const char * SimdVersion();

        \short Gets version of %Simd Library.

        Returns a pointer to a null-terminated, statically allocated string that encodes the library version.
        The format of the string is:
        \verbatim
        major.minor.release[.branch-sha]
        \endverbatim
        where \b major, \b minor and \b release are numeric components taken from the library's version file,
        and the optional \b branch and \b sha suffix identify the Git branch name and short commit hash at
        build time (e.g. <tt>"7.1.161.main-a1b2c3d"</tt>). When version information is not available at
        build time the function returns <tt>"unknown"</tt>.

        The returned pointer is valid for the lifetime of the process and must not be freed.

        Using example:
        \verbatim
        #include "Simd/SimdLib.h"
        #include <iostream>

        int main()
        {
            std::cout << "Simd Library version: " << SimdVersion() << std::endl;
            return 0;
        }
        \endverbatim

        \return a pointer to a static null-terminated string with the version of %Simd Library.
    */
    SIMD_API const char * SimdVersion(void);

    /*! @ingroup info

        \fn const char* SimdCpuDesc(SimdCpuDescType type);

        \short Gets a text description of the CPU.

        Returns a pointer to a null-terminated string whose content depends on the requested ::SimdCpuDescType:
        - ::SimdCpuDescModel — the CPU brand/model name string (e.g. <tt>"Intel(R) Core(TM) i7-8700 CPU @ 3.20GHz"</tt>).
          On x86 it is read from the CPUID brand-string leaves; on Linux/ARM it is obtained via \c lscpu.
          An empty string is returned on platforms where the model name is not available (Apple, Android).

        The returned pointer is valid for the lifetime of the process and must not be freed.
        For an unknown or unsupported \a type value the function returns \c NULL.

        \note See enumeration ::SimdCpuDescType for the full list of supported types.

        Using example:
        \verbatim
        #include "Simd/SimdLib.h"
        #include <iostream>

        int main()
        {
            std::cout << "CPU model: " << SimdCpuDesc(SimdCpuDescModel) << std::endl;
            return 0;
        }
        \endverbatim

        \param [in] type - a type of required description. See ::SimdCpuDescType.
        \return a pointer to a static null-terminated string with the requested CPU description,
                or \c NULL if \a type is not supported.
    */
    SIMD_API const char* SimdCpuDesc(SimdCpuDescType type);

    /*! @ingroup info

        \fn uint64_t SimdCpuInfo(SimdCpuInfoType type);

        \short Gets information about CPU and %Simd Library.

        Depending on the requested ::SimdCpuInfoType, the function returns one of the following kinds of values:
        - CPU topology: number of sockets, physical cores, or logical threads.
        - Cache / RAM sizes in bytes (L1 data cache, L2 cache, L3 cache, physical RAM).
        - SIMD extension availability: 1 if the extension is supported and enabled by the library, 0 otherwise.
          The extensions covered are SSE4.1 (and below), AVX2 (and FMA/AVX), AVX-512BW (and AVX-512F),
          AVX-512VNNI, AMX-BF16 (and AMX-INT8/AVX-512VBMI/AVX-512FP16), NEON, SVE, and HVX.
        - SVE vector width in bytes (::SimdCpuInfoSveSize).
        - Current CPU core frequency in Hz (::SimdCpuInfoCurrentFrequency); returns 0 if unavailable on the platform.

        \note See enumeration ::SimdCpuInfoType.

        Using example:
        \verbatim
        #include "Simd/SimdLib.h"
        #include <iostream>

        int main()
        {
            std::cout << "Sockets: " << SimdCpuInfo(SimdCpuInfoSockets) << std::endl;
            std::cout << "Cores: " << SimdCpuInfo(SimdCpuInfoCores) << std::endl;
            std::cout << "Threads: " << SimdCpuInfo(SimdCpuInfoThreads) << std::endl;
            std::cout << "L1D Cache: " << SimdCpuInfo(SimdCpuInfoCacheL1) / 1024  << " KB" << std::endl;
            std::cout << "L2 Cache: " << SimdCpuInfo(SimdCpuInfoCacheL2) / 1024  << " KB" << std::endl;
            std::cout << "L3 Cache: " << SimdCpuInfo(SimdCpuInfoCacheL3) / 1024  << " KB" << std::endl;
            std::cout << "RAM: " << SimdCpuInfo(SimdCpuInfoRam) / 1024 / 1024 << " MB" << std::endl;
            std::cout << "SSE4.1: " << (SimdCpuInfo(SimdCpuInfoSse41) ? "Yes" : "No") << std::endl;
            std::cout << "AVX2: " << (SimdCpuInfo(SimdCpuInfoAvx2) ? "Yes" : "No") << std::endl;
            std::cout << "AVX-512BW: " << (SimdCpuInfo(SimdCpuInfoAvx512bw) ? "Yes" : "No") << std::endl;
            std::cout << "AVX-512VNNI: " << (SimdCpuInfo(SimdCpuInfoAvx512vnni) ? "Yes" : "No") << std::endl;
            std::cout << "AMX-BF16: " << (SimdCpuInfo(SimdCpuInfoAmxBf16) ? "Yes" : "No") << std::endl;
            std::cout << "ARM-NEON: " << (SimdCpuInfo(SimdCpuInfoNeon) ? "Yes" : "No") << std::endl;
            std::cout << "ARM-SVE: " << (SimdCpuInfo(SimdCpuInfoSve) ? "Yes" : "No") << std::endl;
            std::cout << "ARM-SVE size: " << SimdCpuInfo(SimdCpuInfoSveSize) * 8 << " bits" << std::endl;
            std::cout << "HVX: " << (SimdCpuInfo(SimdCpuInfoHvx) ? "Yes" : "No") << std::endl;
            std::cout << "Current frequency: " << SimdCpuInfo(SimdCpuInfoCurrentFrequency) / 1000000 << " MHz" << std::endl;
            return 0;
        }
        \endverbatim

        \param [in] type - a type of required information.
        \return a value whose meaning depends on \a type: a count (topology), size in bytes (cache/RAM),
                1 or 0 (SIMD availability), size in bytes (SVE vector width), or frequency in Hz (current CPU frequency).
    */
    SIMD_API uint64_t SimdCpuInfo(SimdCpuInfoType type);

    /*! @ingroup info

        \fn const char *SimdPerformanceStatistic();

        \short Gets internal performance statistics of %Simd Library.

        \note %Simd Library have to be build with defined SIMD_PERFORMANCE_STATISTIC macro.

        \return string with internal performance statistics of %Simd Library.
    */
    SIMD_API const char * SimdPerformanceStatistic(void);

    /*! @ingroup memory

        \fn void * SimdAllocate(size_t size, size_t align);

        \short Allocates an aligned memory block.

        Allocates a contiguous memory block of at least \a size bytes whose start address is a multiple of \a align.
        The alignment value must be a power of two and, on POSIX platforms (GCC), is rounded up to at least
        <tt>sizeof(void*)</tt> internally. The actual allocation is performed via the platform-appropriate
        aligned allocator: \c _aligned_malloc (MSVC), \c __mingw_aligned_malloc (MinGW), \c posix_memalign (GCC),
        or plain \c malloc on platforms that do not support aligned allocation.

        The block must be released with ::SimdFree — passing it to the standard \c free or \c delete is undefined behaviour.

        Using example:
        \verbatim
        #include "Simd/SimdLib.h"

        int main()
        {
            const size_t size  = 1024;
            const size_t align = SimdAlignment();
            uint8_t * data = (uint8_t *)SimdAllocate(size, align);
            if (data)
            {
                // use data ...
                SimdFree(data);
            }
            return 0;
        }
        \endverbatim

        \param [in] size - the number of bytes to allocate. Must be greater than zero.
        \param [in] align - the required alignment of the allocated block in bytes. Must be a power of two.
                            Use ::SimdAlignment to obtain the optimal alignment for the current platform.
        \return a pointer to the newly allocated aligned memory block, or \c NULL if the allocation fails.
    */
    SIMD_API void * SimdAllocate(size_t size, size_t align);

    /*! @ingroup memory

        \fn void SimdFree(void * ptr);

        \short Frees an aligned memory block previously allocated by ::SimdAllocate.

        Releases the memory block pointed to by \a ptr, which must have been returned by a prior call to
        ::SimdAllocate. Passing a pointer obtained from any other allocator (e.g. \c malloc, \c new,
        or \c _aligned_malloc) is undefined behaviour.

        Passing \c NULL is safe and has no effect, consistent with the behaviour of the standard \c free function.

        The underlying release call matches the allocator used by ::SimdAllocate for the current platform:
        \c _aligned_free (MSVC), \c __mingw_aligned_free (MinGW), or \c free (GCC and others).

        \param [in] ptr - a pointer to the memory block to free. Must have been returned by ::SimdAllocate,
                          or \c NULL (in which case the call has no effect).
    */
    SIMD_API void SimdFree(void * ptr);

    /*! @ingroup memory

        \fn size_t SimdAlign(size_t size, size_t align);

        \short Rounds a size value up to the nearest multiple of a given alignment.

        Returns the smallest value that is both a multiple of \a align and greater than or equal to \a size.
        If \a size is already a multiple of \a align, it is returned unchanged.

        The function uses the bitwise formula <tt>(size + align - 1) & ~(align - 1)</tt>, which requires
        \a align to be a positive power of two.

        \param [in] size - the original size in bytes (or elements) to be aligned.
        \param [in] align - the required alignment in bytes. Must be a positive power of two.
                            Use ::SimdAlignment to obtain the optimal alignment for the current platform.

        \return the smallest multiple of \a align that is greater than or equal to \a size.
    */
    SIMD_API size_t SimdAlign(size_t size, size_t align);

    /*! @ingroup memory

        \fn size_t SimdAlignment();

        \short Returns the optimal memory alignment for the current platform.

        Returns the byte-width of the widest SIMD register available at runtime, which is the recommended
        alignment value to pass to ::SimdAllocate and ::SimdAlign in order to achieve best performance.

        The value is determined once at library initialization time by probing the active SIMD extensions
        and is constant for the lifetime of the process:
        - \b 128 bytes — HVX (Qualcomm Hexagon)
        - \b 64 bytes — AVX-512 (x86, when either AVX-512BW or AVX-512VNNI is available)
        - \b 32 bytes — AVX2 (x86)
        - \b 16 bytes — SSE4.1 (x86) or NEON (ARM)
        - <b>sizeof(void*)</b> — scalar fallback (no SIMD extensions detected)
        - \b SVE vector size for current CPU in bytes — when SVE is available.

        The returned value is always a power of two and equals the value of the \c SIMD_ALIGN compile-time
        constant used internally by the library.

        \return the optimal alignment in bytes for the current platform.
    */
    SIMD_API size_t SimdAlignment(void);

    /*! @ingroup memory

        \fn void SimdRelease(void * context);

        \short Destroys an opaque context object created by the Simd Library API.

        Releases any context object returned by a Simd Library context-creation function,
        i.e. any function whose name ends in \c Init (such as ::SimdGaussianBlurInit,
        ::SimdResizerInit, ::SimdWarpAffineInit, ::SimdDescrIntInit, ::SimdFontInit,
        ::SimdSynetConvolution32fInit, and others), as well as ::SimdDetectionLoadA.

        Internally the function performs a polymorphic \c delete through the virtual
        destructor of the internal \c Deletable base class, ensuring that the correct
        destructor is always invoked regardless of the actual context type.

        Passing \c NULL is safe and has no effect, consistent with the behaviour of a
        C++ \c delete expression on a null pointer.

        \note Passing a pointer that was not returned by a Simd Library context-creation
              function (for example a pointer from ::SimdAllocate, \c malloc, or \c new)
              is undefined behaviour.

        \param [in] context - a pointer to the context to be released, or \c NULL.
    */    
    SIMD_API void SimdRelease(void * context);

    /*! @ingroup thread

        \fn size_t SimdGetThreadNumber();

        \short Gets current global thread number configured for Simd Library parallel algorithms.

        Returns the value set by ::SimdSetThreadNumber. By default this value is \c 1.
        When set, it is restricted to the range \c [1, std::thread::hardware_concurrency()].

        \return current configured thread number.
    */
    SIMD_API size_t SimdGetThreadNumber(void);

    /*! @ingroup thread

        \fn void SimdSetThreadNumber(size_t threadNumber);

        \short Sets number of threads used by Simd Library to parallelize some algorithms.

        \param [in] threadNumber - a number of threads.
    */
    SIMD_API void SimdSetThreadNumber(size_t threadNumber);

    /*! @ingroup cpu_flags

        \fn void SimdEmpty();

        \short Clears MMX state for x86 SIMD code paths.

        On supported x86 builds this function executes \c EMMS (via \c _mm_empty()) when
        the SSE4.1 backend is enabled at runtime. In other configurations it does nothing.
    */
    SIMD_API void SimdEmpty(void);

    /*! @ingroup cpu_flags

        \fn SimdBool SimdGetFastMode();

        \short Gets the current 'fast mode' state for floating-point subnormal handling.

        When 'fast mode' is active, subnormal (denormalized) floating-point values are flushed
        to zero by the hardware rather than being processed normally, which avoids the significant
        performance penalty of software-assisted denormal handling.

        On x86 platforms with SSE4.1 support, reads the MXCSR register and returns \c SimdTrue
        when either the Flush-To-Zero (FTZ, bit 15) or the Denormals-Are-Zero (DAZ, bit 6)
        bit is set. On ARM platforms with Neon support, reads the FPSCR (AArch32) or FPCR
        (AArch64) register and returns \c SimdTrue when the Flush-To-Zero (FTZ, bit 24) bit
        is set. On platforms without hardware support for this feature, always returns \c SimdFalse.

        \return \c SimdTrue if fast mode is currently enabled, \c SimdFalse otherwise.
    */
    SIMD_API SimdBool SimdGetFastMode(void);

    /*! @ingroup cpu_flags

        \fn void SimdSetFastMode(SimdBool value);

        \short Sets the current thread's 'fast mode' state for floating-point subnormal handling.

        When 'fast mode' is enabled, subnormal (denormalized) floating-point values are flushed
        to zero by the hardware rather than being processed normally, which avoids the significant
        performance penalty of software-assisted denormal handling.

        On x86 platforms with SSE4.1 support, sets or clears both the Flush-To-Zero (FTZ, bit 15)
        and the Denormals-Are-Zero (DAZ, bit 6) bits in the current thread's MXCSR register. On ARM
        platforms with Neon support, sets or clears the Flush-To-Zero (FTZ, bit 24) bit in the current
        thread's FPSCR (AArch32) or FPCR (AArch64) register. Has no effect when this feature is not
        supported or the corresponding SIMD backend is not enabled at runtime.

        \param [in] value - \c SimdTrue to enable fast mode, \c SimdFalse to disable it.
    */
    SIMD_API void SimdSetFastMode(SimdBool value);

    /*! @ingroup cpu_flags

        \fn void SimdSetAmxFull();

        \short Loads the full AMX tile configuration for the current thread.

        On x86 platforms with AMX-BF16 support, this function forces loading of a predefined full
        AMX tile configuration into the current thread by calling the AMX tile configuration
        instruction. It is intended for code paths that need the full tile layout used by the
        library.

        Has no effect on platforms without AMX-BF16 support or when the corresponding SIMD backend
        is not enabled at runtime.
    */
    SIMD_API void SimdSetAmxFull(void);

    /*! @ingroup hash

        \fn uint32_t SimdCrc32(const void * src, size_t size);

        \short Calculates 32-bit cyclic redundancy check (CRC-32) for input data.

        The function uses reflected polynomial 0xEDB88320, initial value 0xFFFFFFFF and final bitwise inversion.

        \param [in] src - a pointer to data.
        \param [in] size - a size of the data.
        \return 32-bit cyclic redundancy check (CRC-32) of the input buffer.
    */
    SIMD_API uint32_t SimdCrc32(const void* src, size_t size);

    /*! @ingroup hash

        \fn uint32_t SimdCrc32c(const void * src, size_t size);

        \short Calculates 32-bit cyclic redundancy check (CRC-32C, Castagnoli) for input data.

        The function uses Castagnoli polynomial (reflected form 0x82F63B78, normal form 0x1EDC6F41),
        initial value 0xFFFFFFFF and final bitwise inversion.

        \param [in] src - a pointer to data.
        \param [in] size - a size of the data.
        \return 32-bit cyclic redundancy check (CRC-32C) of the input buffer.
    */
    SIMD_API uint32_t SimdCrc32c(const void * src, size_t size);

    /*! @ingroup correlation

        \fn void SimdAbsDifference(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride, uint8_t * c, size_t cStride, size_t width, size_t height);

        \short Calculates per-pixel absolute difference of two gray 8-bit images.

        The destination pixel values are computed as:
        \verbatim
        c[x, y] = abs(a[x, y] - b[x, y]).
        \endverbatim
        All three images must have the same width and height.

        \note This function has a C++ wrapper Simd::AbsDifference(const View<A> & a, const View<A> & b, View<A> & c).

        \param [in] a - a pointer to pixels data of first image.
        \param [in] aStride - a row size of first image.
        \param [in] b - a pointer to pixels data of second image.
        \param [in] bStride - a row size of second image.
        \param [out] c - a pointer to pixels data of destination image.
        \param [in] cStride - a row size of destination image.
        \param [in] width - an image width.
        \param [in] height - an image height.
    */
    SIMD_API void SimdAbsDifference(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride, uint8_t * c, size_t cStride, size_t width, size_t height);

    /*! @ingroup correlation

        \fn void SimdAbsDifferenceSum(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride, size_t width, size_t height, uint64_t * sum);

        \short Calculates sum of absolute differences (SAD) of two gray 8-bit images.

        The result value is computed as:
        \verbatim
        sum = Σ abs(a[x, y] - b[x, y]).
        \endverbatim
        Both images must have the same width and height.

        \note This function has a C++ wrapper Simd::AbsDifferenceSum(const View<A> & a, const View<A> & b, uint64_t & sum).

        \param [in] a - a pointer to pixels data of first image.
        \param [in] aStride - a row size of first image.
        \param [in] b - a pointer to pixels data of second image.
        \param [in] bStride - a row size of second image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sum - the result sum of absolute difference of two images.
    */
    SIMD_API void SimdAbsDifferenceSum(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride, size_t width, size_t height, uint64_t * sum);

    /*! @ingroup correlation

        \fn void SimdAbsDifferenceSumMasked(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride, const uint8_t * mask, size_t maskStride, uint8_t index, size_t width, size_t height, uint64_t * sum);

        \short Calculates masked sum of absolute differences (SAD) of two gray 8-bit images.

        The result value is computed for points where mask[x, y] equals index:
        \verbatim
        sum = Σ abs(a[x, y] - b[x, y]), for all (x, y) where mask[x, y] == index.
        \endverbatim
        Both images and mask must have the same width and height.

        \note This function has a C++ wrapper Simd::AbsDifferenceSum(const View<A>& a, const View<A>& b, const View<A>& mask, uint8_t index, uint64_t & sum).

        \param [in] a - a pointer to pixels data of first image.
        \param [in] aStride - a row size of first image.
        \param [in] b - a pointer to pixels data of second image.
        \param [in] bStride - a row size of second image.
        \param [in] mask - a pointer to pixels data of mask image.
        \param [in] maskStride - a row size of mask image.
        \param [in] index - a mask index.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sum - the result sum of absolute difference of two images.
    */
    SIMD_API void SimdAbsDifferenceSumMasked(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride,
        const uint8_t * mask, size_t maskStride, uint8_t index, size_t width, size_t height, uint64_t * sum);

    /*! @ingroup correlation

        \fn void SimdAbsDifferenceSums3x3(const uint8_t * current, size_t currentStride, const uint8_t * background, size_t backgroundStride, size_t width, size_t height, uint64_t * sums);

        \short Calculates 9 sums of absolute differences for all shifts in 3x3 neighborhood.

        Both images must have the same width and height. The image height and width must be equal or greater 3.
        The sums are computed for the central part of current image (without one-pixel border) and for background image
        shifted by dx and dy in range [-1, 1]:
        \verbatim
        sums[(dy + 1)*3 + (dx + 1)] = Σ abs(current[x, y] - background[x + dx, y + dy]),
                                       x = 1..width-2, y = 1..height-2.
        \endverbatim
        Output order is: (-1,-1), (0,-1), (1,-1), (-1,0), (0,0), (1,0), (-1,1), (0,1), (1,1).

        \note This function has a C++ wrapper Simd::AbsDifferenceSums3x3(const View<A>& current, const View<A>& background, uint64_t * sums).

        \param [in] current - a pointer to pixels data of current image.
        \param [in] currentStride - a row size of the current image.
        \param [in] background - a pointer to pixels data of the background image.
        \param [in] backgroundStride - a row size of the background image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sums - the pointer to buffer with result sums. Buffer size must be equal or greater 9.
    */
    SIMD_API void SimdAbsDifferenceSums3x3(const uint8_t * current, size_t currentStride, const uint8_t * background, size_t backgroundStride,
        size_t width, size_t height, uint64_t * sums);

    /*! @ingroup correlation

        \fn void SimdAbsDifferenceSums3x3Masked(const uint8_t *current, size_t currentStride, const uint8_t *background, size_t backgroundStride, const uint8_t *mask, size_t maskStride, uint8_t index, size_t width, size_t height, uint64_t * sums);

        \short Calculates 9 masked sums of absolute differences for all shifts in 3x3 neighborhood.

        The sums are computed only for points where mask[x, y] equals index:
        \verbatim
        sums[(dy + 1)*3 + (dx + 1)] = Σ abs(current[x, y] - background[x + dx, y + dy]),
                                       for x = 1..width-2, y = 1..height-2 and mask[x, y] == index.
        \endverbatim
        Both images and mask must have the same width and height. The image height and width must be equal or greater 3.
        Output order is: (-1,-1), (0,-1), (1,-1), (-1,0), (0,0), (1,0), (-1,1), (0,1), (1,1).

        \note This function has a C++ wrapper Simd::AbsDifferenceSums3x3(const View<A>& current, const View<A>& background, const View<A>& mask, uint8_t index, uint64_t * sums).

        \param [in] current - a pointer to pixels data of current image.
        \param [in] currentStride - a row size of the current image.
        \param [in] background - a pointer to pixels data of the background image.
        \param [in] backgroundStride - a row size of the background image.
        \param [in] mask - a pointer to pixels data of mask image.
        \param [in] maskStride - a row size of mask image.
        \param [in] index - a mask index.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sums - the pointer to buffer with result sums. Buffer size must be equal or greater 9.
    */
    SIMD_API void SimdAbsDifferenceSums3x3Masked(const uint8_t *current, size_t currentStride, const uint8_t *background, size_t backgroundStride,
        const uint8_t *mask, size_t maskStride, uint8_t index, size_t width, size_t height, uint64_t * sums);

    /*! @ingroup other_filter

        \fn void SimdAbsGradientSaturatedSum(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

        \short Calculates saturated sum of horizontal and vertical absolute gradients for each pixel of 8-bit gray image.

        Both images must have the same width and height.

        For border pixels:
        \verbatim
        dst[x, y] = 0;
        \endverbatim

        For non-border pixels:
        \verbatim
        dx = abs(src[x + 1, y] - src[x - 1, y]);
        dy = abs(src[x, y + 1] - src[x, y - 1]);
        dst[x, y] = min(dx + dy, 255);
        \endverbatim

        \note This function has a C++ wrapper Simd::AbsGradientSaturatedSum(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of source 8-bit gray image.
        \param [in] srcStride - a row size of source image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of destination 8-bit gray image.
        \param [in] dstStride - a row size of destination image.
    */
    SIMD_API void SimdAbsGradientSaturatedSum(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        uint8_t * dst, size_t dstStride);

    /*! @ingroup difference_estimation

        \fn void SimdAddFeatureDifference(const uint8_t * value, size_t valueStride, size_t width, size_t height, const uint8_t * lo, size_t loStride, const uint8_t * hi, size_t hiStride, uint16_t weight, uint8_t * difference, size_t differenceStride);

        \short Accumulates weighted feature difference into 8-bit difference map.

        All images must have the same width, height and format (8-bit gray).

        For every point:
        \verbatim
        excess = max(max(value[i] - hi[i], lo[i] - value[i]), 0);
        difference[i] = min(difference[i] + ((weight * excess * excess) >> 16), 255);
        \endverbatim

        This function is used for difference estimation in algorithm of motion detection.

        \note This function has a C++ wrapper Simd::AddFeatureDifference(const View<A>& value, const View<A>& lo, const View<A>& hi, uint16_t weight, View<A>& difference).

        \param [in] value - a pointer to pixels data of current feature value.
        \param [in] valueStride - a row size of the value image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] lo - a pointer to pixels data of feature lower bound of dynamic background.
        \param [in] loStride - a row size of the lo image.
        \param [in] hi - a pointer to pixels data of feature upper bound of dynamic background.
        \param [in] hiStride - a row size of the hi image.
        \param [in] weight - a current feature weight (unsigned 16-bit value).
        \param [in, out] difference - a pointer to pixels data of image with total difference.
        \param [in] differenceStride - a row size of difference image.
    */
    SIMD_API void SimdAddFeatureDifference(const uint8_t * value, size_t valueStride, size_t width, size_t height,
        const uint8_t * lo, size_t loStride, const uint8_t * hi, size_t hiStride,
        uint16_t weight, uint8_t * difference, size_t differenceStride);

    /*! @ingroup drawing

        \fn void SimdAlphaBlending(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, const uint8_t * alpha, size_t alphaStride, uint8_t * dst, size_t dstStride);

        \short Blends source image over destination image using per-pixel 8-bit alpha mask.

        All images must have the same width and height. Source and destination images must have the same format (8 bit per channel, for example GRAY8, UV16, BGR24 or BGRA32). Alpha must be 8-bit gray image.

        For every point and channel:
        \verbatim
        dst[x, y, c] = DivideBy255(src[x, y, c]*alpha[x, y] + dst[x, y, c]*(255 - alpha[x, y]));
        \endverbatim
        where DivideBy255(v) = (v + 1 + (v >> 8)) >> 8.

        This function is used for image drawing.

        \note This function has a C++ wrapper Simd::AlphaBlending(const View<A>& src, const View<A>& alpha, View<A>& dst).

        \param [in] src - a pointer to pixels data of foreground image.
        \param [in] srcStride - a row size of the foreground image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a channel count for foreground and background images (1 <= channelCount <= 4).
        \param [in] alpha - a pointer to pixels data of image with alpha channel.
        \param [in] alphaStride - a row size of the alpha image.
        \param [in, out] dst - a pointer to pixels data of background image.
        \param [in] dstStride - a row size of the background image.
    */
    SIMD_API void SimdAlphaBlending(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount,
        const uint8_t * alpha, size_t alphaStride, uint8_t * dst, size_t dstStride);

    /*! @ingroup drawing

        \fn void SimdAlphaBlending2x(const uint8_t* src0, size_t src0Stride, const uint8_t* alpha0, size_t alpha0Stride, const uint8_t* src1, size_t src1Stride, const uint8_t* alpha1, size_t alpha1Stride, size_t width, size_t height, size_t channelCount, uint8_t* dst, size_t dstStride);

        \short Performs two sequential alpha blendings of source images over destination image.

        All images must have the same width and height. Source and destination images must have the same format (8 bit per channel, for example GRAY8, UV16, BGR24 or BGRA32). Alphas must be 8-bit gray image.

        For every point and channel:
        \verbatim
        tmp = DivideBy255(src0[x, y, c]*alpha0[x, y] + dst[x, y, c]*(255 - alpha0[x, y]));
        dst[x, y, c] = DivideBy255(src1[x, y, c]*alpha1[x, y] + tmp*(255 - alpha1[x, y]));
        \endverbatim

        This function is used for image drawing.

        \note This function has a C++ wrapper Simd::AlphaBlending(const View<A>& src0, const View<A>& alpha0, const View<A>& src1, const View<A>& alpha1, View<A>& dst).

        \param [in] src0 - a pointer to pixels data of the first foreground image.
        \param [in] src0Stride - a row size of the first foreground image.
        \param [in] alpha0 - a pointer to pixels data of image with the first alpha channel.
        \param [in] alpha0Stride - a row size of the first alpha image.
        \param [in] src1 - a pointer to pixels data of the second foreground image.
        \param [in] src1Stride - a row size of the second foreground image.
        \param [in] alpha1 - a pointer to pixels data of image with the second alpha channel.
        \param [in] alpha1Stride - a row size of the second alpha image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a channel count for foreground and background images (1 <= channelCount <= 4).
        \param [in, out] dst - a pointer to pixels data of background image.
        \param [in] dstStride - a row size of the background image.
    */
    SIMD_API void SimdAlphaBlending2x(const uint8_t* src0, size_t src0Stride, const uint8_t* alpha0, size_t alpha0Stride, 
        const uint8_t* src1, size_t src1Stride, const uint8_t* alpha1, size_t alpha1Stride, 
        size_t width, size_t height, size_t channelCount, uint8_t* dst, size_t dstStride);

    /*! @ingroup drawing

        \fn void SimdAlphaBlendingBgraToYuv420p(const uint8_t* bgra, size_t bgraStride, size_t width, size_t height, uint8_t* y, size_t yStride, uint8_t* u, size_t uStride, uint8_t* v, size_t vStride, SimdYuvType yuvType);

        \short Converts BGRA to YUV420P and alpha-blends it with destination Y, U and V planes.

        For every BGRA pixel, Y is computed from BGR and blended with corresponding destination Y using this pixel alpha.
        For every 2x2 BGRA block, U and V are computed from averaged B, G, R values and blended with destination U and V
        using average alpha of this 2x2 block.
        The input BGRA and output Y images must have the same width and height.
        The output U and V images must have half width and half height relative to Y component.

        \note This function has a C++ wrapper Simd::AlphaBlendingBgraToYuv420p(const View<A>& bgra, View<A>& y, View<A>& u, View<A>& v, SimdYuvType yuvType = SimdYuvBt601).

        \param [in] bgra - a pointer to pixels data of foreground BGRA-32 image.
        \param [in] bgraStride - a row size of the foreground BGRA-32 image.
        \param [in] width - an image width. It must be even.
        \param [in] height - an image height. It must be even.
        \param [in, out] y - a pointer to pixels data of Y-component of background YUV420P image.
        \param [in] yStride - a row size of Y-component.
        \param [in, out] u - a pointer to pixels data of U-component of background YUV420P image.
        \param [in] uStride - a row size of U-component.
        \param [in, out] v - a pointer to pixels data of V-component of background YUV420P image.
        \param [in] vStride - a row size of V-component.
        \param [in] yuvType - a type of output YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdAlphaBlendingBgraToYuv420p(const uint8_t* bgra, size_t bgraStride, size_t width, size_t height, 
        uint8_t* y, size_t yStride, uint8_t* u, size_t uStride, uint8_t* v, size_t vStride, SimdYuvType yuvType);

    /*! @ingroup drawing

        \fn void SimdAlphaBlendingUniform(const uint8_t* src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t alpha, uint8_t* dst, size_t dstStride);

        \short Blends source image over destination image with the same alpha value for all pixels.

        All images must have the same width and height. Source and destination images must have the same format (8 bit per channel, for example GRAY8, UV16, BGR24 or BGRA32).

        For every point and channel:
        \verbatim
        dst[x, y, c] = DivideBy255(src[x, y, c]*alpha + dst[x, y, c]*(255 - alpha));
        \endverbatim

        This function is used for image drawing.

        \note This function has a C++ wrapper Simd::AlphaBlending(const View<A>& src, uint8_t alpha, View<A>& dst).

        \param [in] src - a pointer to pixels data of foreground image.
        \param [in] srcStride - a row size of the foreground image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a channel count for foreground and background images (1 <= channelCount <= 4).
        \param [in] alpha - a value of alpha.
        \param [in, out] dst - a pointer to pixels data of background image.
        \param [in] dstStride - a row size of the background image.
    */
    SIMD_API void SimdAlphaBlendingUniform(const uint8_t* src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t alpha, uint8_t* dst, size_t dstStride);

    /*! @ingroup drawing

        \fn void SimdAlphaFilling(uint8_t * dst, size_t dstStride, size_t width, size_t height, const uint8_t * channel, size_t channelCount, const uint8_t * alpha, size_t alphaStride);

        \short Blends constant pixel value into destination image using per-pixel 8-bit alpha mask.

        All images must have the same width and height. Destination images must have 8 bit per channel (for example GRAY8, BGR24 or BGRA32). Alpha must be 8-bit gray image.

        For every point and channel:
        \verbatim
        dst[x, y, c] = DivideBy255(channel[c]*alpha[x, y] + dst[x, y, c]*(255 - alpha[x, y]));
        \endverbatim

        This function is used for image drawing.

        \note This function has a C++ wrapper Simd::AlphaFilling(View<A> & dst, const Pixel & pixel, const View<A> & alpha).

        \param [in, out] dst - a pointer to pixels data of background image.
        \param [in] dstStride - a row size of the background image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channel - a pointer to pixel with foreground color.
        \param [in] channelCount - a channel count for foreground color and background images (1 <= channelCount <= 4).
        \param [in] alpha - a pointer to pixels data of image with alpha channel.
        \param [in] alphaStride - a row size of the alpha image.
    */
    SIMD_API void SimdAlphaFilling(uint8_t * dst, size_t dstStride, size_t width, size_t height, const uint8_t * channel, size_t channelCount, const uint8_t * alpha, size_t alphaStride);

    /*! @ingroup drawing

        \fn void SimdAlphaPremultiply(const uint8_t* src, size_t srcStride, size_t width, size_t height, uint8_t* dst, size_t dstStride, SimdBool argb);

        \short Converts straight-alpha 4-channel image to premultiplied-alpha representation.

        All images must have the same width, height and format (BGRA32, RGBA32, ARGB32).

        For every point:
        \verbatim
         color = DivideBy255(color * alpha);
         alpha is copied unchanged.
        \endverbatim
        If argb == SimdFalse then alpha channel index is 3 (BGRA32/RGBA32 layout).
        If argb == SimdTrue then alpha channel index is 0 (ARGB32 layout).

        This function is used for image drawing as a part of alpha blending operation.

        \note This function has a C++ wrapper Simd::AlphaPremultiply(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of input image.
        \param [in] srcStride - a row size of the input image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of output premultiplied image.
        \param [in] dstStride - a row size of the output premultiplied image.
        \param [in] argb - a boolean flag describing image format (BGRA32, RGBA32 - SimdFalse; ARGB32 - SimdTrue).
    */
    SIMD_API void SimdAlphaPremultiply(const uint8_t* src, size_t srcStride, size_t width, size_t height, uint8_t* dst, size_t dstStride, SimdBool argb);

    /*! @ingroup drawing

        \fn void SimdAlphaUnpremultiply(const uint8_t* src, size_t srcStride, size_t width, size_t height, uint8_t* dst, size_t dstStride, SimdBool argb);

        \short Converts premultiplied-alpha 4-channel image to straight-alpha representation.

        All images must have the same width, height and format (BGRA32, RGBA32, ARGB32).

        For every point:
        \verbatim
         color = clamp(int(color * (alpha ? 255.00001f/alpha : 0.0f)), 0, 255);
         alpha is copied unchanged.
        \endverbatim
        If argb == SimdFalse then alpha channel index is 3 (BGRA32/RGBA32 layout).
        If argb == SimdTrue then alpha channel index is 0 (ARGB32 layout).

        This function is used for image drawing as a part of alpha blending operation.

        \note This function has a C++ wrapper Simd::AlphaUnpremultiply(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of input image.
        \param [in] srcStride - a row size of the input image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of output unpremultiplied image.
        \param [in] dstStride - a row size of the output unpremultiplied image.
        \param [in] argb - a boolean flag describing image format (BGRA32, RGBA32 - SimdFalse; ARGB32 - SimdTrue).
    */
    SIMD_API void SimdAlphaUnpremultiply(const uint8_t* src, size_t srcStride, size_t width, size_t height, uint8_t* dst, size_t dstStride, SimdBool argb);

    /*! @ingroup background

        \fn void SimdBackgroundGrowRangeSlow(const uint8_t * value, size_t valueStride, size_t width, size_t height, uint8_t * lo, size_t loStride, uint8_t * hi, size_t hiStride);

        \short Performs slow expansion of background range.

        All images must have the same width, height and format (8-bit gray).

        For every point, range bounds are moved by one step toward current value:
        \verbatim
        lo[i] -= value[i] < lo[i] ? 1 : 0;
        hi[i] += value[i] > hi[i] ? 1 : 0;
        \endverbatim

        This function is used for background updating in motion detection algorithm.

        \note This function has a C++ wrapper Simd::BackgroundGrowRangeSlow(const View<A>& value, View<A>& lo, View<A>& hi).

        \param [in] value - a pointer to pixels data of current feature value.
        \param [in] valueStride - a row size of the value image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in, out] lo - a pointer to pixels data of feature lower bound of dynamic background.
        \param [in] loStride - a row size of the lo image.
        \param [in, out] hi - a pointer to pixels data of feature upper bound of dynamic background.
        \param [in] hiStride - a row size of the hi image.
    */
    SIMD_API void SimdBackgroundGrowRangeSlow(const uint8_t * value, size_t valueStride, size_t width, size_t height,
        uint8_t * lo, size_t loStride, uint8_t * hi, size_t hiStride);

    /*! @ingroup background

        \fn void SimdBackgroundGrowRangeFast(const uint8_t * value, size_t valueStride, size_t width, size_t height, uint8_t * lo, size_t loStride, uint8_t * hi, size_t hiStride);

        \short Performs fast expansion of background range.

        All images must have the same width, height and format (8-bit gray).

        For every point, range bounds are expanded to include current value:
        \verbatim
        lo[i] = value[i] < lo[i] ? value[i] : lo[i];
        hi[i] = value[i] > hi[i] ? value[i] : hi[i];
        \endverbatim

        This function is used for background updating in motion detection algorithm.

        \note This function has a C++ wrapper Simd::BackgroundGrowRangeFast(const View<A>& value, View<A>& lo, View<A>& hi).

        \param [in] value - a pointer to pixels data of current feature value.
        \param [in] valueStride - a row size of the value image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in, out] lo - a pointer to pixels data of feature lower bound of dynamic background.
        \param [in] loStride - a row size of the lo image.
        \param [in, out] hi - a pointer to pixels data of feature upper bound of dynamic background.
        \param [in] hiStride - a row size of the hi image.
    */
    SIMD_API void SimdBackgroundGrowRangeFast(const uint8_t * value, size_t valueStride, size_t width, size_t height,
        uint8_t * lo, size_t loStride, uint8_t * hi, size_t hiStride);

    /*! @ingroup background

        \fn void SimdBackgroundIncrementCount(const uint8_t * value, size_t valueStride, size_t width, size_t height, const uint8_t * loValue, size_t loValueStride, const uint8_t * hiValue, size_t hiValueStride, uint8_t * loCount, size_t loCountStride, uint8_t * hiCount, size_t hiCountStride);

        \short Collects background out-of-range statistics.

        All images must have the same width, height and format (8-bit gray).

        For every point, counters are incremented with saturation to 255:
        \verbatim
        loCount[i] += (value[i] < loValue[i] && loCount[i] < 255) ? 1 : 0;
        hiCount[i] += (value[i] > hiValue[i] && hiCount[i] < 255) ? 1 : 0;
        \endverbatim

        This function is used for background updating in motion detection algorithm.

        \note This function has a C++ wrapper Simd::BackgroundIncrementCount(const View<A>& value, const View<A>& loValue, const View<A>& hiValue, View<A>& loCount, View<A>& hiCount).

        \param [in] value - a pointer to pixels data of current feature value.
        \param [in] valueStride - a row size of the value image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] loValue - a pointer to pixels data of value of feature lower bound of dynamic background.
        \param [in] loValueStride - a row size of the loValue image.
        \param [in] hiValue - a pointer to pixels data of value of feature upper bound of dynamic background.
        \param [in] hiValueStride - a row size of the hiValue image.
        \param [in, out] loCount - a pointer to pixels data of count of feature lower bound of dynamic background.
        \param [in] loCountStride - a row size of the loCount image.
        \param [in, out] hiCount - a pointer to pixels data of count of feature upper bound of dynamic background.
        \param [in] hiCountStride - a row size of the hiCount image.
    */
    SIMD_API void SimdBackgroundIncrementCount(const uint8_t * value, size_t valueStride, size_t width, size_t height,
        const uint8_t * loValue, size_t loValueStride, const uint8_t * hiValue, size_t hiValueStride,
        uint8_t * loCount, size_t loCountStride, uint8_t * hiCount, size_t hiCountStride);

    /*! @ingroup background

        \fn void SimdBackgroundAdjustRange(uint8_t * loCount, size_t loCountStride, size_t width, size_t height, uint8_t * loValue, size_t loValueStride, uint8_t * hiCount, size_t hiCountStride, uint8_t * hiValue, size_t hiValueStride, uint8_t threshold);

        \short Adjusts background range using collected counters.

        All images must have the same width, height and format (8-bit gray).

        For every point:
        \verbatim
        loValue[i] -= (loCount[i] > threshold && loValue[i] > 0) ? 1 : 0;
        loValue[i] += (loCount[i] < threshold && loValue[i] < 255) ? 1 : 0;
        loCount[i] = 0;
        hiValue[i] += (hiCount[i] > threshold && hiValue[i] < 255) ? 1 : 0;
        hiValue[i] -= (hiCount[i] < threshold && hiValue[i] > 0) ? 1 : 0;
        hiCount[i] = 0;
        \endverbatim

        This function is used for background updating in motion detection algorithm.

        \note This function has a C++ wrapper Simd::BackgroundAdjustRange(View<A>& loCount, View<A>& loValue, View<A>& hiCount, View<A>& hiValue, uint8_t threshold).

        \param [in, out] loCount - a pointer to pixels data of count of feature lower bound of dynamic background.
        \param [in] loCountStride - a row size of the loCount image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in, out] hiCount - a pointer to pixels data of count of feature upper bound of dynamic background.
        \param [in] hiCountStride - a row size of the hiCount image.
        \param [in, out] loValue - a pointer to pixels data of value of feature lower bound of dynamic background.
        \param [in] loValueStride - a row size of the loValue image.
        \param [in, out] hiValue - a pointer to pixels data of value of feature upper bound of dynamic background.
        \param [in] hiValueStride - a row size of the hiValue image.
        \param [in] threshold - a count threshold.
    */
    SIMD_API void SimdBackgroundAdjustRange(uint8_t * loCount, size_t loCountStride, size_t width, size_t height,
        uint8_t * loValue, size_t loValueStride, uint8_t * hiCount, size_t hiCountStride,
        uint8_t * hiValue, size_t hiValueStride, uint8_t threshold);

    /*! @ingroup background

        \fn void SimdBackgroundAdjustRangeMasked(uint8_t * loCount, size_t loCountStride, size_t width, size_t height, uint8_t * loValue, size_t loValueStride, uint8_t * hiCount, size_t hiCountStride, uint8_t * hiValue, size_t hiValueStride, uint8_t threshold, const uint8_t * mask, size_t maskStride);

        \short Adjusts background range using collected counters and a mask.

        All images must have the same width, height and format (8-bit gray).

        For every point:
        \verbatim
        if(mask[i])
        {
            loValue[i] -= (loCount[i] > threshold && loValue[i] > 0) ? 1 : 0;
            loValue[i] += (loCount[i] < threshold && loValue[i] < 255) ? 1 : 0;
            loCount[i] = 0;
            hiValue[i] += (hiCount[i] > threshold && hiValue[i] < 255) ? 1 : 0;
            hiValue[i] -= (hiCount[i] < threshold && hiValue[i] > 0) ? 1 : 0;
            hiCount[i] = 0;
        }
        \endverbatim

        This function is used for background updating in motion detection algorithm.

        \note This function has a C++ wrapper Simd::BackgroundAdjustRange(View<A>& loCount, View<A>& loValue, View<A>& hiCount, View<A>& hiValue, uint8_t threshold, const View<A>& mask).

        \param [in, out] loCount - a pointer to pixels data of count of feature lower bound of dynamic background.
        \param [in] loCountStride - a row size of the loCount image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in, out] hiCount - a pointer to pixels data of count of feature upper bound of dynamic background.
        \param [in] hiCountStride - a row size of the hiCount image.
        \param [in, out] loValue - a pointer to pixels data of value of feature lower bound of dynamic background.
        \param [in] loValueStride - a row size of the loValue image.
        \param [in, out] hiValue - a pointer to pixels data of value of feature upper bound of dynamic background.
        \param [in] hiValueStride - a row size of the hiValue image.
        \param [in] threshold - a count threshold.
        \param [in] mask - a pointer to pixels data of adjust range mask.
        \param [in] maskStride - a row size of the mask image.
    */
    SIMD_API void SimdBackgroundAdjustRangeMasked(uint8_t * loCount, size_t loCountStride, size_t width, size_t height,
        uint8_t * loValue, size_t loValueStride, uint8_t * hiCount, size_t hiCountStride,
        uint8_t * hiValue, size_t hiValueStride, uint8_t threshold, const uint8_t * mask, size_t maskStride);

    /*! @ingroup background

        \fn void SimdBackgroundShiftRange(const uint8_t * value, size_t valueStride, size_t width, size_t height, uint8_t * lo, size_t loStride, uint8_t * hi, size_t hiStride);

        \short Shifts background range to include current value.

        All images must have the same width, height and format (8-bit gray).

        For every point:
        \verbatim
        add = value[i] - hi[i];
        sub = lo[i] - value[i];
        if(add > 0)
        {
            lo[i] = min(lo[i] + add, 255);
            hi[i] = min(hi[i] + add, 255);
        }
        if(sub > 0)
        {
            lo[i] = max(lo[i] - sub, 0);
            hi[i] = max(hi[i] - sub, 0);
        }
        \endverbatim

        This function is used for fast background updating in motion detection algorithm.

        \note This function has a C++ wrapper Simd::BackgroundShiftRange(const View<A>& value, View<A>& lo, View<A>& hi).

        \param [in] value - a pointer to pixels data of current feature value.
        \param [in] valueStride - a row size of the value image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in, out] lo - a pointer to pixels data of feature lower bound of dynamic background.
        \param [in] loStride - a row size of the lo image.
        \param [in, out] hi - a pointer to pixels data of feature upper bound of dynamic background.
        \param [in] hiStride - a row size of the hi image.
    */
    SIMD_API void SimdBackgroundShiftRange(const uint8_t * value, size_t valueStride, size_t width, size_t height,
        uint8_t * lo, size_t loStride, uint8_t * hi, size_t hiStride);

    /*! @ingroup background

        \fn void SimdBackgroundShiftRangeMasked(const uint8_t * value, size_t valueStride, size_t width, size_t height, uint8_t * lo, size_t loStride, uint8_t * hi, size_t hiStride, const uint8_t * mask, size_t maskStride);

        \short Shifts background range to include current value using a mask.

        All images must have the same width, height and format (8-bit gray).

        For every point:
        \verbatim
        if(mask[i])
        {
            add = value[i] - hi[i];
            sub = lo[i] - value[i];
            if(add > 0)
            {
                lo[i] = min(lo[i] + add, 255);
                hi[i] = min(hi[i] + add, 255);
            }
            if(sub > 0)
            {
                lo[i] = max(lo[i] - sub, 0);
                hi[i] = max(hi[i] - sub, 0);
            }
        }
        \endverbatim

        This function is used for fast background updating in motion detection algorithm.

        \note This function has a C++ wrapper Simd::BackgroundShiftRange(const View<A>& value, View<A>& lo, View<A>& hi, const View<A>& mask).

        \param [in] value - a pointer to pixels data of current feature value.
        \param [in] valueStride - a row size of the value image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in, out] lo - a pointer to pixels data of feature lower bound of dynamic background.
        \param [in] loStride - a row size of the lo image.
        \param [in, out] hi - a pointer to pixels data of feature upper bound of dynamic background.
        \param [in] hiStride - a row size of the hi image.
        \param [in] mask - a pointer to pixels data of shift range mask.
        \param [in] maskStride - a row size of the mask image.
    */
    SIMD_API void SimdBackgroundShiftRangeMasked(const uint8_t * value, size_t valueStride, size_t width, size_t height,
        uint8_t * lo, size_t loStride, uint8_t * hi, size_t hiStride, const uint8_t * mask, size_t maskStride);

    /*! @ingroup background

        \fn void SimdBackgroundInitMask(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t index, uint8_t value, uint8_t * dst, size_t dstStride);

        \short Initializes background update mask by selected source index.

        All images must have the same width, height and format (8-bit gray).

        For every point:
        \verbatim
        if(src[i] == index)
            dst[i] = value;
        \endverbatim

        This function is used for background updating in motion detection algorithm.

        \note This function has a C++ wrapper Simd::BackgroundInitMask(const View<A>& src, uint8_t index, uint8_t value, View<A>& dst).

        \param [in] src - a pointer to pixels data of input mask image.
        \param [in] srcStride - a row size of input mask image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] index - a mask index into input mask.
        \param [in] value - a value to fill the output mask.
        \param [out] dst - a pointer to pixels data of output mask image.
        \param [in] dstStride - a row size of output mask image.
    */
    SIMD_API void SimdBackgroundInitMask(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        uint8_t index, uint8_t value, uint8_t * dst, size_t dstStride);

    /*! @ingroup base64

        \fn void SimdBase64Decode(const uint8_t* src, size_t srcSize, uint8_t* dst, size_t* dstSize);

        \short Decodes a Base64-encoded byte sequence into its original binary data.

        The function decodes a Base64-encoded input (as defined by RFC 4648) into the
        original binary data. The input length must be a multiple of 4 and at least 4 bytes.
        Padding characters ('=') at the end of the input are handled automatically, so the
        actual decoded length may be 1 or 2 bytes less than srcSize / 4 * 3.

        \note This function has a C++ wrapper std::string Simd::Base64Decode(const std::string & src).

        \param [in] src - a pointer to the Base64-encoded input data. Its length must be a multiple of 4 and at least 4.
        \param [in] srcSize - a size (in bytes) of the Base64-encoded input. Must be a non-zero multiple of 4.
        \param [out] dst - a pointer to the output buffer for the decoded binary data. The buffer size must be at least srcSize / 4 * 3 bytes.
        \param [out] dstSize - a pointer to a variable that receives the number of bytes written to the output buffer.
    */
    SIMD_API void SimdBase64Decode(const uint8_t* src, size_t srcSize, uint8_t* dst, size_t* dstSize);

    /*! @ingroup base64

        \fn void SimdBase64Encode(const uint8_t* src, size_t size, uint8_t* dst);

        \short Encodes binary data into a Base64 string.

        The function encodes arbitrary binary data into its Base64 representation (as defined
        by RFC 4648). Every 3 input bytes are encoded as 4 Base64 characters. If the input
        length is not a multiple of 3, the output is padded with '=' characters to the next
        multiple of 4. The output is NOT null-terminated; its length is exactly (size + 2) / 3 * 4 bytes.

        \note This function has a C++ wrapper std::string Simd::Base64Encode(const std::string & src).

        \param [in] src - a pointer to the input binary data to be encoded.
        \param [in] size - a size (in bytes) of the input data.
        \param [out] dst - a pointer to the output buffer for the Base64-encoded string. The buffer size must be at least (size + 2) / 3 * 4 bytes.
    */
    SIMD_API void SimdBase64Encode(const uint8_t* src, size_t size, uint8_t* dst);

    /*! @ingroup bayer_conversion

        \fn void SimdBayerToBgr(const uint8_t * bayer, size_t width, size_t height, size_t bayerStride, SimdPixelFormatType bayerFormat, uint8_t * bgr, size_t bgrStride);

        \short Converts an 8-bit Bayer image to a 24-bit BGR image using edge-directed demosaicing.

        The function performs demosaicing of a raw Bayer-patterned image into a full-color 24-bit BGR image.
        Missing color samples at each pixel are reconstructed using a gradient-based interpolation that
        selects between vertical and horizontal neighbors according to local edge strength, producing
        sharper results along edges than simple bilinear interpolation.
        Both images must have the same width and height, and both dimensions must be even.

        \note This function has a C++ wrapper Simd::BayerToBgr(const View<A>& bayer, View<A>& bgr).

        \param [in] bayer - a pointer to pixels data of input 8-bit Bayer image.
        \param [in] width - an image width. Must be even and at least 4.
        \param [in] height - an image height. Must be even and at least 4.
        \param [in] bayerStride - a row size (in bytes) of the bayer image.
        \param [in] bayerFormat - a format of the input bayer image. It can be ::SimdPixelFormatBayerGrbg, ::SimdPixelFormatBayerGbrg, ::SimdPixelFormatBayerRggb or ::SimdPixelFormatBayerBggr.
        \param [out] bgr - a pointer to pixels data of output 24-bit BGR image.
        \param [in] bgrStride - a row size (in bytes) of the bgr image.
    */
    SIMD_API void SimdBayerToBgr(const uint8_t * bayer, size_t width, size_t height, size_t bayerStride, SimdPixelFormatType bayerFormat, uint8_t * bgr, size_t bgrStride);

    /*! @ingroup bayer_conversion

        \fn void SimdBayerToBgra(const uint8_t * bayer, size_t width, size_t height, size_t bayerStride, SimdPixelFormatType bayerFormat, uint8_t * bgra, size_t bgraStride, uint8_t alpha);

        \short Converts an 8-bit Bayer image to a 32-bit BGRA image using edge-directed demosaicing.

        The function performs demosaicing of a raw Bayer-patterned image into a full-color 32-bit BGRA image.
        Missing color samples at each pixel are reconstructed using a gradient-based interpolation that
        selects between vertical and horizontal neighbors according to local edge strength. The alpha channel
        of every output pixel is set to the constant value specified by the \a alpha parameter.
        Both images must have the same width and height, and both dimensions must be even.

        \note This function has a C++ wrapper Simd::BayerToBgra(const View<A>& bayer, View<A>& bgra, uint8_t alpha).

        \param [in] bayer - a pointer to pixels data of input 8-bit Bayer image.
        \param [in] width - an image width. Must be even and at least 4.
        \param [in] height - an image height. Must be even and at least 4.
        \param [in] bayerStride - a row size (in bytes) of the bayer image.
        \param [in] bayerFormat - a format of the input bayer image. It can be ::SimdPixelFormatBayerGrbg, ::SimdPixelFormatBayerGbrg, ::SimdPixelFormatBayerRggb or ::SimdPixelFormatBayerBggr.
        \param [out] bgra - a pointer to pixels data of output 32-bit BGRA image.
        \param [in] bgraStride - a row size (in bytes) of the bgra image.
        \param [in] alpha - a constant value to fill the alpha channel of every output pixel.
    */
    SIMD_API void SimdBayerToBgra(const uint8_t * bayer, size_t width, size_t height, size_t bayerStride, SimdPixelFormatType bayerFormat, uint8_t * bgra, size_t bgraStride, uint8_t alpha);

    /*! @ingroup bgra_conversion

        \fn void SimdBgraToBayer(const uint8_t * bgra, size_t width, size_t height, size_t bgraStride, uint8_t * bayer, size_t bayerStride, SimdPixelFormatType bayerFormat);

        \short Converts a 32-bit BGRA image to an 8-bit Bayer image by sub-sampling color channels.

        The function down-samples a full-color 32-bit BGRA image to an 8-bit Bayer-patterned image.
        For each 2x2 block of BGRA pixels, exactly one color channel value (Blue, Green, or Red) is
        selected per output pixel according to the specified Bayer pattern. The alpha channel of the
        input image is ignored. Both images must have the same width and height, and both dimensions
        must be even.

        \note This function has a C++ wrapper Simd::BgraToBayer(const View<A>& bgra, View<A>& bayer).

        \param [in] bgra - a pointer to pixels data of input 32-bit BGRA image.
        \param [in] width - an image width. Must be even.
        \param [in] height - an image height. Must be even.
        \param [in] bgraStride - a row size (in bytes) of the bgra image.
        \param [out] bayer - a pointer to pixels data of output 8-bit Bayer image.
        \param [in] bayerStride - a row size (in bytes) of the bayer image.
        \param [in] bayerFormat - a format of the output bayer image. It can be ::SimdPixelFormatBayerGrbg, ::SimdPixelFormatBayerGbrg, ::SimdPixelFormatBayerRggb or ::SimdPixelFormatBayerBggr.
    */
    SIMD_API void SimdBgraToBayer(const uint8_t * bgra, size_t width, size_t height, size_t bgraStride, uint8_t * bayer, size_t bayerStride, SimdPixelFormatType bayerFormat);

    /*! @ingroup bgra_conversion

        \fn void SimdBgraToBgr(const uint8_t * bgra, size_t width, size_t height, size_t bgraStride, uint8_t * bgr, size_t bgrStride);

        \short Converts a 32-bit BGRA image to a 24-bit BGR image by dropping the alpha channel.

        The function converts a 32-bit BGRA (Blue, Green, Red, Alpha) image to a 24-bit BGR (Blue, Green, Red) image.
        The Blue, Green, and Red channels are copied unchanged for each pixel; the alpha channel is discarded.
        This function can also be used for 32-bit RGBA to 24-bit RGB conversion, since the channel layout
        operation (dropping the 4th channel) is identical. Both images must have the same width and height.

        \note This function has C++ wrappers: Simd::BgraToBgr(const View<A>& bgra, View<A>& bgr)
            and Simd::RgbaToRgb(const View<A>& rgba, View<A>& rgb).

        \param [in] bgra - a pointer to pixels data of input 32-bit BGRA (or 32-bit RGBA) image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] bgraStride - a row size (in bytes) of the bgra image.
        \param [out] bgr - a pointer to pixels data of output 24-bit BGR (or 24-bit RGB) image.
        \param [in] bgrStride - a row size (in bytes) of the bgr image.
    */
    SIMD_API void SimdBgraToBgr(const uint8_t * bgra, size_t width, size_t height, size_t bgraStride, uint8_t * bgr, size_t bgrStride);

    /*! @ingroup bgra_conversion

        \fn void SimdBgraToGray(const uint8_t * bgra, size_t width, size_t height, size_t bgraStride, uint8_t * gray, size_t grayStride);

        \short Converts a 32-bit BGRA image to an 8-bit grayscale image.

        The function converts a 32-bit BGRA (Blue, Green, Red, Alpha) image to an 8-bit grayscale image.
        The alpha channel is ignored. The luminance value of each pixel is calculated from the Blue, Green,
        and Red channels using the ITU-R BT.601 standard weighted sum:
        \verbatim
        gray = (0.114 * blue + 0.587 * green + 0.299 * red)
        \endverbatim
        Both images must have the same width and height.

        \note This function has a C++ wrapper Simd::BgraToGray(const View<A>& bgra, View<A>& gray).

        \param [in] bgra - a pointer to pixels data of input 32-bit BGRA image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] bgraStride - a row size (in bytes) of the bgra image.
        \param [out] gray - a pointer to pixels data of output 8-bit gray image.
        \param [in] grayStride - a row size (in bytes) of the gray image.
    */
    SIMD_API void SimdBgraToGray(const uint8_t * bgra, size_t width, size_t height, size_t bgraStride, uint8_t * gray, size_t grayStride);

    /*! @ingroup bgra_conversion

        \fn void SimdBgraToRgb(const uint8_t * bgra, size_t width, size_t height, size_t bgraStride, uint8_t * rgb, size_t rgbStride);

        \short Converts a 32-bit BGRA image to a 24-bit RGB image by swapping the Red and Blue channels and dropping the alpha channel.

        The function converts a 32-bit BGRA (Blue, Green, Red, Alpha) image to a 24-bit RGB (Red, Green, Blue) image.
        The Blue and Red channels are swapped, the Green channel is copied unchanged, and the alpha channel is discarded.
        This function can also be used for 32-bit RGBA to 24-bit BGR conversion, since the channel reordering
        operation is identical. Both images must have the same width and height.

        \note This function has C++ wrappers: Simd::BgraToRgb(const View<A>& bgra, View<A>& rgb)
            and Simd::RgbaToBgr(const View<A>& rgba, View<A>& bgr).

        \param [in] bgra - a pointer to pixels data of input 32-bit BGRA (or 32-bit RGBA) image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] bgraStride - a row size (in bytes) of the bgra image.
        \param [out] rgb - a pointer to pixels data of output 24-bit RGB (or 24-bit BGR) image.
        \param [in] rgbStride - a row size (in bytes) of the rgb image.
    */
    SIMD_API void SimdBgraToRgb(const uint8_t* bgra, size_t width, size_t height, size_t bgraStride, uint8_t* rgb, size_t rgbStride);

    /*! @ingroup bgra_conversion

        \fn void SimdBgraToRgba(const uint8_t * bgra, size_t width, size_t height, size_t bgraStride, uint8_t * rgba, size_t rgbaStride);

        \short Converts a 32-bit BGRA image to a 32-bit RGBA image by swapping the Red and Blue channels while preserving the alpha channel.

        The function converts a 32-bit BGRA (Blue, Green, Red, Alpha) image to a 32-bit RGBA (Red, Green, Blue, Alpha) image.
        The Blue and Red channels are swapped, while the Green and Alpha channels are copied unchanged for each pixel.
        This function can also be used for 32-bit RGBA to 32-bit BGRA conversion, since the transformation is its own inverse.
        Both images must have the same width and height.

        \note This function has C++ wrappers: Simd::BgraToRgba(const View<A>& bgra, View<A>& rgba)
            and Simd::RgbaToBgra(const View<A>& rgba, View<A>& bgra).

        \param [in] bgra - a pointer to pixels data of input 32-bit BGRA (or 32-bit RGBA) image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] bgraStride - a row size (in bytes) of the bgra image.
        \param [out] rgba - a pointer to pixels data of output 32-bit RGBA (or 32-bit BGRA) image.
        \param [in] rgbaStride - a row size (in bytes) of the rgba image.
    */
    SIMD_API void SimdBgraToRgba(const uint8_t* bgra, size_t width, size_t height, size_t bgraStride, uint8_t* rgba, size_t rgbaStride);

    /*! @ingroup bgra_conversion

        \fn void SimdBgraToYuv420pV2(const uint8_t * bgra, size_t bgraStride, size_t width, size_t height, uint8_t * y, size_t yStride, uint8_t * u, size_t uStride, uint8_t * v, size_t vStride, SimdYuvType yuvType);

        \short Converts a 32-bit BGRA image to planar YUV420P (4:2:0) and ignores input alpha.

        Y is computed for every source pixel from its B, G, R values.
        U and V are computed for every 2x2 block from averaged B, G, R values of this block.
        The input BGRA and output Y images must have the same width and height.
        The output U and V images must have half width and half height relative to Y.
        The width and the height must be even.

        \note This function has a C++ wrapper Simd::BgraToYuv420p(const View<A>& bgra, View<A>& y, View<A>& u, View<A>& v, SimdYuvType yuvType = SimdYuvBt601).

        \param [in] bgra - a pointer to pixels data of input 32-bit BGRA image.
        \param [in] bgraStride - a row size of the BGRA image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] y - a pointer to pixels data of output 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [out] u - a pointer to pixels data of output 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [out] v - a pointer to pixels data of output 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] yuvType - a type of output YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdBgraToYuv420pV2(const uint8_t* bgra, size_t bgraStride, size_t width, size_t height, uint8_t* y, size_t yStride, uint8_t* u, size_t uStride, uint8_t* v, size_t vStride, SimdYuvType yuvType);

    /*! @ingroup bgra_conversion

        \fn void SimdBgraToYuv422pV2(const uint8_t * bgra, size_t bgraStride, size_t width, size_t height, uint8_t * y, size_t yStride, uint8_t * u, size_t uStride, uint8_t * v, size_t vStride, SimdYuvType yuvType);

        \short Converts a 32-bit BGRA image to planar YUV422P (4:2:2) and ignores input alpha.

        Y is computed for every source pixel from its B, G, R values.
        U and V are computed for each horizontal pair of pixels from averaged B, G, R values of this pair.
        The input BGRA and output Y images must have the same width and height.
        The output U and V images must have half width and the same height relative to Y.
        The width must be even.

        \note This function has a C++ wrapper Simd::BgraToYuv422p(const View<A>& bgra, View<A>& y, View<A>& u, View<A>& v, SimdYuvType yuvType = SimdYuvBt601).

        \param [in] bgra - a pointer to pixels data of input 32-bit BGRA image.
        \param [in] bgraStride - a row size of the BGRA image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] y - a pointer to pixels data of output 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [out] u - a pointer to pixels data of output 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [out] v - a pointer to pixels data of output 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] yuvType - a type of output YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdBgraToYuv422pV2(const uint8_t* bgra, size_t bgraStride, size_t width, size_t height, uint8_t* y, size_t yStride, uint8_t* u, size_t uStride, uint8_t* v, size_t vStride, SimdYuvType yuvType);

    /*! @ingroup bgra_conversion

        \fn void SimdBgraToYuv444pV2(const uint8_t* bgra, size_t bgraStride, size_t width, size_t height, uint8_t* y, size_t yStride, uint8_t* u, size_t uStride, uint8_t* v, size_t vStride, SimdYuvType yuvType);

        \short Converts a 32-bit BGRA image to planar YUV444P (4:4:4) and ignores input alpha.

        Y, U and V are computed for every source pixel from its B, G, R values.
        The input BGRA and output Y, U and V images must have the same width and height.

        \note This function has a C++ wrapper Simd::BgraToYuv444p(const View<A>& bgra, View<A>& y, View<A>& u, View<A>& v, SimdYuvType yuvType = SimdYuvBt601).

        \param [in] bgra - a pointer to pixels data of input 32-bit BGRA image.
        \param [in] bgraStride - a row size of the BGRA image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] y - a pointer to pixels data of output 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [out] u - a pointer to pixels data of output 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [out] v - a pointer to pixels data of output 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] yuvType - a type of output YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdBgraToYuv444pV2(const uint8_t* bgra, size_t bgraStride, size_t width, size_t height, 
        uint8_t* y, size_t yStride, uint8_t* u, size_t uStride, uint8_t* v, size_t vStride, SimdYuvType yuvType);

    /*! @ingroup bgra_conversion

        \fn void SimdBgraToYuva420pV2(const uint8_t * bgra, size_t bgraStride, size_t width, size_t height, uint8_t * y, size_t yStride, uint8_t * u, size_t uStride, uint8_t * v, size_t vStride, uint8_t * a, size_t aStride, SimdYuvType yuvType);

        \short Converts a 32-bit BGRA image to planar YUVA420P (YUV 4:2:0 plus full-size alpha plane).

        Y is computed for every source pixel from its B, G, R values.
        U and V are computed for every 2x2 block from averaged B, G, R values of this block.
        A is copied from the source alpha channel for every pixel without changes.
        The input BGRA and output Y and A images must have the same width and height.
        The output U and V images must have half width and half height relative to Y.
        The width and the height must be even.

        \note This function has a C++ wrapper Simd::BgraToYuva420p(const View<A> & bgra, View<A> & y, View<A> & u, View<A> & v, View<A> & a, SimdYuvType yuvType = SimdYuvBt601).

        \param [in] bgra - a pointer to pixels data of input 32-bit BGRA image.
        \param [in] bgraStride - a row size of the BGRA image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] y - a pointer to pixels data of output 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [out] u - a pointer to pixels data of output 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [out] v - a pointer to pixels data of output 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [out] a - a pointer to pixels data of output 8-bit image with alpha plane.
        \param [in] aStride - a row size of the a image.
        \param [in] yuvType - a type of output YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdBgraToYuva420pV2(const uint8_t* bgra, size_t bgraStride, size_t width, size_t height,
        uint8_t* y, size_t yStride, uint8_t* u, size_t uStride, uint8_t* v, size_t vStride, uint8_t* a, size_t aStride, SimdYuvType yuvType);

    /*! @ingroup bgr_conversion

        \fn void SimdBgrToBayer(const uint8_t * bgr, size_t width, size_t height, size_t bgrStride, uint8_t * bayer, size_t bayerStride, SimdPixelFormatType bayerFormat);

        \short Converts a 24-bit BGR image to a single-channel 8-bit Bayer mosaic image.

        For each output pixel only one source color component (B, G or R) is selected according to bayerFormat
        and its position inside a 2x2 Bayer pattern tile.
        Input and output images must have the same width and height.
        The width and the height must be even.

        \note This function has a C++ wrapper Simd::BgrToBayer(const View<A>& bgr, View<A>& bayer).

        \param [in] bgr - a pointer to pixels data of input 24-bit BGR image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] bgrStride - a row size of the bgr image.
        \param [out] bayer - a pointer to pixels data of output 8-bit Bayer image.
        \param [in] bayerStride - a row size of the bayer image.
        \param [in] bayerFormat - a format of the output bayer image. It can be ::SimdPixelFormatBayerGrbg, ::SimdPixelFormatBayerGbrg, ::SimdPixelFormatBayerRggb or ::SimdPixelFormatBayerBggr.
    */
    SIMD_API void SimdBgrToBayer(const uint8_t * bgr, size_t width, size_t height, size_t bgrStride, uint8_t * bayer, size_t bayerStride, SimdPixelFormatType bayerFormat);

    /*! @ingroup bgr_conversion

        \fn void SimdBgrToBgra(const uint8_t * bgr, size_t width, size_t height, size_t bgrStride, uint8_t * bgra, size_t bgraStride, uint8_t alpha);

        \short Converts a 24-bit BGR image to a 32-bit BGRA image with constant alpha.

        For each pixel, B, G and R are copied unchanged and A is set to the \a alpha parameter.
        Input and output images must have the same width and height.

        \note This function has a C++ wrapper Simd::BgrToBgra(const View<A>& bgr, View<A>& bgra, uint8_t alpha).

        \param [in] bgr - a pointer to pixels data of input 24-bit BGR image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] bgrStride - a row size of the bgr image.
        \param [out] bgra - a pointer to pixels data of output 32-bit BGRA image.
        \param [in] bgraStride - a row size of the bgra image.
        \param [in] alpha - a value of alpha channel.
    */
    SIMD_API void SimdBgrToBgra(const uint8_t * bgr, size_t width, size_t height, size_t bgrStride, uint8_t * bgra, size_t bgraStride, uint8_t alpha);

    /*! @ingroup other_conversion

        \fn void SimdBgr48pToBgra32(const uint8_t * blue, size_t blueStride, size_t width, size_t height, const uint8_t * green, size_t greenStride, const uint8_t * red, size_t redStride, uint8_t * bgra, size_t bgraStride, uint8_t alpha);

        \short Converts a planar 48-bit BGR image (three 16-bit planes) to 32-bit BGRA.

        For each pixel, one 8-bit value is taken from every 16-bit source channel
        (low byte on little-endian systems, high byte on big-endian systems), and alpha is set to \a alpha.
        All input and output images must have the same width and height.

        \note This function has a C++ wrapper Simd::Bgr48pToBgra32(const View<A>& blue, const View<A>& green, const View<A>& red, View<A>& bgra, uint8_t alpha).

        \param [in] blue - a pointer to pixels data of input 16-bit image with blue color plane.
        \param [in] blueStride - a row size of the blue image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] green - a pointer to pixels data of input 16-bit image with green color plane.
        \param [in] greenStride - a row size of the green image.
        \param [in] red - a pointer to pixels data of input 16-bit image with red color plane.
        \param [in] redStride - a row size of the red image.
        \param [out] bgra - a pointer to pixels data of output 32-bit BGRA image.
        \param [in] bgraStride - a row size of the bgra image.
        \param [in] alpha - a value of alpha channel.
    */
    SIMD_API void SimdBgr48pToBgra32(const uint8_t * blue, size_t blueStride, size_t width, size_t height,
        const uint8_t * green, size_t greenStride, const uint8_t * red, size_t redStride, uint8_t * bgra, size_t bgraStride, uint8_t alpha);

    /*! @ingroup bgr_conversion

        \fn void SimdBgrToGray(const uint8_t * bgr, size_t width, size_t height, size_t bgrStride, uint8_t * gray, size_t grayStride);

        \short Converts a 24-bit BGR image to an 8-bit gray image.

        For every pixel:
        \verbatim
        gray = round(0.114*blue + 0.587*green + 0.299*red).
        \endverbatim
        Input and output images must have the same width and height.

        \note This function has a C++ wrapper Simd::BgrToGray(const View<A>& bgr, View<A>& gray).

        \param [in] bgr - a pointer to pixels data of input 24-bit BGR image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] bgrStride - a row size of the bgr image.
        \param [out] gray - a pointer to pixels data of output 8-bit gray image.
        \param [in] grayStride - a row size of the gray image.
    */
    SIMD_API void SimdBgrToGray(const uint8_t * bgr, size_t width, size_t height, size_t bgrStride, uint8_t * gray, size_t grayStride);

    /*! @ingroup bgr_conversion

        \fn void SimdBgrToHsl(const uint8_t * bgr, size_t width, size_t height, size_t bgrStride, uint8_t * hsl, size_t hslStride);

        \short Converts a 24-bit BGR image to a 24-bit HSL (Hue, Saturation, Lightness) image.

        For each output pixel: hsl[0] = hue, hsl[1] = saturation, hsl[2] = lightness.
        All HSL components are stored as 8-bit values in range [0, 255].
        Input and output images must have the same width and height.

        \note This function has a C++ wrapper Simd::BgrToHsl(const View<A>& bgr, View<A>& hsl).

        \param [in] bgr - a pointer to pixels data of input 24-bit BGR image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] bgrStride - a row size of the bgr image.
        \param [out] hsl - a pointer to pixels data of output 24-bit HSL image.
        \param [in] hslStride - a row size of the hsl image.
    */
    SIMD_API void SimdBgrToHsl(const uint8_t * bgr, size_t width, size_t height, size_t bgrStride, uint8_t * hsl, size_t hslStride);

    /*! @ingroup bgr_conversion

        \fn void SimdBgrToHsv(const uint8_t * bgr, size_t width, size_t height, size_t bgrStride, uint8_t * hsv, size_t hsvStride);

        \short Converts a 24-bit BGR image to a 24-bit HSV (Hue, Saturation, Value) image.

        For each output pixel: hsv[0] = hue, hsv[1] = saturation, hsv[2] = value.
        All HSV components are stored as 8-bit values in range [0, 255].
        Input and output images must have the same width and height.

        \note This function has a C++ wrapper Simd::BgrToHsv(const View<A>& bgr, View<A>& hsv).

        \param [in] bgr - a pointer to pixels data of input 24-bit BGR image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] bgrStride - a row size of the bgr image.
        \param [out] hsv - a pointer to pixels data of output 24-bit HSV image.
        \param [in] hsvStride - a row size of the hsv image.
    */
    SIMD_API void SimdBgrToHsv(const uint8_t * bgr, size_t width, size_t height, size_t bgrStride, uint8_t * hsv, size_t hsvStride);

    /*! @ingroup bgr_conversion

        \fn void SimdBgrToLab(const uint8_t * bgr, size_t bgrStride, size_t width, size_t height, uint8_t * lab, size_t labStride);

        \short Converts a 24-bit BGR image to a 24-bit CIELAB image.

        For each output pixel: lab[0] = L, lab[1] = A, lab[2] = B.
        All LAB components are stored as 8-bit values (OpenCV-compatible CIELAB encoding).
        Input and output images must have the same width and height.

        \note This function has a C++ wrapper Simd::BgrToLab(const View<A>& bgr, View<A>& lab).

        \param [in] bgr - a pointer to pixels data of input 24-bit BGR image.
        \param [in] bgrStride - a row size of the bgr image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] lab - a pointer to pixels data of output 24-bit LAB image.
        \param [in] labStride - a row size of the lab image.
    */
    SIMD_API void SimdBgrToLab(const uint8_t * bgr, size_t bgrStride, size_t width, size_t height, uint8_t * lab, size_t labStride);

    /*! @ingroup bgr_conversion

        \fn void SimdBgrToRgb(const uint8_t * bgr, size_t width, size_t height, size_t bgrStride, uint8_t * rgb, size_t rgbStride);

        \short Swaps blue and red channels in a 24-bit image.

        For each output pixel: rgb[0] = bgr[2], rgb[1] = bgr[1], rgb[2] = bgr[0].
        Input and output images must have the same width and height.

        \note This function has C++ wrappers: Simd::BgrToRgb(const View<A> & bgr, View<A> & rgb) 
            and Simd::RgbToBgr(const View<A>& rgb, View<A>& bgr).

        \param [in] bgr - a pointer to pixels data of input 24-bit BGR image (or 24-bit RGB image).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] bgrStride - a row size of the bgr image.
        \param [out] rgb - a pointer to pixels data of output 24-bit RGB image (or 24-bit BGR image).
        \param [in] rgbStride - a row size of the rgb image.
    */
    SIMD_API void SimdBgrToRgb(const uint8_t * bgr, size_t width, size_t height, size_t bgrStride, uint8_t * rgb, size_t rgbStride);


    /*! @ingroup bgr_conversion

        \fn void SimdBgrToYuv420pV2(const uint8_t * bgr, size_t bgrStride, size_t width, size_t height, uint8_t * y, size_t yStride, uint8_t * u, size_t uStride, uint8_t * v, size_t vStride, SimdYuvType yuvType);

        \short Converts a 24-bit BGR image to planar YUV420P.

        The input BGR and output Y images must have the same width and height.
        U and V images are half-sized in both dimensions: uWidth = vWidth = width/2 and uHeight = vHeight = height/2.
        Image width and height must be even and not less than 2.
        Y is computed for every source pixel. U and V are computed per each 2x2 source block from averaged B, G and R values.

        \note This function has a C++ wrapper Simd::BgrToYuv420p(const View<A>& bgr, View<A>& y, View<A>& u, View<A>& v, SimdYuvType yuvType = SimdYuvBt601).

        \param [in] bgr - a pointer to pixels data of input 24-bit BGR image.
        \param [in] bgrStride - a row size of the BGR image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] y - a pointer to pixels data of output 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [out] u - a pointer to pixels data of output 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [out] v - a pointer to pixels data of output 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] yuvType - a type of output YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdBgrToYuv420pV2(const uint8_t* bgr, size_t bgrStride, size_t width, size_t height, uint8_t* y, size_t yStride, uint8_t* u, size_t uStride, uint8_t* v, size_t vStride, SimdYuvType yuvType);

    /*! @ingroup bgr_conversion

        \fn void SimdBgrToYuv422pV2(const uint8_t * bgr, size_t bgrStride, size_t width, size_t height, uint8_t * y, size_t yStride, uint8_t * u, size_t uStride, uint8_t * v, size_t vStride, SimdYuvType yuvType);

        \short Converts a 24-bit BGR image to planar YUV422P.

        The input BGR and output Y images must have the same width and height.
        U and V images are half-sized horizontally: uWidth = vWidth = width/2 and uHeight = vHeight = height.
        Image width must be even and not less than 2.
        Y is computed for every source pixel. U and V are computed per each pair of neighboring horizontal pixels from averaged B, G and R values.

        \note This function has a C++ wrapper Simd::BgrToYuv422p(const View<A>& bgr, View<A>& y, View<A>& u, View<A>& v, SimdYuvType yuvType = SimdYuvBt601).

        \param [in] bgr - a pointer to pixels data of input 24-bit BGR image.
        \param [in] bgrStride - a row size of the BGR image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] y - a pointer to pixels data of output 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [out] u - a pointer to pixels data of output 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [out] v - a pointer to pixels data of output 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] yuvType - a type of output YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdBgrToYuv422pV2(const uint8_t* bgr, size_t bgrStride, size_t width, size_t height, uint8_t* y, size_t yStride, uint8_t* u, size_t uStride, uint8_t* v, size_t vStride, SimdYuvType yuvType);

    /*! @ingroup bgr_conversion

        \fn void SimdBgrToYuv444pV2(const uint8_t * bgr, size_t bgrStride, size_t width, size_t height, uint8_t * y, size_t yStride, uint8_t * u, size_t uStride, uint8_t * v, size_t vStride, SimdYuvType yuvType);

        \short Converts a 24-bit BGR image to planar YUV444P.

        The input BGR and output Y, U and V images must have the same width and height.
        Y, U and V are computed for each source pixel without chroma subsampling.

        \note This function has a C++ wrapper Simd::BgrToYuv444p(const View<A>& bgr, View<A>& y, View<A>& u, View<A>& v, SimdYuvType yuvType = SimdYuvBt601).

        \param [in] bgr - a pointer to pixels data of input 24-bit BGR image.
        \param [in] bgrStride - a row size of the BGR image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] y - a pointer to pixels data of output 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [out] u - a pointer to pixels data of output 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [out] v - a pointer to pixels data of output 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] yuvType - a type of output YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdBgrToYuv444pV2(const uint8_t* bgr, size_t bgrStride, size_t width, size_t height, uint8_t* y, size_t yStride, uint8_t* u, size_t uStride, uint8_t* v, size_t vStride, SimdYuvType yuvType);

    /*! @ingroup binarization

        \fn void SimdBinarization(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t value, uint8_t positive, uint8_t negative, uint8_t * dst, size_t dstStride, SimdCompareType compareType);

        \short Performs per-pixel binarization of an 8-bit gray image.

        All images must have 8-bit gray format and must have the same width and height.

        For every point:
        \verbatim
        dst[i] = compare(src[i], value) ? positive : negative;
        \endverbatim
        where compare(a, b) is selected by compareType (see ::SimdCompareType).

        \note This function has a C++ wrapper Simd::Binarization(const View<A>& src, uint8_t value, uint8_t positive, uint8_t negative, View<A>& dst, SimdCompareType compareType).

        \param [in] src - a pointer to pixels data of input 8-bit gray image (first value for compare operation).
        \param [in] srcStride - a row size of the src image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] value - a second value for compare operation.
        \param [in] positive - a destination value if comparison operation has a positive result.
        \param [in] negative - a destination value if comparison operation has a negative result.
        \param [out] dst - a pointer to pixels data of output 8-bit gray binarized image.
        \param [in] dstStride - a row size of the dst image.
        \param [in] compareType - a compare operation type (see ::SimdCompareType).
    */
    SIMD_API void SimdBinarization(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        uint8_t value, uint8_t positive, uint8_t negative, uint8_t * dst, size_t dstStride, SimdCompareType compareType);

    /*! @ingroup binarization

        \fn void SimdAveragingBinarization(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t value, size_t neighborhood, uint8_t threshold, uint8_t positive, uint8_t negative, uint8_t * dst, size_t dstStride, SimdCompareType compareType);

        \short Performs neighborhood-based binarization of an 8-bit gray image.

        All images must have 8-bit gray format and must have the same width and height.
        Image width and height must be greater than neighborhood; neighborhood must be less than 128.

        For every point:
        \verbatim
        sum = 0; area = 0;
        for(dy = -neighborhood; dy <= neighborhood; ++dy)
        {
            for(dx = -neighborhood; dx <= neighborhood; ++dx)
            {
                if(x + dx >= 0 && x + dx < width && y + dy >= 0 && y + dy < height)
                {
                    area++;
                    if(compare(src[x + dx, y + dy], value))
                        sum++;
                }
            }
        }
        dst[x, y] = sum*255 > area*threshold ? positive : negative;
        \endverbatim
        where compare(a, b) is selected by compareType (see ::SimdCompareType).

        \note This function has a C++ wrapper Simd::AveragingBinarization(const View<A>& src, uint8_t value, size_t neighborhood, uint8_t threshold, uint8_t positive, uint8_t negative, View<A>& dst, SimdCompareType compareType).

        \param [in] src - a pointer to pixels data of input 8-bit gray image (first value for compare operation).
        \param [in] srcStride - a row size of the src image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] value - a second value for compare operation.
        \param [in] neighborhood - an averaging neighborhood.
        \param [in] threshold - a threshold value in range [0, 255] used as: sum*255 > area*threshold.
        \param [in] positive - a destination value if for neighborhood of this point number of positive comparisons is greater than threshold.
        \param [in] negative - a destination value if for neighborhood of this point number of positive comparisons is less than or equal to threshold.
        \param [out] dst - a pointer to pixels data of output 8-bit gray binarized image.
        \param [in] dstStride - a row size of the dst image.
        \param [in] compareType - a compare operation type (see ::SimdCompareType).
    */
    SIMD_API void SimdAveragingBinarization(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        uint8_t value, size_t neighborhood, uint8_t threshold, uint8_t positive, uint8_t negative,
        uint8_t * dst, size_t dstStride, SimdCompareType compareType);

    /*! @ingroup binarization

        \fn void SimdAveragingBinarizationV2(const uint8_t* src, size_t srcStride, size_t width, size_t height, size_t neighborhood, int32_t shift, uint8_t positive, uint8_t negative, uint8_t* dst, size_t dstStride);

        \short Performs adaptive mean-like binarization of an 8-bit gray image.

        All images must have 8-bit gray format and must have the same width and height.
        Image width and height must be greater than neighborhood.

        For every point:
        \verbatim
        sum = 0; area = 0;
        for(dy = -neighborhood; dy <= neighborhood; ++dy)
        {
            for(dx = -neighborhood; dx <= neighborhood; ++dx)
            {
                if(x + dx >= 0 && x + dx < width && y + dy >= 0 && y + dy < height)
                {
                    area++;
                    sum += src[x + dx, x + dy];
                }
            }
        }
        dst[x, y] = (src[x, y] + shift)*area > sum ? positive : negative;
        \endverbatim

        \note This function has a C++ wrapper Simd::AveragingBinarizationV2(const View<A>& src, size_t neighborhood, int32_t shift, uint8_t positive, uint8_t negative, View<A>& dst).

        \param [in] src - a pointer to pixels data of input 8-bit gray image.
        \param [in] srcStride - a row size of the src image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] neighborhood - an averaging neighborhood.
        \param [in] shift - an additive shift in condition: (src[x, y] + shift)*area > sum.
        \param [in] positive - a destination value for positive value of the condition.
        \param [in] negative - a destination value for negative value of the condition.
        \param [out] dst - a pointer to pixels data of output 8-bit gray binarized image.
        \param [in] dstStride - a row size of the dst image.
    */
    SIMD_API void SimdAveragingBinarizationV2(const uint8_t* src, size_t srcStride, size_t width, size_t height,
        size_t neighborhood, int32_t shift, uint8_t positive, uint8_t negative, uint8_t* dst, size_t dstStride);

    /*! @ingroup conditional

        \fn void SimdConditionalCount8u(const uint8_t * src, size_t stride, size_t width, size_t height, uint8_t value, SimdCompareType compareType, uint32_t * count);

        \short Counts the number of pixels in an 8-bit gray image that satisfy a given comparison condition against a reference value.

        For every pixel:
        \verbatim
        if(compare(src[x, y], value))
            count++;
        \endverbatim
        where compare(a, b) depends on compareType (see ::SimdCompareType).

        The output count is initialized to zero before accumulation.

        \note This function has a C++ wrapper Simd::ConditionalCount8u(const View<A> & src, uint8_t value, SimdCompareType compareType, uint32_t & count).

        \param [in] src - a pointer to pixels data of the input 8-bit gray image. Each pixel is compared against \a value.
        \param [in] stride - a row size of the \a src image in bytes.
        \param [in] width - an image width in pixels.
        \param [in] height - an image height in pixels.
        \param [in] value - a reference value used as the second operand in the comparison.
        \param [in] compareType - a comparison operation type (see ::SimdCompareType).
        \param [out] count - a pointer to an unsigned 32-bit integer that receives the number of pixels satisfying the condition.
    */
    SIMD_API void SimdConditionalCount8u(const uint8_t * src, size_t stride, size_t width, size_t height,
        uint8_t value, SimdCompareType compareType, uint32_t * count);

    /*! @ingroup conditional

        \fn void SimdConditionalCount16i(const uint8_t * src, size_t stride, size_t width, size_t height, int16_t value, SimdCompareType compareType, uint32_t * count);

        \short Counts the number of pixels in a 16-bit signed integer image that satisfy a given comparison condition against a reference value.

        For every pixel:
        \verbatim
        if(compare(src[x, y], value))
            count++;
        \endverbatim
        where compare(a, b) depends on compareType (see ::SimdCompareType).

        The output count is initialized to zero before accumulation.
        Although the \a src pointer has type `uint8_t *`, each pixel occupies 2 bytes and is interpreted as a signed 16-bit integer.
        The \a stride is expressed in bytes, while \a width is expressed in 16-bit pixels (elements).

        \note This function has a C++ wrapper Simd::ConditionalCount16i(const View<A> & src, int16_t value, SimdCompareType compareType, uint32_t & count).

        \param [in] src - a pointer to pixels data of the input 16-bit signed integer image. Each pixel is compared against \a value.
        \param [in] stride - a row size of the \a src image in bytes.
        \param [in] width - an image width in 16-bit pixels (elements per row).
        \param [in] height - an image height in pixels.
        \param [in] value - a reference value used as the second operand in the comparison.
        \param [in] compareType - a comparison operation type (see ::SimdCompareType).
        \param [out] count - a pointer to an unsigned 32-bit integer that receives the number of pixels satisfying the condition.
    */
    SIMD_API void SimdConditionalCount16i(const uint8_t * src, size_t stride, size_t width, size_t height,
        int16_t value, SimdCompareType compareType, uint32_t * count);

    /*! @ingroup conditional

        \fn void SimdConditionalSum(const uint8_t * src, size_t srcStride, size_t width, size_t height, const uint8_t * mask, size_t maskStride, uint8_t value, SimdCompareType compareType, uint64_t * sum);

        \short Calculates the sum of pixels in a source image at positions where the corresponding mask pixels satisfy a given comparison condition.

        All images must have 8-bit gray format and the same width and height.

        For every pixel:
        \verbatim
        if(compare(mask[x, y], value))
            sum += src[x, y];
        \endverbatim
        where compare(a, b) depends on compareType (see ::SimdCompareType).

        The output sum is initialized to zero before accumulation.

        \note This function has a C++ wrapper Simd::ConditionalSum(const View<A> & src, const View<A> & mask, uint8_t value, SimdCompareType compareType, uint64_t & sum).

        \param [in] src - a pointer to pixels data of the input 8-bit gray image whose pixel values are accumulated.
        \param [in] srcStride - a row size of the \a src image in bytes.
        \param [in] width - an image width in pixels.
        \param [in] height - an image height in pixels.
        \param [in] mask - a pointer to pixels data of the 8-bit gray mask image. Each mask pixel is compared against \a value.
        \param [in] maskStride - a row size of the \a mask image in bytes.
        \param [in] value - a reference value used as the second operand in the comparison.
        \param [in] compareType - a comparison operation type (see ::SimdCompareType).
        \param [out] sum - a pointer to an unsigned 64-bit integer that receives the accumulated sum.
    */
    SIMD_API void SimdConditionalSum(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        const uint8_t * mask, size_t maskStride, uint8_t value, SimdCompareType compareType, uint64_t * sum);

    /*! @ingroup conditional

        \fn void SimdConditionalSquareSum(const uint8_t * src, size_t srcStride, size_t width, size_t height, const uint8_t * mask, size_t maskStride, uint8_t value, SimdCompareType compareType, uint64_t * sum);

        \short Calculates the sum of squared pixel values in a source image at positions where the corresponding mask pixels satisfy a given comparison condition.

        All images must have 8-bit gray format and the same width and height.

        For every pixel:
        \verbatim
        if(compare(mask[x, y], value))
            sum += src[x, y] * src[x, y];
        \endverbatim
        where compare(a, b) depends on compareType (see ::SimdCompareType).

        The output sum is initialized to zero before accumulation.

        \note This function has a C++ wrapper Simd::ConditionalSquareSum(const View<A> & src, const View<A> & mask, uint8_t value, SimdCompareType compareType, uint64_t & sum).

        \param [in] src - a pointer to pixels data of the input 8-bit gray image whose squared pixel values are accumulated.
        \param [in] srcStride - a row size of the \a src image in bytes.
        \param [in] width - an image width in pixels.
        \param [in] height - an image height in pixels.
        \param [in] mask - a pointer to pixels data of the 8-bit gray mask image. Each mask pixel is compared against \a value.
        \param [in] maskStride - a row size of the \a mask image in bytes.
        \param [in] value - a reference value used as the second operand in the comparison.
        \param [in] compareType - a comparison operation type (see ::SimdCompareType).
        \param [out] sum - a pointer to an unsigned 64-bit integer that receives the accumulated sum of squares.
    */
    SIMD_API void SimdConditionalSquareSum(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        const uint8_t * mask, size_t maskStride, uint8_t value, SimdCompareType compareType, uint64_t * sum);

    /*! @ingroup conditional

        \fn void SimdConditionalSquareGradientSum(const uint8_t * src, size_t srcStride, size_t width, size_t height, const uint8_t * mask, size_t maskStride, uint8_t value, SimdCompareType compareType, uint64_t * sum);

        \short Calculates the sum of squared gradient magnitudes in a source image at positions where the corresponding mask pixels satisfy a given comparison condition.

        All images must have 8-bit gray format and the same width and height. The image width and height must each be at least 3.
        Border pixels (first and last row, first and last column) are excluded from processing.

        For every non-border pixel:
        \verbatim
        if(compare(mask[x, y], value))
        {
            dx = src[x + 1, y] - src[x - 1, y];
            dy = src[x, y + 1] - src[x, y - 1];
            sum += dx*dx + dy*dy;
        }
        \endverbatim
        where compare(a, b) depends on compareType (see ::SimdCompareType).

        The output sum is initialized to zero before accumulation.

        \note This function has a C++ wrapper Simd::ConditionalSquareGradientSum(const View<A> & src, const View<A> & mask, uint8_t value, SimdCompareType compareType, uint64_t & sum).

        \param [in] src - a pointer to pixels data of the input 8-bit gray image used to compute gradients.
        \param [in] srcStride - a row size of the \a src image in bytes.
        \param [in] width - an image width in pixels (must be >= 3).
        \param [in] height - an image height in pixels (must be >= 3).
        \param [in] mask - a pointer to pixels data of the 8-bit gray mask image. Each mask pixel is compared against \a value.
        \param [in] maskStride - a row size of the \a mask image in bytes.
        \param [in] value - a reference value used as the second operand in the comparison.
        \param [in] compareType - a comparison operation type (see ::SimdCompareType).
        \param [out] sum - a pointer to an unsigned 64-bit integer that receives the accumulated sum of squared gradients.
    */
    SIMD_API void SimdConditionalSquareGradientSum(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        const uint8_t * mask, size_t maskStride, uint8_t value, SimdCompareType compareType, uint64_t * sum);

    /*! @ingroup conditional

        \fn void SimdConditionalFill(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t threshold, SimdCompareType compareType, uint8_t value, uint8_t * dst, size_t dstStride);

        \short Fills pixels of an 8-bit gray destination image with a given value at positions where the corresponding source pixels satisfy a given comparison condition. Pixels that do not satisfy the condition are left unchanged.

        All images must have 8-bit gray format and the same width and height.

        For every pixel:
        \verbatim
        if(compare(src[x, y], threshold))
            dst[x, y] = value;
        \endverbatim
        where compare(a, b) depends on compareType (see ::SimdCompareType).

        \note This function has a C++ wrapper Simd::ConditionalFill(const View<A> & src, uint8_t threshold, SimdCompareType compareType, uint8_t value, View<A> & dst).

        \param [in] src - a pointer to pixels data of the input 8-bit gray image. Each pixel is compared against \a threshold.
        \param [in] srcStride - a row size of the \a src image in bytes.
        \param [in] width - an image width in pixels.
        \param [in] height - an image height in pixels.
        \param [in] threshold - a reference value used as the second operand in the comparison.
        \param [in] compareType - a comparison operation type (see ::SimdCompareType).
        \param [in] value - a fill value written to \a dst pixels where the condition is satisfied.
        \param [in, out] dst - a pointer to pixels data of the output 8-bit gray image. Pixels not satisfying the condition retain their existing values.
        \param [in] dstStride - a row size of the \a dst image in bytes.
    */
    SIMD_API void SimdConditionalFill(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        uint8_t threshold, SimdCompareType compareType, uint8_t value, uint8_t * dst, size_t dstStride);

    /*! @ingroup copying

        \fn void SimdCopy(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t pixelSize, uint8_t * dst, size_t dstStride);

        \short Copies pixel data row by row from a source image to a destination image.

        Supports any pixel format; \a pixelSize specifies the number of bytes per pixel.
        The source and destination images must have the same width, height, and pixel size,
        but may have different row strides (e.g. due to row alignment padding).

        \note This function has a C++ wrapper Simd::Copy(const View<A> & src, View<B> & dst).

        \param [in] src - a pointer to pixels data of the source image.
        \param [in] srcStride - a row size of the \a src image in bytes (including any padding).
        \param [in] width - an image width in pixels.
        \param [in] height - an image height in pixels.
        \param [in] pixelSize - a size of one pixel in bytes.
        \param [out] dst - a pointer to pixels data of the destination image.
        \param [in] dstStride - a row size of the \a dst image in bytes (including any padding).
    */
    SIMD_API void SimdCopy(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t pixelSize, uint8_t * dst, size_t dstStride);

    /*! @ingroup copying

        \fn void SimdCopyFrame(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t pixelSize, size_t frameLeft, size_t frameTop, size_t frameRight, size_t frameBottom, uint8_t * dst, size_t dstStride);

        \short Copies the outer frame region of a source image to the destination image, leaving the interior rectangle untouched.

        The source and destination images must have the same width, height, and pixel size.
        The frame is defined by the rectangle [\a frameLeft, \a frameRight) x [\a frameTop, \a frameBottom).
        Only pixels outside this rectangle (i.e. the surrounding border area) are copied from \a src to \a dst.
        Pixels inside the frame interior are not written to \a dst.

        The following regions are copied:
        - All rows above \a frameTop (full width).
        - All rows at or below \a frameBottom (full width).
        - For rows within [\a frameTop, \a frameBottom): columns to the left of \a frameLeft.
        - For rows within [\a frameTop, \a frameBottom): columns at or to the right of \a frameRight.

        \note This function has a C++ wrapper Simd::CopyFrame(const View<A>& src, const Rectangle<ptrdiff_t> & frame, View<A>& dst).

        \param [in] src - a pointer to pixels data of the source image.
        \param [in] srcStride - a row size of the \a src image in bytes (including any padding).
        \param [in] width - an image width in pixels.
        \param [in] height - an image height in pixels.
        \param [in] pixelSize - a size of one pixel in bytes.
        \param [in] frameLeft - the left boundary (inclusive) of the interior rectangle in pixels.
        \param [in] frameTop - the top boundary (inclusive) of the interior rectangle in pixels.
        \param [in] frameRight - the right boundary (exclusive) of the interior rectangle in pixels.
        \param [in] frameBottom - the bottom boundary (exclusive) of the interior rectangle in pixels.
        \param [out] dst - a pointer to pixels data of the destination image.
        \param [in] dstStride - a row size of the \a dst image in bytes (including any padding).
    */
    SIMD_API void SimdCopyFrame(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t pixelSize,
        size_t frameLeft, size_t frameTop, size_t frameRight, size_t frameBottom, uint8_t * dst, size_t dstStride);

    /*! @ingroup descrint

        \fn void * SimdDescrIntInit(size_t size, size_t depth);

        \short Initializes Integer Descriptor Engine context.

        The engine context stores the parameters needed to encode float descriptors into a compact
        integer representation, decode them back, and compute cosine distances directly on the
        encoded form without full decoding.

        Each encoded descriptor produced by this engine is a byte buffer whose layout is:
        - Bytes  0.. 3: 32-bit float inverse quantization scale (1 / scale).
        - Bytes  4.. 7: 32-bit float minimum value (shift) used during quantization.
        - Bytes  8..11: 32-bit float precomputed sum helper for dot-product reconstruction.
        - Bytes 12..15: 32-bit float precomputed L2 norm of the original float descriptor.
        - Bytes 16.. N: bit-packed quantized integer values, \a depth bits per element,
                        packed contiguously in little-endian order.

        The total byte size of the encoded buffer is returned by ::SimdDescrIntEncodedSize.

        \param [in] size - a length of the original (32-bit or 16-bit float) descriptor, i.e. the number of float elements. 
                          It must be a multiple of 8 and must not exceed 32768.
        \param [in] depth - the number of bits used to represent each quantized element in the encoded descriptor. Supported values: 4, 5, 6, 7, 8.
        \return a pointer to Integer Descriptor Engine context. On error it returns NULL. It must be released with using of function ::SimdRelease.
                This pointer is used in functions ::SimdDescrIntEncodedSize, ::SimdDescrIntDecodedSize, 
                ::SimdDescrIntEncode32f, ::SimdDescrIntEncode16f, ::SimdDescrIntDecode32f, ::SimdDescrIntDecode16f, 
                ::SimdDescrIntCosineDistance, ::SimdDescrIntCosineDistancesMxNa, ::SimdDescrIntCosineDistancesMxNp, ::SimdDescrIntVectorNorm.
    */
    SIMD_API void * SimdDescrIntInit(size_t size, size_t depth);

    /*! @ingroup descrint

        \fn size_t SimdDescrIntEncodedSize(const void* context);

        \short Gets the size in bytes of an encoded integer descriptor produced by this engine.

        The encoded descriptor consists of a 16-byte header (4 x 32-bit floats storing the inverse
        quantization scale, the minimum value, a precomputed sum helper, and the precomputed L2 norm)
        followed by the bit-packed quantized integer data. The total size equals
        16 + ceil(size * depth / 8), where \a size and \a depth are the values passed to ::SimdDescrIntInit.

        \param [in] context - a pointer to Integer Descriptor Engine context. It must be created by function ::SimdDescrIntInit and released by function ::SimdRelease.
        \return the size in bytes of an encoded integer descriptor.
    */
    SIMD_API size_t SimdDescrIntEncodedSize(const void* context);

    /*! @ingroup descrint

        \fn size_t SimdDescrIntDecodedSize(const void* context);

        \short Gets the number of elements (floats) in the original descriptor.

        This is the value of the \a size parameter that was passed to ::SimdDescrIntInit.
        It equals the number of 32-bit or 16-bit float elements in the uncompressed descriptor,
        and is the required length of the \a src buffer for ::SimdDescrIntEncode32f / ::SimdDescrIntEncode16f
        and the \a dst buffer for ::SimdDescrIntDecode32f / ::SimdDescrIntDecode16f.

        \param [in] context - a pointer to Integer Descriptor Engine context. It must be created by function ::SimdDescrIntInit and released by function ::SimdRelease.
        \return the number of float elements in the original (decoded) descriptor.
    */
    SIMD_API size_t SimdDescrIntDecodedSize(const void* context);

    /*! @ingroup descrint

        \fn void SimdDescrIntEncode32f(const void* context, const float * src, uint8_t * dst);

        \short Encodes a 32-bit float descriptor into a compact integer representation.

        The function quantizes each element of the input float array linearly into the range
        [0, 2^depth - 1], where \a depth was specified at context creation. The encoding procedure:
        1. Finds the minimum and maximum values of the source descriptor.
        2. Computes a quantization scale: scale = (2^depth - 1) / (max - min).
        3. Quantizes each element: q[i] = round((src[i] - min) * scale).
        4. Packs the quantized values bit-by-bit (\a depth bits per element) into the output buffer
           starting at byte offset 16.
        5. Writes a 16-byte header at the beginning of \a dst containing four 32-bit floats:
           inverse scale (1/scale), minimum value (min), a precomputed sum helper used for
           dot-product reconstruction, and the precomputed L2 norm of the original descriptor.

        The precomputed norm and sum helper in the header allow ::SimdDescrIntCosineDistance and
        related functions to compute cosine distances without decoding the descriptor.

        \param [in] context - a pointer to Integer Descriptor Engine context. It must be created by function ::SimdDescrIntInit and released by function ::SimdRelease.
        \param [in] src - a pointer to the input 32-bit float descriptor. The number of elements must equal the value returned by ::SimdDescrIntDecodedSize.
        \param [out] dst - a pointer to the output encoded integer descriptor. The buffer size in bytes must be at least the value returned by ::SimdDescrIntEncodedSize.
    */
    SIMD_API void SimdDescrIntEncode32f(const void* context, const float * src, uint8_t * dst);

    /*! @ingroup descrint

        \fn void SimdDescrIntEncode16f(const void* context, const uint16_t * src, uint8_t * dst);

        \short Encodes a 16-bit float descriptor into a compact integer representation.

        This function is identical in behavior to ::SimdDescrIntEncode32f except that the input
        descriptor elements are 16-bit floats (half precision, stored as uint16_t). Each element is
        first converted to 32-bit float internally, then quantized and packed in the same way.
        The output encoded descriptor format is identical to that produced by ::SimdDescrIntEncode32f
        and is fully compatible with all decode and distance functions.

        \param [in] context - a pointer to Integer Descriptor Engine context. It must be created by function ::SimdDescrIntInit and released by function ::SimdRelease.
        \param [in] src - a pointer to the input 16-bit float descriptor (half precision, stored as uint16_t). The number of elements must equal the value returned by ::SimdDescrIntDecodedSize.
        \param [out] dst - a pointer to the output encoded integer descriptor. The buffer size in bytes must be at least the value returned by ::SimdDescrIntEncodedSize.
    */
    SIMD_API void SimdDescrIntEncode16f(const void* context, const uint16_t* src, uint8_t* dst);

    /*! @ingroup descrint

        \fn void SimdDescrIntDecode32f(const void* context, const uint8_t* src, float* dst);

        \short Decodes an integer descriptor back into a 32-bit float descriptor.

        The function reconstructs the original float values from the bit-packed quantized data
        using the inverse scale and minimum value stored in the 16-byte header of the encoded
        descriptor. Each reconstructed element is computed as: dst[i] = q[i] * invScale + min,
        where \a invScale and \a min are read from the first two 32-bit floats of \a src.
        The decoded values are approximations of the original floats; precision depends on \a depth.

        \param [in] context - a pointer to Integer Descriptor Engine context. It must be created by function ::SimdDescrIntInit and released by function ::SimdRelease.
        \param [in] src - a pointer to the encoded integer descriptor. The buffer size in bytes must be at least the value returned by ::SimdDescrIntEncodedSize.
        \param [out] dst - a pointer to the output 32-bit float descriptor. The number of elements must equal the value returned by ::SimdDescrIntDecodedSize.
    */
    SIMD_API void SimdDescrIntDecode32f(const void* context, const uint8_t* src, float* dst);

    /*! @ingroup descrint

        \fn void SimdDescrIntDecode16f(const void* context, const uint8_t* src, uint16_t* dst);

        \short Decodes an integer descriptor back into a 16-bit float descriptor.

        This function is identical in behavior to ::SimdDescrIntDecode32f except that each
        reconstructed element is converted from 32-bit float to 16-bit float (half precision,
        stored as uint16_t) before being written to the output buffer.

        \param [in] context - a pointer to Integer Descriptor Engine context. It must be created by function ::SimdDescrIntInit and released by function ::SimdRelease.
        \param [in] src - a pointer to the encoded integer descriptor. The buffer size in bytes must be at least the value returned by ::SimdDescrIntEncodedSize.
        \param [out] dst - a pointer to the output 16-bit float descriptor (half precision, stored as uint16_t). The number of elements must equal the value returned by ::SimdDescrIntDecodedSize.
    */
    SIMD_API void SimdDescrIntDecode16f(const void* context, const uint8_t* src, uint16_t* dst);

    /*! @ingroup descrint

        \fn void SimdDescrIntCosineDistance(const void* context, const uint8_t* a, const uint8_t* b, float* distance);

        \short Calculates the cosine distance between two encoded integer descriptors.

        The cosine distance is defined as: distance = 1 - dot(a, b) / (||a|| * ||b||),
        where \a a and \a b are treated as vectors in the original float space.
        The function computes the integer dot product directly on the bit-packed data and then
        reconstructs the true float dot product using the quantization scale and shift stored
        in the 16-byte headers of the encoded descriptors. The L2 norms are read directly from
        the precomputed values in the headers, avoiding full decoding.
        The result is clamped to the range [0, 2].

        \note An encoded integer descriptor is produced by ::SimdDescrIntEncode32f or ::SimdDescrIntEncode16f. Its size in bytes is determined by function ::SimdDescrIntEncodedSize.

        \param [in] context - a pointer to Integer Descriptor Engine context. It must be created by function ::SimdDescrIntInit and released by function ::SimdRelease.
        \param [in] a - a pointer to the first encoded integer descriptor.
        \param [in] b - a pointer to the second encoded integer descriptor.
        \param [out] distance - a pointer to a 32-bit float that receives the cosine distance in the range [0, 2].
    */
    SIMD_API void SimdDescrIntCosineDistance(const void* context, const uint8_t* a, const uint8_t* b, float* distance);

    /*! @ingroup descrint

        \fn void SimdDescrIntCosineDistancesMxNa(const void* context, size_t M, size_t N, const uint8_t* const* A, const uint8_t* const* B, float* distances);

        \short Calculates all pairwise cosine distances between two sets of encoded integer descriptors (array-of-pointers form).

        Computes the M x N matrix of cosine distances, where distances[i * N + j] is the cosine
        distance between the i-th descriptor in \a A and the j-th descriptor in \a B.
        See ::SimdDescrIntCosineDistance for the definition of cosine distance.
        This variant accepts the descriptors through arrays of pointers, which allows non-contiguous
        memory layouts. For contiguous storage use ::SimdDescrIntCosineDistancesMxNp instead.
        The implementation automatically selects cache-friendly blocking strategies.

        \note An encoded integer descriptor is produced by ::SimdDescrIntEncode32f or ::SimdDescrIntEncode16f. Its size in bytes is determined by function ::SimdDescrIntEncodedSize.

        \param [in] context - a pointer to Integer Descriptor Engine context. It must be created by function ::SimdDescrIntInit and released by function ::SimdRelease.
        \param [in] M - the number of descriptors in set \a A (number of rows in the output matrix).
        \param [in] N - the number of descriptors in set \a B (number of columns in the output matrix).
        \param [in] A - an array of M pointers, each pointing to an encoded integer descriptor.
        \param [in] B - an array of N pointers, each pointing to an encoded integer descriptor.
        \param [out] distances - a pointer to the output M x N matrix of 32-bit float cosine distances stored in row-major order. The buffer must hold at least M * N elements.
    */
    SIMD_API void SimdDescrIntCosineDistancesMxNa(const void* context, size_t M, size_t N, const uint8_t* const* A, const uint8_t* const* B, float* distances);

    /*! @ingroup descrint

        \fn void SimdDescrIntCosineDistancesMxNp(const void* context, size_t M, size_t N, const uint8_t* A, const uint8_t* B, float* distances);

        \short Calculates all pairwise cosine distances between two sets of encoded integer descriptors (packed/contiguous form).

        Computes the M x N matrix of cosine distances, where distances[i * N + j] is the cosine
        distance between the i-th descriptor in \a A and the j-th descriptor in \a B.
        See ::SimdDescrIntCosineDistance for the definition of cosine distance.
        This variant accepts the descriptors as two flat contiguous arrays, where descriptor \a i
        starts at A + i * encodedSize and descriptor \a j starts at B + j * encodedSize,
        with encodedSize returned by ::SimdDescrIntEncodedSize.
        For non-contiguous memory layouts use ::SimdDescrIntCosineDistancesMxNa instead.

        \note An encoded integer descriptor is produced by ::SimdDescrIntEncode32f or ::SimdDescrIntEncode16f. Its size in bytes is determined by function ::SimdDescrIntEncodedSize.

        \param [in] context - a pointer to Integer Descriptor Engine context. It must be created by function ::SimdDescrIntInit and released by function ::SimdRelease.
        \param [in] M - the number of descriptors in set \a A (number of rows in the output matrix).
        \param [in] N - the number of descriptors in set \a B (number of columns in the output matrix).
        \param [in] A - a pointer to the contiguous array of M encoded integer descriptors.
        \param [in] B - a pointer to the contiguous array of N encoded integer descriptors.
        \param [out] distances - a pointer to the output M x N matrix of 32-bit float cosine distances stored in row-major order. The buffer must hold at least M * N elements.
    */
    SIMD_API void SimdDescrIntCosineDistancesMxNp(const void* context, size_t M, size_t N, const uint8_t* A, const uint8_t* B, float* distances);

    /*! @ingroup descrint

        \fn void SimdDescrIntVectorNorm(const void* context, const uint8_t* a, float* norm);

        \short Gets the precomputed L2 norm of an encoded integer descriptor.

        The L2 norm of the original float descriptor is computed and stored in the 16-byte header
        of the encoded descriptor during encoding (by ::SimdDescrIntEncode32f or ::SimdDescrIntEncode16f).
        This function retrieves that precomputed value without performing any additional computation.
        The norm equals the Euclidean length of the original float descriptor before quantization.

        \note An encoded integer descriptor is produced by ::SimdDescrIntEncode32f or ::SimdDescrIntEncode16f. Its size in bytes is determined by function ::SimdDescrIntEncodedSize.

        \param [in] context - a pointer to Integer Descriptor Engine context. It must be created by function ::SimdDescrIntInit and released by function ::SimdRelease.
        \param [in] a - a pointer to the encoded integer descriptor.
        \param [out] norm - a pointer to a 32-bit float that receives the precomputed L2 norm of the original float descriptor.
    */
    SIMD_API void SimdDescrIntVectorNorm(const void* context, const uint8_t* a, float* norm);

    /*! @ingroup deinterleave_conversion

        \fn void SimdDeinterleaveUv(const uint8_t * uv, size_t uvStride, size_t width, size_t height, uint8_t * u, size_t uStride, uint8_t * v, size_t vStride);

        \short Deinterleaves 16-bit UV interleaved image into one or two separated 8-bit U and V planar images.

        The input UV image and every non-null output image must have the same width and height.
        For every point:
        \verbatim
        u[i] = uv[2*i + 0];
        v[i] = uv[2*i + 1];
        \endverbatim
        Any output image pointer can be NULL; in this case corresponding channel is not extracted and its stride is ignored.
        If both output image pointers are NULL, the function does nothing.
        This function can be used for extraction of U and/or V planes from NV12 image.

        \note This function has a C++ wrapper Simd::DeinterleaveUv(const View<A>& uv, View<A>& u, View<A>& v).

        \param [in] uv - a pointer to pixels data of input 16-bit UV interleaved image.
        \param [in] uvStride - a row size of the uv image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] u - a pointer to pixels data of 8-bit U planar image. It can be NULL if you don't need this image.
        \param [in] uStride - a row size of the u image.
        \param [out] v - a pointer to pixels data of 8-bit V planar image. It can be NULL if you don't need this image.
        \param [in] vStride - a row size of the v image.
    */
    SIMD_API void SimdDeinterleaveUv(const uint8_t * uv, size_t uvStride, size_t width, size_t height,
        uint8_t * u, size_t uStride, uint8_t * v, size_t vStride);

    /*! @ingroup deinterleave_conversion

        \fn void SimdDeinterleaveBgr(const uint8_t * bgr, size_t bgrStride, size_t width, size_t height, uint8_t * b, size_t bStride, uint8_t * g, size_t gStride, uint8_t * r, size_t rStride);

        \short Deinterleaves 24-bit BGR interleaved image into one, two or three separated 8-bit Blue, Green and Red planar images.

        The input BGR image and every non-null output image must have the same width and height.
        For every point:
        \verbatim
        b[i] = bgr[3*i + 0];
        g[i] = bgr[3*i + 1];
        r[i] = bgr[3*i + 2];
        \endverbatim
        Any output image pointer can be NULL; in this case corresponding channel is not extracted and its stride is ignored.
        If all output image pointers are NULL, the function does nothing.

        \note This function has C++ wrappers:
            Simd::DeinterleaveBgr(const View<A>& bgr, View<A>& b, View<A>& g, View<A>& r),
            Simd::DeinterleaveRgb(const View<A>& rgb, View<A>& r, View<A>& g, View<A>& b).

        \param [in] bgr - a pointer to pixels data of input 24-bit BGR interleaved image.
        \param [in] bgrStride - a row size of the bgr image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] b - a pointer to pixels data of 8-bit Blue planar image. It can be NULL if you don't need this image.
        \param [in] bStride - a row size of the b image.
        \param [out] g - a pointer to pixels data of 8-bit Green planar image. It can be NULL if you don't need this image.
        \param [in] gStride - a row size of the g image.
        \param [out] r - a pointer to pixels data of 8-bit Red planar image. It can be NULL if you don't need this image.
        \param [in] rStride - a row size of the r image.
    */
    SIMD_API void SimdDeinterleaveBgr(const uint8_t * bgr, size_t bgrStride, size_t width, size_t height,
        uint8_t * b, size_t bStride, uint8_t * g, size_t gStride, uint8_t * r, size_t rStride);

    /*! @ingroup deinterleave_conversion

        \fn void SimdDeinterleaveBgra(const uint8_t * bgra, size_t bgraStride, size_t width, size_t height, uint8_t * b, size_t bStride, uint8_t * g, size_t gStride, uint8_t * r, size_t rStride, uint8_t * a, size_t aStride);

        \short Deinterleaves 32-bit BGRA interleaved image into one, two, three or four separated 8-bit Blue, Green, Red and Alpha planar images.

        The input BGRA image and every non-null output image must have the same width and height.
        For every point:
        \verbatim
        b[i] = bgra[4*i + 0];
        g[i] = bgra[4*i + 1];
        r[i] = bgra[4*i + 2];
        a[i] = bgra[4*i + 3];
        \endverbatim
        Any output image pointer can be NULL; in this case corresponding channel is not extracted and its stride is ignored.
        If all output image pointers are NULL, the function does nothing.

        \note This function has C++ wrappers:
            Simd::DeinterleaveBgra(const View<A>& bgra, View<A>& b, View<A>& g, View<A>& r, View<A>& a),
            Simd::DeinterleaveRgba(const View<A>& rgba, View<A>& r, View<A>& g, View<A>& b, View<A>& a).

        \param [in] bgra - a pointer to pixels data of input 32-bit BGRA interleaved image.
        \param [in] bgraStride - a row size of the bgra image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] b - a pointer to pixels data of 8-bit Blue planar image. It can be NULL if you don't need this image.
        \param [in] bStride - a row size of the b image.
        \param [out] g - a pointer to pixels data of 8-bit Green planar image. It can be NULL if you don't need this image.
        \param [in] gStride - a row size of the g image.
        \param [out] r - a pointer to pixels data of 8-bit Red planar image. It can be NULL if you don't need this image.
        \param [in] rStride - a row size of the r image.
        \param [out] a - a pointer to pixels data of 8-bit Alpha planar image. It can be NULL if you don't need this image.
        \param [in] aStride - a row size of the a image. 
    */
    SIMD_API void SimdDeinterleaveBgra(const uint8_t * bgra, size_t bgraStride, size_t width, size_t height,
        uint8_t * b, size_t bStride, uint8_t * g, size_t gStride, uint8_t * r, size_t rStride, uint8_t * a, size_t aStride);

    /*! @ingroup object_detection

        \fn void * SimdDetectionLoadA(const char * path);

        \short Loads an OpenCV-format cascade classifier from an XML file.

        The loader parses BOOST cascades with HAAR or LBP features. HOG cascades, tree-based cascades
        and old cascade formats are not supported. The returned object contains parsed cascade data
        (original window size, stages, features and flags) and is used to create working detection contexts.

        \note This function is used for implementation of Simd::Detection.

        \param [in] path - a path to XML cascade file.
        \return a pointer to loaded cascade. On error it returns NULL.
                This pointer is used in functions ::SimdDetectionInfo and ::SimdDetectionInit, and must be released by function ::SimdRelease.
    */
    SIMD_API void * SimdDetectionLoadA(const char * path);

    /*! @ingroup object_detection

        \fn void * SimdDetectionLoadStringXml(char * xml);

        \short Loads an OpenCV-format cascade classifier from a mutable XML string.

        The loader parses BOOST cascades with HAAR or LBP features. HOG cascades, tree-based cascades
        and old cascade formats are not supported. The XML buffer must be zero-terminated and writable:
        the parser can modify it while parsing. The buffer is needed only during this call; parsed cascade
        data is stored in the returned object.

        \note This function is used for implementation of Simd::Detection.

        \param [in,out] xml - a zero-terminated writable XML string with classifier cascade.
        \return a pointer to loaded cascade. On error it returns NULL.
                This pointer is used in functions ::SimdDetectionInfo and ::SimdDetectionInit, and must be released by function ::SimdRelease.
    */
    SIMD_API void * SimdDetectionLoadStringXml(char * xml);

    /*! @ingroup object_detection

        \fn void SimdDetectionInfo(const void * data, size_t * width, size_t * height, SimdDetectionInfoFlags * flags);

        \short Gets original window size and feature flags of a loaded classifier cascade.

        For a valid cascade this function writes original scanning window size and cascade flags to
        non-NULL output pointers. If data is NULL the function does nothing.
        The low bits of flags contain cascade feature type (see ::SimdDetectionInfoFeatureMask).
        Other bits describe presence of tilted HAAR features and availability of 16-bit LBP detection.

        \note This function is used for implementation of Simd::Detection.

        \param [in] data - a pointer to cascade received from ::SimdDetectionLoadA or ::SimdDetectionLoadStringXml.
        \param [out] width - a pointer to returned width of original cascade window. It can be NULL.
        \param [out] height - a pointer to returned height of original cascade window. It can be NULL.
        \param [out] flags - a pointer to returned flags with other information (see ::SimdDetectionInfoFlags). It can be NULL.
    */
    SIMD_API void SimdDetectionInfo(const void * data, size_t * width, size_t * height, SimdDetectionInfoFlags * flags);

    /*! @ingroup object_detection

        \fn void * SimdDetectionInit(const void * data, uint8_t * sum, size_t sumStride, size_t width, size_t height, uint8_t * sqsum, size_t sqsumStride, uint8_t * tilted, size_t tiltedStride, int throughColumn, int int16);

        \short Initializes a working classifier cascade context for integral images of a fixed input image size.

        The hidden context stores references to provided integral images and precomputes pointers to cascade features.
        The sum image size is also the integral image size; the corresponding source gray image has
        width - 1 by height - 1 pixels. Integral images must be calculated by ::SimdIntegral with 32-bit
        integer sum format. HAAR cascades require sum and squared sum images, and require tilted image
        only when ::SimdDetectionInfoHasTilted is set. LBP cascades use only the sum image.

        If throughColumn is non-zero, the context is prepared for the interlaced detection functions
        (*32fi and *16ii), which scan every second row and column. If int16 is non-zero and the loaded
        LBP cascade has ::SimdDetectionInfoCanInt16, the context uses a 16-bit integer LBP representation
        and must be used with *16ip or *16ii detection functions. Otherwise LBP detection uses 32-bit
        integral sums and must be used with *32fp or *32fi detection functions.

        \note This function is used for implementation of Simd::Detection.

        \param [in] data - a pointer to cascade received from ::SimdDetectionLoadA or ::SimdDetectionLoadStringXml.
        \param [in] sum - a pointer to 32-bit integer integral sum image of input 8-bit gray image.
                          See ::SimdIntegral in order to estimate this integral sum.
        \param [in] sumStride - a row size of the sum image (in bytes).
        \param [in] width - a width of the integral images. It must be one greater than width of input 8-bit gray image.
        \param [in] height - a height of the integral images. It must be one greater than height of input 8-bit gray image.
        \param [in] sqsum - a pointer to 32-bit integer squared integral sum image. It is required for HAAR cascades and ignored for LBP cascades.
        \param [in] sqsumStride - a row size of the sqsum image (in bytes).
        \param [in] tilted - a pointer to 32-bit integer tilted integral sum image. It is required only for HAAR cascades with tilted features.
        \param [in] tiltedStride - a row size of the tilted image (in bytes).
        \param [in] throughColumn - a flag to prepare context for scanning every second row and column.
        \param [in] int16 - a flag to request 16-bit integer LBP detection (see ::SimdDetectionInfoCanInt16).
        \return a pointer to hidden cascade. On error it returns NULL.
                This pointer is used in functions ::SimdDetectionPrepare, ::SimdDetectionHaarDetect32fp, ::SimdDetectionHaarDetect32fi,
                ::SimdDetectionLbpDetect32fp, ::SimdDetectionLbpDetect32fi, ::SimdDetectionLbpDetect16ip and ::SimdDetectionLbpDetect16ii.
                It must be released by function ::SimdRelease.
    */
    SIMD_API void * SimdDetectionInit(const void * data, uint8_t * sum, size_t sumStride, size_t width, size_t height,
        uint8_t * sqsum, size_t sqsumStride, uint8_t * tilted, size_t tiltedStride, int throughColumn, int int16);

    /*! @ingroup object_detection

        \fn void SimdDetectionPrepare(void * hid);

        \short Prepares hidden classifier cascade context after integral images have been updated.

        This function rebuilds internal derived buffers from current integral image data:
        through-column copies for interlaced scanning and 16-bit converted sums for integer LBP detection.
        It must be called after every update of integral images and before any call to
        ::SimdDetectionHaarDetect32fp, ::SimdDetectionHaarDetect32fi,
        ::SimdDetectionLbpDetect32fp, ::SimdDetectionLbpDetect32fi,
        ::SimdDetectionLbpDetect16ip or ::SimdDetectionLbpDetect16ii.

        \note This function is used for implementation of Simd::Detection.

        \param [in] hid - a pointer to hidden cascade received from ::SimdDetectionInit.
    */
    SIMD_API void SimdDetectionPrepare(void * hid);

    /*! @ingroup object_detection

        \fn void SimdDetectionHaarDetect32fp(const void * hid, const uint8_t * mask, size_t maskStride, ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, uint8_t * dst, size_t dstStride);

        \short Performs HAAR cascade detection with 32-bit floating-point arithmetic and scans every point.

        Use this function only with a HAAR hidden cascade initialized with throughColumn equal to 0.
        ::SimdDetectionPrepare must be called before this function. The mask and bounding box restrict
        positions of the left-top corner of the scanning window. For each point in the half-open rectangle
        [left, right) x [top, bottom), a zero mask value skips detection and a non-zero mask value allows it.
        When a window passes the cascade, the corresponding dst point is set to 1. Initialize dst before
        calling this function if a zero background is required.

        \note This function is used for implementation of Simd::Detection.

        \param [in] hid - a pointer to hidden HAAR cascade received from ::SimdDetectionInit.
        \param [in] mask - a pointer to 8-bit mask image. Its size is equal to source image size.
        \param [in] maskStride - a row size of the mask image (in bytes).
        \param [in] left - a left side of scan rectangle for window left-top corner.
        \param [in] top - a top side of scan rectangle for window left-top corner.
        \param [in] right - a right side of scan rectangle for window left-top corner.
        \param [in] bottom - a bottom side of scan rectangle for window left-top corner.
        \param [out] dst - a pointer to 8-bit output image. Points set to 1 refer to left-top corners of detected objects.
        \param [in] dstStride - a row size of the dst image (in bytes).
    */
    SIMD_API void SimdDetectionHaarDetect32fp(const void * hid, const uint8_t * mask, size_t maskStride,
        ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, uint8_t * dst, size_t dstStride);

    /*! @ingroup object_detection

        \fn void SimdDetectionHaarDetect32fi(const void * hid, const uint8_t * mask, size_t maskStride, ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, uint8_t * dst, size_t dstStride);

        \short Performs HAAR cascade detection with 32-bit floating-point arithmetic and scans every second point.

        Use this function only with a HAAR hidden cascade initialized with throughColumn not equal to 0.
        ::SimdDetectionPrepare must be called before this function. The mask and bounding box restrict
        positions of the left-top corner of the scanning window. The function checks every second row and
        column in the half-open rectangle [left, right) x [top, bottom). A zero mask value skips detection;
        a non-zero mask value allows it. When a window passes the cascade, the corresponding dst point is set
        to 1. Initialize dst before calling this function if a zero background is required.

        \note This function is used for implementation of Simd::Detection.

        \param [in] hid - a pointer to hidden HAAR cascade received from ::SimdDetectionInit.
        \param [in] mask - a pointer to 8-bit mask image. Its size is equal to source image size.
        \param [in] maskStride - a row size of the mask image (in bytes).
        \param [in] left - a left side of scan rectangle for window left-top corner.
        \param [in] top - a top side of scan rectangle for window left-top corner.
        \param [in] right - a right side of scan rectangle for window left-top corner.
        \param [in] bottom - a bottom side of scan rectangle for window left-top corner.
        \param [out] dst - a pointer to 8-bit output image. Points set to 1 refer to left-top corners of detected objects.
        \param [in] dstStride - a row size of the dst image (in bytes).
    */
    SIMD_API void SimdDetectionHaarDetect32fi(const void * hid, const uint8_t * mask, size_t maskStride,
        ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, uint8_t * dst, size_t dstStride);

    /*! @ingroup object_detection

        \fn void SimdDetectionLbpDetect32fp(const void * hid, const uint8_t * mask, size_t maskStride, ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, uint8_t * dst, size_t dstStride);

        \short Performs LBP cascade detection with 32-bit integral sums and floating-point stage weights, scanning every point.

        Use this function only with an LBP hidden cascade initialized with throughColumn equal to 0 and
        without 16-bit integer representation. ::SimdDetectionPrepare must be called before this function.
        The mask and bounding box restrict positions of the left-top corner of the scanning window. For each
        point in the half-open rectangle [left, right) x [top, bottom), a zero mask value skips detection and
        a non-zero mask value allows it. When a window passes the cascade, the corresponding dst point is set
        to 1. Initialize dst before calling this function if a zero background is required.

        \note This function is used for implementation of Simd::Detection.

        \param [in] hid - a pointer to hidden LBP cascade received from ::SimdDetectionInit.
        \param [in] mask - a pointer to 8-bit mask image. Its size is equal to source image size.
        \param [in] maskStride - a row size of the mask image (in bytes).
        \param [in] left - a left side of scan rectangle for window left-top corner.
        \param [in] top - a top side of scan rectangle for window left-top corner.
        \param [in] right - a right side of scan rectangle for window left-top corner.
        \param [in] bottom - a bottom side of scan rectangle for window left-top corner.
        \param [out] dst - a pointer to 8-bit output image. Points set to 1 refer to left-top corners of detected objects.
        \param [in] dstStride - a row size of the dst image (in bytes).
    */
    SIMD_API void SimdDetectionLbpDetect32fp(const void * hid, const uint8_t * mask, size_t maskStride,
        ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, uint8_t * dst, size_t dstStride);

    /*! @ingroup object_detection

        \fn void SimdDetectionLbpDetect32fi(const void * hid, const uint8_t * mask, size_t maskStride, ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, uint8_t * dst, size_t dstStride);

        \short Performs LBP cascade detection with 32-bit integral sums and floating-point stage weights, scanning every second point.

        Use this function only with an LBP hidden cascade initialized with throughColumn not equal to 0 and
        without 16-bit integer representation. ::SimdDetectionPrepare must be called before this function.
        The mask and bounding box restrict positions of the left-top corner of the scanning window. The function
        checks every second row and column in the half-open rectangle [left, right) x [top, bottom). A zero mask
        value skips detection; a non-zero mask value allows it. When a window passes the cascade, the corresponding
        dst point is set to 1. Initialize dst before calling this function if a zero background is required.

        \note This function is used for implementation of Simd::Detection.

        \param [in] hid - a pointer to hidden LBP cascade received from ::SimdDetectionInit.
        \param [in] mask - a pointer to 8-bit mask image. Its size is equal to source image size.
        \param [in] maskStride - a row size of the mask image (in bytes).
        \param [in] left - a left side of scan rectangle for window left-top corner.
        \param [in] top - a top side of scan rectangle for window left-top corner.
        \param [in] right - a right side of scan rectangle for window left-top corner.
        \param [in] bottom - a bottom side of scan rectangle for window left-top corner.
        \param [out] dst - a pointer to 8-bit output image. Points set to 1 refer to left-top corners of detected objects.
        \param [in] dstStride - a row size of the dst image (in bytes).
    */
    SIMD_API void SimdDetectionLbpDetect32fi(const void * hid, const uint8_t * mask, size_t maskStride,
        ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, uint8_t * dst, size_t dstStride);

    /*! @ingroup object_detection

        \fn void SimdDetectionLbpDetect16ip(const void * hid, const uint8_t * mask, size_t maskStride, ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, uint8_t * dst, size_t dstStride);

        \short Performs LBP cascade detection with 16-bit integral sums and integer stage weights, scanning every point.

        Use this function only with an LBP hidden cascade initialized with throughColumn equal to 0 and
        int16 not equal to 0. The loaded cascade must have ::SimdDetectionInfoCanInt16 set.
        ::SimdDetectionPrepare must be called before this function. The mask and bounding box restrict positions
        of the left-top corner of the scanning window. For each point in the half-open rectangle [left, right) x
        [top, bottom), a zero mask value skips detection and a non-zero mask value allows it. When a window passes
        the cascade, the corresponding dst point is set to 1. Initialize dst before calling this function if a zero
        background is required.

        \note This function is used for implementation of Simd::Detection.

        \param [in] hid - a pointer to hidden LBP cascade received from ::SimdDetectionInit.
        \param [in] mask - a pointer to 8-bit mask image. Its size is equal to source image size.
        \param [in] maskStride - a row size of the mask image (in bytes).
        \param [in] left - a left side of scan rectangle for window left-top corner.
        \param [in] top - a top side of scan rectangle for window left-top corner.
        \param [in] right - a right side of scan rectangle for window left-top corner.
        \param [in] bottom - a bottom side of scan rectangle for window left-top corner.
        \param [out] dst - a pointer to 8-bit output image. Points set to 1 refer to left-top corners of detected objects.
        \param [in] dstStride - a row size of the dst image (in bytes).
    */
    SIMD_API void SimdDetectionLbpDetect16ip(const void * hid, const uint8_t * mask, size_t maskStride,
        ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, uint8_t * dst, size_t dstStride);

    /*! @ingroup object_detection

        \fn void SimdDetectionLbpDetect16ii(const void * hid, const uint8_t * mask, size_t maskStride, ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, uint8_t * dst, size_t dstStride);

        \short Performs LBP cascade detection with 16-bit integral sums and integer stage weights, scanning every second point.

        Use this function only with an LBP hidden cascade initialized with throughColumn not equal to 0 and
        int16 not equal to 0. The loaded cascade must have ::SimdDetectionInfoCanInt16 set.
        ::SimdDetectionPrepare must be called before this function. The mask and bounding box restrict positions
        of the left-top corner of the scanning window. The function checks every second row and column in the
        half-open rectangle [left, right) x [top, bottom). A zero mask value skips detection; a non-zero mask value
        allows it. When a window passes the cascade, the corresponding dst point is set to 1. Initialize dst before
        calling this function if a zero background is required.

        \note This function is used for implementation of Simd::Detection.

        \param [in] hid - a pointer to hidden LBP cascade received from ::SimdDetectionInit.
        \param [in] mask - a pointer to 8-bit mask image. Its size is equal to source image size.
        \param [in] maskStride - a row size of the mask image (in bytes).
        \param [in] left - a left side of scan rectangle for window left-top corner.
        \param [in] top - a top side of scan rectangle for window left-top corner.
        \param [in] right - a right side of scan rectangle for window left-top corner.
        \param [in] bottom - a bottom side of scan rectangle for window left-top corner.
        \param [out] dst - a pointer to 8-bit output image. Points set to 1 refer to left-top corners of detected objects.
        \param [in] dstStride - a row size of the dst image (in bytes).
    */
    SIMD_API void SimdDetectionLbpDetect16ii(const void * hid, const uint8_t * mask, size_t maskStride,
        ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, uint8_t * dst, size_t dstStride);

    /*! @ingroup drawing

        \fn void SimdDrawLine(uint8_t* canvas, size_t stride, size_t width, size_t height, size_t channels, ptrdiff_t x1, ptrdiff_t y1, ptrdiff_t x2, ptrdiff_t y2, const uint8_t* color, size_t lineWidth);

        \short Draws a clipped line segment on an image.

        The function draws a line from (x1, y1) to (x2, y2) into canvas. Coordinates use the usual
        image coordinate system: X grows to the right and Y grows downward. The segment is clipped
        to the canvas rectangle [0, width - 1] x [0, height - 1]; if it is completely outside,
        the function does nothing. Only images with 1, 2, 3 or 4 bytes per pixel are supported.
        The color buffer must contain channels bytes. The line is drawn with the specified width
        in pixels around the rasterized segment.

        \note This function has a C++ wrapper: Simd::DrawLine(View<A> & canvas, ptrdiff_t x1, ptrdiff_t y1, ptrdiff_t x2, ptrdiff_t y2, const Color & color, size_t width = 1).

        \param [out] canvas - a pointer to pixels data of canvas image.
        \param [in] stride - a row size of canvas image (in bytes).
        \param [in] width - a width of canvas image (in pixels).
        \param [in] height - a height of canvas image (in pixels).
        \param [in] channels - a size of one canvas pixel in bytes. It must be in range [1, 4].
        \param [in] x1 - X coordinate of the first point of the line.
        \param [in] y1 - Y coordinate of the first point of the line.
        \param [in] x2 - X coordinate of the second point of the line.
        \param [in] y2 - Y coordinate of the second point of the line.
        \param [in] color - a pointer to line color. It must point to channels bytes.
        \param [in] lineWidth - a line width (in pixels).
    */
    SIMD_API void SimdDrawLine(uint8_t* canvas, size_t stride, size_t width, size_t height, size_t channels, ptrdiff_t x1, ptrdiff_t y1, ptrdiff_t x2, ptrdiff_t y2, const uint8_t* color, size_t lineWidth);

    /*! @ingroup drawing

        \fn void SimdDrawRectangle(uint8_t* canvas, size_t stride, size_t width, size_t height, size_t channels, ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, const uint8_t* color, size_t lineWidth);

        \short Draws a clipped rectangle frame on an image.

        The function draws four clipped lines: (left, top)-(right, top), (right, top)-(right, bottom),
        (right, bottom)-(left, bottom) and (left, bottom)-(left, top). Coordinates use the usual image
        coordinate system: X grows to the right and Y grows downward. The rectangle sides may be outside
        the canvas; each side is clipped by ::SimdDrawLine. Only images with 1, 2, 3 or 4 bytes per pixel
        are supported. The color buffer must contain channels bytes.

        \note This function has C++ wrappers: Simd::DrawRectangle(View<A> & canvas, ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, const Color & color, size_t width = 1),
            Simd::DrawRectangle(View<A> & canvas, const Rectangle<ptrdiff_t> & rect, const Color & color, size_t width = 1).

        \param [out] canvas - a pointer to pixels data of canvas image.
        \param [in] stride - a row size of canvas image (in bytes).
        \param [in] width - a width of canvas image (in pixels).
        \param [in] height - a height of canvas image (in pixels).
        \param [in] channels - a size of one canvas pixel in bytes. It must be in range [1, 4].
        \param [in] left - X coordinate of the left side of the rectangle.
        \param [in] top - Y coordinate of the top side of the rectangle.
        \param [in] right - X coordinate of the right side of the rectangle.
        \param [in] bottom - Y coordinate of the bottom side of the rectangle.
        \param [in] color - a pointer to rectangle color. It must point to channels bytes.
        \param [in] lineWidth - a width of rectangle frame (in pixels).
    */
    SIMD_API void SimdDrawRectangle(uint8_t* canvas, size_t stride, size_t width, size_t height, size_t channels, ptrdiff_t left, ptrdiff_t top, ptrdiff_t right, ptrdiff_t bottom, const uint8_t* color, size_t lineWidth);

    /*! @ingroup filling

        \fn void SimdFill(uint8_t * dst, size_t stride, size_t width, size_t height, size_t pixelSize, uint8_t value);

        \short Fills every byte of image pixel data with the given 8-bit value.

        For each row the function writes width*pixelSize bytes with value and then moves to the next
        row by stride bytes. Padding bytes after width*pixelSize in each row are not modified.

        \note This function has a C++ wrapper Simd::Fill(View<A>& dst, uint8_t value).

        \param [out] dst - a pointer to pixels data of destination image.
        \param [in] stride - a row size of the dst image (in bytes).
        \param [in] width - an image width (in pixels).
        \param [in] height - an image height (in pixels).
        \param [in] pixelSize - a size of one image pixel (in bytes).
        \param [in] value - a byte value to fill image pixel data.
    */
    SIMD_API void SimdFill(uint8_t * dst, size_t stride, size_t width, size_t height, size_t pixelSize, uint8_t value);

    /*! @ingroup filling

        \fn void SimdFillFrame(uint8_t * dst, size_t stride, size_t width, size_t height, size_t pixelSize, size_t frameLeft, size_t frameTop, size_t frameRight, size_t frameBottom, uint8_t value);

        \short Fills image pixel data outside of the given inner frame with the given 8-bit value.

        The function fills four areas: rows above frameTop, rows below frameBottom, columns before
        frameLeft inside frame vertical range, and columns after frameRight inside frame vertical range.
        The rectangle [frameLeft, frameRight) x [frameTop, frameBottom) is left unchanged.
        Frame coordinates must satisfy frameLeft <= frameRight <= width and frameTop <= frameBottom <= height.

        \note This function has a C++ wrapper Simd::FillFrame(View<A>& dst, const Rectangle<ptrdiff_t> & frame, uint8_t value).

        \param [out] dst - a pointer to pixels data of destination image.
        \param [in] stride - a row size of the dst image (in bytes).
        \param [in] width - an image width (in pixels).
        \param [in] height - an image height (in pixels).
        \param [in] pixelSize - a size of one image pixel (in bytes).
        \param [in] frameLeft - a left side of the inner frame.
        \param [in] frameTop - a top side of the inner frame.
        \param [in] frameRight - a right side of the inner frame.
        \param [in] frameBottom - a bottom side of the inner frame.
        \param [in] value - a byte value to fill image pixel data outside of the frame.
    */
    SIMD_API void SimdFillFrame(uint8_t * dst, size_t stride, size_t width, size_t height, size_t pixelSize,
        size_t frameLeft, size_t frameTop, size_t frameRight, size_t frameBottom, uint8_t value);

    /*! @ingroup filling

        \fn void SimdFillBgr(uint8_t * dst, size_t stride, size_t width, size_t height, uint8_t blue, uint8_t green, uint8_t red);

        \short Fills every pixel of a 24-bit BGR image with the given color.

        For every output pixel: dst[0] = blue, dst[1] = green, dst[2] = red.
        Padding bytes after width*3 in each row are not modified.

        \note This function has a C++ wrapper Simd::FillBgr(View<A>& dst, uint8_t blue, uint8_t green, uint8_t red).

        \param [out] dst - a pointer to pixels data of destination image.
        \param [in] stride - a row size of the dst image (in bytes).
        \param [in] width - an image width (in pixels).
        \param [in] height - an image height (in pixels).
        \param [in] blue - a blue channel value of BGR color.
        \param [in] green - a green channel value of BGR color.
        \param [in] red - a red channel value of BGR color.
    */
    SIMD_API void SimdFillBgr(uint8_t * dst, size_t stride, size_t width, size_t height, uint8_t blue, uint8_t green, uint8_t red);

    /*! @ingroup filling

        \fn void SimdFillBgra(uint8_t * dst, size_t stride, size_t width, size_t height, uint8_t blue, uint8_t green, uint8_t red, uint8_t alpha);

        \short Fills every pixel of a 32-bit BGRA image with the given color.

        For every output pixel: dst[0] = blue, dst[1] = green, dst[2] = red, dst[3] = alpha.
        Padding bytes after width*4 in each row are not modified.

        \note This function has a C++ wrapper Simd::FillBgra(View<A>& dst, uint8_t blue, uint8_t green, uint8_t red, uint8_t alpha).

        \param [out] dst - a pointer to pixels data of destination image.
        \param [in] stride - a row size of the dst image (in bytes).
        \param [in] width - an image width (in pixels).
        \param [in] height - an image height (in pixels).
        \param [in] blue - a blue channel value of BGRA color.
        \param [in] green - a green channel value of BGRA color.
        \param [in] red - a red channel value of BGRA color.
        \param [in] alpha - an alpha channel value of BGRA color.
    */
    SIMD_API void SimdFillBgra(uint8_t * dst, size_t stride, size_t width, size_t height,
        uint8_t blue, uint8_t green, uint8_t red, uint8_t alpha);

    /*! @ingroup filling

        \fn void SimdFillPixel(uint8_t * dst, size_t stride, size_t width, size_t height, const uint8_t * pixel, size_t pixelSize);

        \short Fills every image pixel with the given pixel value.

        The function supports pixel sizes from 1 to 4 bytes. For pixelSize equal to 1, 2, 3 or 4
        it fills the image as 8-bit gray, 16-bit two-channel, 24-bit BGR or 32-bit BGRA data
        respectively. Padding bytes after width*pixelSize in each row are not modified.

        \note This function has a C++ wrapper Simd::FillPixel(View<A> & dst, const Pixel & pixel).

        \param [out] dst - a pointer to pixels data of destination image.
        \param [in] stride - a row size of the dst image (in bytes).
        \param [in] width - an image width (in pixels).
        \param [in] height - an image height (in pixels).
        \param [in] pixel - a pointer to pixel value to fill image. It must point to pixelSize bytes.
        \param [in] pixelSize - a size of one image pixel (in bytes). It must be in range [1, 4].
    */
    SIMD_API void SimdFillPixel(uint8_t * dst, size_t stride, size_t width, size_t height, const uint8_t * pixel, size_t pixelSize);

    /*! @ingroup filling

        \fn void SimdFill32f(float * dst, size_t size, const float * value);

        \short Fills a 32-bit float array with the given value.

        If value is NULL or value[0] is equal to 0.0, the function fills dst with zeros.
        Otherwise every dst element is set to value[0].

        \param [out] dst - a pointer to 32-bit float array.
        \param [in] size - a number of elements in the array.
        \param [in] value - a pointer to value to fill. It can be NULL; in this case filling value is assumed to be zero.
    */
    SIMD_API void SimdFill32f(float * dst, size_t size, const float * value);

    /*! @ingroup bfloat16

        \fn void SimdFloat32ToBFloat16(const float * src, size_t size, uint16_t * dst);

        \short Converts an array of 32-bit floats to 16-bit bfloat16 values.

        For each element the function stores the bfloat16 representation of src[i] to dst[i].
        The bfloat16 value contains the high 16 bits of IEEE 754 binary32 after rounding the
        discarded low 16 bits to nearest-even.

        \param [in] src - a pointer to the input array with 32-bit float point numbers.
        \param [in] size - a number of elements in input and output arrays.
        \param [out] dst - a pointer to the output array with 16-bit bfloat16 values.
    */
    SIMD_API void SimdFloat32ToBFloat16(const float* src, size_t size, uint16_t* dst);

    /*! @ingroup bfloat16

        \fn void SimdBFloat16ToFloat32(const uint16_t* src, size_t size, float  * dst);

        \short Converts an array of 16-bit bfloat16 values to 32-bit floats.

        For each element the function expands src[i] to IEEE 754 binary32 by placing the bfloat16
        bits into the high 16 bits of the result and setting the low 16 bits to zero.

        \param [in] src - a pointer to the input array with 16-bit bfloat16 values.
        \param [in] size - a number of elements in input and output arrays.
        \param [out] dst - a pointer to the output array with 32-bit float point numbers.
    */
    SIMD_API void SimdBFloat16ToFloat32(const uint16_t* src, size_t size, float* dst);

    /*! @ingroup float16

        \fn void SimdFloat32ToFloat16(const float * src, size_t size, uint16_t * dst);

        \short Converts an array of 32-bit floats to 16-bit float values.

        For each element the function stores the IEEE 754 binary16 representation of src[i] to dst[i].
        The conversion handles sign, normal values, subnormal values, infinities and NaNs according
        to the internal half-precision conversion helper.

        \param [in] src - a pointer to the input array with 32-bit float point numbers.
        \param [in] size - a number of elements in input and output arrays.
        \param [out] dst - a pointer to the output array with 16-bit float point numbers.
    */
    SIMD_API void SimdFloat32ToFloat16(const float * src, size_t size, uint16_t * dst);

    /*! @ingroup float16

        \fn void SimdFloat16ToFloat32(const uint16_t* src, size_t size, float  * dst);

        \short Converts an array of 16-bit float values to 32-bit floats.

        For each element the function expands the IEEE 754 binary16 value src[i] to a 32-bit
        float value dst[i], including normal values, subnormal values, infinities and NaNs.

        \param [in] src - a pointer to the input array with 16-bit float point numbers.
        \param [in] size - a number of elements in input and output arrays.
        \param [out] dst - a pointer to the output array with 32-bit float point numbers.
    */
    SIMD_API void SimdFloat16ToFloat32(const uint16_t * src, size_t size, float * dst);

    /*! @ingroup drawing

        \fn void* SimdFontInit();

        \short Creates a font context with embedded ASCII glyph data.

        The context stores a built-in monospace-like font and is used by ::SimdFontResize,
        ::SimdFontHeight, ::SimdFontMeasure and ::SimdFontDraw. Call ::SimdFontResize to choose
        a drawable font height before measuring or drawing text.

        \return a pointer to font context. On error it returns NULL.
                This pointer is used in functions ::SimdFontResize, ::SimdFontHeight, ::SimdFontMeasure and ::SimdFontDraw.
                It must be released by function ::SimdRelease.
    */
    SIMD_API void* SimdFontInit();

    /*! @ingroup drawing

        \fn SimdBool SimdFontResize(void * context, size_t height);

        \short Resizes the font context to the given glyph height.

        The function recreates internal 8-bit alpha glyph images from embedded font data.
        It returns ::SimdFalse if height is outside the supported range of the embedded font.
        Reusing the current height is a successful no-op.

        \param [in] context - a font context. It must be created by ::SimdFontInit and released by ::SimdRelease.
        \param [in] height - a new glyph height in pixels.
        \return ::SimdTrue on success and ::SimdFalse on failure.
    */
    SIMD_API SimdBool SimdFontResize(void * context, size_t height);

    /*! @ingroup drawing

        \fn size_t SimdFontHeight(void* context);

        \short Gets current glyph height of the font context.

        \param [in] context - a font context. It must be created by ::SimdFontInit and released by ::SimdRelease.
        \return the current glyph height in pixels.
    */
    SIMD_API size_t SimdFontHeight(void* context);

    /*! @ingroup drawing

        \fn void SimdFontMeasure(void* context, const char* text, size_t* width, size_t* height);

        \short Measures the rectangle required to draw a zero-terminated text string.

        The embedded font supports ASCII glyphs from the built-in font table. Supported glyphs advance
        the current X position by current glyph width. The '\n' character starts a new line and advances
        Y by current glyph height. Unsupported characters are ignored. If the text contains at least one
        drawable glyph, the returned size also includes the font indentation on all sides. The width and
        height output pointers are optional.

        \param [in] context - a font context. It must be created by ::SimdFontInit and released by ::SimdRelease.
        \param [in] text - a pointer to zero-terminated text string.
        \param [out] width - a pointer to measured text region width in pixels. It can be NULL.
        \param [out] height - a pointer to measured text region height in pixels. It can be NULL.
    */
    SIMD_API void SimdFontMeasure(void* context, const char* text, size_t* width, size_t* height);

    /*! @ingroup drawing

        \fn void SimdFontDraw(void* context, uint8_t* canvas, size_t stride, size_t width, size_t height, size_t channels, const char* text, size_t left, size_t top, const uint8_t* color);

        \short Draws a zero-terminated text string on an 8-bit-per-channel image.

        The function creates an 8-bit alpha mask from supported glyphs and blends color into canvas
        through this mask by ::SimdAlphaFilling. The text position (left, top) specifies the top-left
        corner of the measured text region; glyphs are shifted by the current font indentation inside it.
        Drawing is clipped to the canvas. Supported glyphs advance X by current glyph width, '\n' starts
        a new line, and unsupported characters are ignored. The canvas must have 1, 2, 3 or 4 channels.

        \param [in] context - a font context. It must be created by ::SimdFontInit and released by ::SimdRelease.
        \param [out] canvas - a pointer to pixels data of canvas image.
        \param [in] stride - a row size of canvas image (in bytes).
        \param [in] width - a width of canvas image (in pixels).
        \param [in] height - a height of canvas image (in pixels).
        \param [in] channels - a number of 8-bit channels in canvas image. It must be in range [1, 4].
        \param [in] text - a pointer to zero-terminated text string.
        \param [in] left - X coordinate of the measured text region left side.
        \param [in] top - Y coordinate of the measured text region top side.
        \param [in] color - a pointer to text color. It must point to channels bytes.
    */
    SIMD_API void SimdFontDraw(void* context, uint8_t* canvas, size_t stride, size_t width, size_t height, size_t channels, const char* text, size_t left, size_t top, const uint8_t* color);

    /*! @ingroup float16

        \fn void SimdSquaredDifferenceSum16f(const uint16_t * a, const uint16_t * b, size_t size, float * sum);

        \short Calculates sum of squared differences for two 16-bit float arrays.

        The input values are IEEE 754 binary16 values stored in uint16_t elements. Each element is
        converted to 32-bit float before subtraction and accumulation. Input arrays must have the same size.

        Algorithm description:
        \verbatim
        da = Float16ToFloat32(a[i]) - Float16ToFloat32(b[i]);
        sum[0] = Sum(da*da);
        \endverbatim

        \param [in] a - a pointer to the first 16-bit float array.
        \param [in] b - a pointer to the second 16-bit float array.
        \param [in] size - a number of elements in input arrays.
        \param [out] sum - a pointer to 32-bit float sum of squared differences.
    */
    SIMD_API void SimdSquaredDifferenceSum16f(const uint16_t * a, const uint16_t * b, size_t size, float * sum);

    /*! @ingroup float16

        \fn void SimdCosineDistance16f(const uint16_t * a, const uint16_t * b, size_t size, float * distance);

        \short Calculates cosine distance of two 16-bit float arrays.

        The input values are IEEE 754 binary16 values stored in uint16_t elements. Each element is
        converted to 32-bit float before multiplication and accumulation. Input arrays must have the same size
        and non-zero Euclidean norm.

        Algorithm description:
        \verbatim
        fa = Float16ToFloat32(a[i]);
        fb = Float16ToFloat32(b[i]);
        distance[0] = 1 - Sum(fa*fb)/Sqrt(Sum(fa*fa)*Sum(fb*fb));
        \endverbatim

        \param [in] a - a pointer to the first 16-bit float array.
        \param [in] b - a pointer to the second 16-bit float array.
        \param [in] size - a number of elements in input arrays.
        \param [out] distance - a pointer to 32-bit float cosine distance.
    */
    SIMD_API void SimdCosineDistance16f(const uint16_t * a, const uint16_t * b, size_t size, float * distance);

    /*! @ingroup float16

        \fn void SimdCosineDistancesMxNa16f(size_t M, size_t N, size_t K, const uint16_t * const * A, const uint16_t * const * B, float * distances);

        \short Calculates pairwise cosine distances for two sets of 16-bit float vectors.

        A is an array of M pointers to vectors of length K, and B is an array of N pointers to vectors
        of length K. The input values are IEEE 754 binary16 values stored in uint16_t elements and are
        converted to 32-bit float for accumulation. Every input vector is expected to have non-zero
        Euclidean norm. The output matrix is stored in row-major order.

        Algorithm description:
        \verbatim
        distances[i*N + j] = SimdCosineDistance16f(A[i], B[j], K);
        \endverbatim

        \param [in] M - a number of A arrays.
        \param [in] N - a number of B arrays.
        \param [in] K - a number of elements in every A and B vector.
        \param [in] A - a pointer to the first array with M pointers to 16-bit float vectors.
        \param [in] B - a pointer to the second array with N pointers to 16-bit float vectors.
        \param [out] distances - a pointer to result 32-bit float array with row-major cosine distance matrix. Its size must be M*N.
    */
    SIMD_API void SimdCosineDistancesMxNa16f(size_t M, size_t N, size_t K, const uint16_t * const * A, const uint16_t * const * B, float * distances);

    /*! @ingroup float16

        \fn void SimdCosineDistancesMxNp16f(size_t M, size_t N, size_t K, const uint16_t* A, const uint16_t* B, float* distances);

        \short Calculates pairwise cosine distances for two packed sets of 16-bit float vectors.

        A contains M contiguous vectors of length K and B contains N contiguous vectors of length K.
        The input values are IEEE 754 binary16 values stored in uint16_t elements and are converted
        to 32-bit float for accumulation. Every input vector is expected to have non-zero Euclidean norm.
        The output matrix is stored in row-major order.

        Algorithm description:
        \verbatim
        distances[i*N + j] = SimdCosineDistance16f(A + i*K, B + j*K, K);
        \endverbatim

        \param [in] M - a number of A arrays.
        \param [in] N - a number of B arrays.
        \param [in] K - a number of elements in every A and B vector.
        \param [in] A - a pointer to M packed 16-bit float vectors.
        \param [in] B - a pointer to N packed 16-bit float vectors.
        \param [out] distances - a pointer to result 32-bit float array with row-major cosine distance matrix. Its size must be M*N.
    */
    SIMD_API void SimdCosineDistancesMxNp16f(size_t M, size_t N, size_t K, const uint16_t* A, const uint16_t* B, float* distances);

    /*! @ingroup float16

        \fn void SimdVectorNormNa16f(size_t N, size_t K, const uint16_t* const* A, float* norms);

        \short Calculates Euclidean norms for an array of 16-bit float vectors.

        A is an array of N pointers to vectors of length K. The input values are IEEE 754 binary16
        values stored in uint16_t elements and are converted to 32-bit float before accumulation.

        Algorithm description:
        \verbatim
        fa = Float16ToFloat32(A[j][k]);
        norms[j] = Sqrt(Sum(fa*fa));
        \endverbatim

        \param [in] N - a number of A vectors.
        \param [in] K - a number of elements in every A vector.
        \param [in] A - a pointer to an array with N pointers to 16-bit float vectors.
        \param [out] norms - a pointer to result 32-bit float array with vector norms. Its size must be N.
    */
    SIMD_API void SimdVectorNormNa16f(size_t N, size_t K, const uint16_t* const* A, float* norms);

    /*! @ingroup float16

        \fn void SimdVectorNormNp16f(size_t N, size_t K, const uint16_t* A, float* norms);

        \short Calculates Euclidean norms for a packed array of 16-bit float vectors.

        A contains N contiguous vectors of length K. The input values are IEEE 754 binary16 values
        stored in uint16_t elements and are converted to 32-bit float before accumulation.

        Algorithm description:
        \verbatim
        fa = Float16ToFloat32(A[j*K + k]);
        norms[j] = Sqrt(Sum(fa*fa));
        \endverbatim

        \param [in] N - a number of A vectors.
        \param [in] K - a number of elements in every A vector.
        \param [in] A - a pointer to N packed 16-bit float vectors.
        \param [out] norms - a pointer to result 32-bit float array with vector norms. Its size must be N.
    */
    SIMD_API void SimdVectorNormNp16f(size_t N, size_t K, const uint16_t* A, float* norms);

    /*! @ingroup other_conversion

        \fn void SimdFloat32ToUint8(const float * src, size_t size, const float * lower, const float * upper, uint8_t * dst);

        \short Converts an array of 32-bit floats to 8-bit unsigned integers with linear saturation.

        lower and upper point to scalar bounds. Each source value is saturated to [lower[0], upper[0]],
        shifted by lower[0], scaled to [0, 255], and stored as uint8_t. The upper bound must be greater
        than the lower bound.

        For every element:
        \verbatim
        dst[i] = uint8_t((Min(Max(src[i], lower[0]), upper[0]) - lower[0])*255/(upper[0] - lower[0]));
        \endverbatim

        \param [in] src - a pointer to the input array with 32-bit float point numbers.
        \param [in] size - a number of elements in input and output arrays.
        \param [in] lower - a pointer to lower saturated bound of the input array.
        \param [in] upper - a pointer to upper saturated bound of the input array.
        \param [out] dst - a pointer to the output array with 8-bit unsigned integer numbers.
    */
    SIMD_API void SimdFloat32ToUint8(const float * src, size_t size, const float * lower, const float * upper, uint8_t * dst);

    /*! @ingroup other_conversion

        \fn void SimdUint8ToFloat32(const uint8_t* src, size_t size, const float * lower, const float * upper, float * dst);

        \short Converts an array of 8-bit unsigned integers to 32-bit floats with linear scaling.

        lower and upper point to scalar bounds. Each source value is scaled from [0, 255] to
        [lower[0], upper[0]]. The upper bound must be greater than the lower bound.

        For every element:
        \verbatim
        dst[i] = src[i]*(upper[0] - lower[0])/255 + lower[0];
        \endverbatim

        \param [in] src - a pointer to the input array with 8-bit unsigned integer numbers.
        \param [in] size - a number of elements in input and output arrays.
        \param [in] lower - a pointer to lower bound of the output array.
        \param [in] upper - a pointer to upper bound of the output array.
        \param [out] dst - a pointer to the output array with 32-bit float point numbers.
    */
    SIMD_API void SimdUint8ToFloat32(const uint8_t * src, size_t size, const float * lower, const float * upper, float * dst);

    /*! @ingroup correlation

        \fn void SimdCosineDistance32f(const float * a, const float * b, size_t size, float * distance);

        \short Calculates the cosine distance between two 32-bit floating-point vectors.

        The input vectors must contain size elements. The function writes one scalar result:
        \verbatim
        aa = Sum(a[i]*a[i]);
        ab = Sum(a[i]*b[i]);
        bb = Sum(b[i]*b[i]);
        distance[0] = 1 - ab/Sqrt(aa*bb);
        \endverbatim

        Both input vectors have to have non-zero Euclidean norm.

        \param [in] a - a pointer to the first 32-bit float array.
        \param [in] b - a pointer to the second 32-bit float array.
        \param [in] size - a number of elements in both input arrays.
        \param [out] distance - a pointer to 32-bit float with the cosine distance.
    */
    SIMD_API void SimdCosineDistance32f(const float * a, const float * b, size_t size, float * distance);

    /*! @ingroup gaussian_filter

        \fn void SimdGaussianBlur3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride);

        \short Performs 3x3 Gaussian blur for an 8-bit interleaved image.

        The function applies the same separable 3x3 kernel to every channel independently. For every
        channel c of pixel (x, y):
        \verbatim
        sx0 = Max(x - 1, 0);
        sx1 = x;
        sx2 = Min(x + 1, width - 1);
        sy0 = Max(y - 1, 0);
        sy1 = y;
        sy2 = Min(y + 1, height - 1);

        dst[x, y, c] = (src[sx0, sy0, c] + 2*src[sx1, sy0, c] + src[sx2, sy0, c] +
                      2*(src[sx0, sy1, c] + 2*src[sx1, sy1, c] + src[sx2, sy1, c]) +
                         src[sx0, sy2, c] + 2*src[sx1, sy2, c] + src[sx2, sy2, c] + 8) / 16;
        \endverbatim

        The source and destination images must have the same width, height and number of interleaved
        8-bit channels. Valid channel counts are 1, 2, 3 and 4.

        \note This function has a C++ wrapper Simd::GaussianBlur3x3(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of source image.
        \param [in] srcStride - a row size of the src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of destination image.
        \param [in] dstStride - a row size of the dst image (in bytes).
    */
    SIMD_API void SimdGaussianBlur3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        size_t channelCount, uint8_t * dst, size_t dstStride);

    /*! @ingroup gaussian_filter

        \fn void * SimdGaussianBlurInit(size_t width, size_t height, size_t channels, const float * sigma, const float* epsilon);

        \short Creates a context for separable Gaussian blurring of an 8-bit interleaved image.

        The context stores image parameters and normalized 1D Gaussian coefficients used by
        ::SimdGaussianBlurRun:
        \verbatim
        half = floor(sqrt(-log(epsilon[0])) * sigma[0]);
        kernel = 2*half + 1;
        weight[kernel];

        for(x = -half; x <= half; ++x)
            weight[x + half] = exp(-Square(x/sigma[0])/2);

        sum = 0;
        for(x = -half; x <= half; ++x)
            sum += weight[x + half];

        for(x = -half; x <= half; ++x)
            weight[x + half] /= sum;
        \endverbatim

        \param [in] width - a width of input and output image.
        \param [in] height - a height of input and output image.
        \param [in] channels - a number of 8-bit channels per pixel. Its value must be in range [1..4].
        \param [in] sigma - a pointer to sigma parameter (blur radius). Its value must be greater than or equal to 0.000001.
        \param [in] epsilon - a pointer to epsilon parameter (permissible relative error).
                              Its value must be in range [0.000001..1.0]. Pointer can be NULL and by default value 0.001 is used.
        \return a pointer to filter context. On error it returns NULL.
                This pointer is used by ::SimdGaussianBlurRun and must be released by ::SimdRelease.
    */
    SIMD_API void* SimdGaussianBlurInit(size_t width, size_t height, size_t channels, const float * sigma, const float* epsilon);

    /*! @ingroup gaussian_filter

        \fn void SimdGaussianBlurRun(const void* filter, const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);

        \short Performs image Gaussian blurring with a context created by ::SimdGaussianBlurInit.

        The function applies the context's normalized 1D kernel horizontally and vertically to every
        channel independently. Border pixels are handled by nearest-pixel replication:
        \verbatim
        sum = 0;
        for(y = -half; y <= half; ++y)
        {
            sy = Min(Max(0, dy + y), height - 1);
            for(x = -half; x <= half; ++x)
            {
                sx = Min(Max(0, dx + x), width - 1);
                sum += src[sx, sy, c]*weight[x + half]*weight[y + half];
            }
        }
        dst[dx, dy, c] = Round(sum);
        \endverbatim

        \param [in] filter - a filter context. It must be created by function ::SimdGaussianBlurInit and released by function ::SimdRelease.
        \param [in] src - a pointer to pixels data of the original input image.
        \param [in] srcStride - a row size (in bytes) of the input image.
        \param [out] dst - a pointer to pixels data of the filtered output image.
        \param [in] dstStride - a row size (in bytes) of the output image.
    */
    SIMD_API void SimdGaussianBlurRun(const void* filter, const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);

    /*! @ingroup matrix

        \fn void SimdGemm32fNN(size_t M, size_t N, size_t K, const float * alpha, const float * A, size_t lda, const float * B, size_t ldb, const float * beta, float * C, size_t ldc);

        \short Performs general matrix multiplication for row-major 32-bit floating-point matrices.

        A and B are used without transposition:
        \verbatim
        for(i = 0; i < M; ++i)
            for(j = 0; j < N; ++j)
                C[i*ldc + j] = alpha[0]*Sum(A[i*lda + k]*B[k*ldb + j]) + beta[0]*C[i*ldc + j];
        \endverbatim

        \note This function supports multithreading (See functions ::SimdGetThreadNumber and ::SimdSetThreadNumber).

        \param [in] M - a height of A and height of C matrices.
        \param [in] N - a width of B and width of C matrices.
        \param [in] K - a width of A and height of B matrices.
        \param [in] alpha - a pointer to scalar multiplier of A*B.
        \param [in] A - a pointer to input A matrix.
        \param [in] lda - a row stride of A matrix (in 32-bit floats).
        \param [in] B - a pointer to input B matrix.
        \param [in] ldb - a row stride of B matrix (in 32-bit floats).
        \param [in] beta - a pointer to scalar multiplier of the original C matrix.
        \param [out] C - a pointer to input/output C matrix.
        \param [in] ldc - a row stride of C matrix (in 32-bit floats).
    */
    SIMD_API void SimdGemm32fNN(size_t M, size_t N, size_t K, const float * alpha, const float * A, size_t lda, const float * B, size_t ldb, const float * beta, float * C, size_t ldc);

    /*! @ingroup matrix

        \fn void SimdGemm32fNT(size_t M, size_t N, size_t K, const float * alpha, const float * A, size_t lda, const float * B, size_t ldb, const float * beta, float * C, size_t ldc);

        \short Performs general matrix multiplication with transposed B for row-major 32-bit floating-point matrices.

        A is an M by K row-major matrix. B is stored as an N by K row-major matrix and is used as
        Trans(B) in the multiplication:
        \verbatim
        for(i = 0; i < M; ++i)
            for(j = 0; j < N; ++j)
                C[i*ldc + j] = alpha[0]*Sum(A[i*lda + k]*B[j*ldb + k]) + beta[0]*C[i*ldc + j];
        \endverbatim

        \note This function supports multithreading (See functions ::SimdGetThreadNumber and ::SimdSetThreadNumber).

        \param [in] M - a height of A and height of C matrices.
        \param [in] N - a height of B and width of C matrices.
        \param [in] K - a width of A and width of B matrices.
        \param [in] alpha - a pointer to scalar multiplier of A*Trans(B).
        \param [in] A - a pointer to input A matrix.
        \param [in] lda - a row stride of A matrix (in 32-bit floats).
        \param [in] B - a pointer to input B matrix.
        \param [in] ldb - a row stride of B matrix (in 32-bit floats).
        \param [in] beta - a pointer to scalar multiplier of the original C matrix.
        \param [out] C - a pointer to input/output C matrix.
        \param [in] ldc - a row stride of C matrix (in 32-bit floats).
    */
    SIMD_API void SimdGemm32fNT(size_t M, size_t N, size_t K, const float * alpha, const float * A, size_t lda, const float * B, size_t ldb, const float * beta, float * C, size_t ldc);

    /*! @ingroup gray_conversion

        \fn void SimdGrayToBgr(const uint8_t * gray, size_t width, size_t height, size_t grayStride, uint8_t * bgr, size_t bgrStride);

        \short Converts an 8-bit gray image to a 24-bit BGR image.

        For every pixel:
        \verbatim
        bgr[3*x + 0] = gray[x];
        bgr[3*x + 1] = gray[x];
        bgr[3*x + 2] = gray[x];
        \endverbatim

        Since all color channels receive the same value, the function can also be used for gray to
        RGB conversion.

        All images must have the same width and height.

        \note This function has C++ wrappers: Simd::GrayToBgr(const View<A>& gray, View<A>& bgr) 
            and Simd::GrayToRgb(const View<A>& gray, View<A>& rgb).

        \param [in] gray - a pointer to pixels data of input 8-bit gray image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] grayStride - a row size of the gray image (in bytes).
        \param [out] bgr - a pointer to pixels data of output 24-bit BGR (or 24-bit RGB) image.
        \param [in] bgrStride - a row size of the bgr image (in bytes).
    */
    SIMD_API void SimdGrayToBgr(const uint8_t *gray, size_t width, size_t height, size_t grayStride, uint8_t *bgr, size_t bgrStride);

    /*! @ingroup gray_conversion

        \fn void SimdGrayToBgra(const uint8_t * gray, size_t width, size_t height, size_t grayStride, uint8_t * bgra, size_t bgraStride, uint8_t alpha);

        \short Converts an 8-bit gray image to a 32-bit BGRA image.

        For every pixel:
        \verbatim
        bgra[4*x + 0] = gray[x];
        bgra[4*x + 1] = gray[x];
        bgra[4*x + 2] = gray[x];
        bgra[4*x + 3] = alpha;
        \endverbatim

        Since all color channels receive the same value, the function can also be used for gray to
        RGBA conversion.

        All images must have the same width and height.

        \note This function has C++ wrappers: Simd::GrayToBgra(const View<A>& gray, View<A>& bgra, uint8_t alpha) 
            and Simd::GrayToRgba(const View<A>& gray, View<A>& rgba, uint8_t alpha).

        \param [in] gray - a pointer to pixels data of input 8-bit gray image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] grayStride - a row size of the gray image (in bytes).
        \param [out] bgra - a pointer to pixels data of output 32-bit BGRA (or 32-bit RGBA) image.
        \param [in] bgraStride - a row size of the bgra image (in bytes).
        \param [in] alpha - a value of the alpha channel.
    */
    SIMD_API void SimdGrayToBgra(const uint8_t *gray, size_t width, size_t height, size_t grayStride,
        uint8_t *bgra, size_t bgraStride, uint8_t alpha);

    /*! @ingroup gray_conversion

        \fn void SimdGrayToY(const uint8_t* gray, size_t grayStride, size_t width, size_t height, uint8_t* y, size_t yStride);

        \short Converts an 8-bit full-range gray image to an 8-bit limited-range Y plane.

        For every pixel:
        \verbatim
        y[x] = RestrictRange(((220*gray[x] + 128) >> 8) + 16, 16, 235);
        \endverbatim

        Thus gray value 0 maps to Y value 16, and gray value 255 maps to Y value 235.

        All images must have the same width and height.

        \note This function has C++ wrappers: Simd::GrayToY(const View<A>& gray, View<A>& y).

        \param [in] gray - a pointer to pixels data of input 8-bit gray image.
        \param [in] grayStride - a row size of the gray image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] y - a pointer to pixels data of output 8-bit Y plane.
        \param [in] yStride - a row size of the y image (in bytes).
    */
    SIMD_API void SimdGrayToY(const uint8_t* gray, size_t grayStride, size_t width, size_t height, uint8_t* y, size_t yStride);

    /*! @ingroup histogram

        \fn void SimdAbsSecondDerivativeHistogram(const uint8_t * src, size_t width, size_t height, size_t stride, size_t step, size_t indent, uint32_t * histogram);

        \short Calculates a histogram of second-derivative magnitudes for an 8-bit gray image.

        The function clears histogram and processes only pixels inside the rectangle without the
        indent-pixel border. For every processed pixel:
        \verbatim
        avgX = (src[x - step, y] + src[x + step, y] + 1) / 2;
        avgY = (src[x, y - step] + src[x, y + step] + 1) / 2;
        dx = Abs(src[x, y] - avgX);
        dy = Abs(src[x, y] - avgY);
        histogram[Max(dx, dy)]++;
        \endverbatim

        The output histogram has 256 bins and is overwritten. The parameters must satisfy:
        width > 2*indent, height > 2*indent and indent >= step.

        \note This function has a C++ wrapper Simd::AbsSecondDerivativeHistogram(const View<A>& src, size_t step, size_t indent, uint32_t * histogram).

        \param [in] src - a pointer to pixels data of input 8-bit gray image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] stride - a row size of the image (in bytes).
        \param [in] step - an offset in pixels for second-derivative calculation.
        \param [in] indent - a number of pixels skipped at every image boundary.
        \param [out] histogram - a pointer to the output histogram (array of 256 unsigned 32-bit values).
    */
    SIMD_API void SimdAbsSecondDerivativeHistogram(const uint8_t * src, size_t width, size_t height, size_t stride,
        size_t step, size_t indent, uint32_t * histogram);

    /*! @ingroup histogram

        \fn void SimdHistogram(const uint8_t * src, size_t width, size_t height, size_t stride, uint32_t * histogram);

        \short Calculates a histogram for an 8-bit gray image.

        The function clears histogram and then counts every pixel:
        \verbatim
        for(y = 0; y < height; ++y)
            for(x = 0; x < width; ++x)
                histogram[src[x, y]]++;
        \endverbatim

        The output histogram has 256 bins and is overwritten.

        \note This function has a C++ wrapper Simd::Histogram(const View<A>& src, uint32_t * histogram).

        \param [in] src - a pointer to pixels data of input 8-bit gray image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] stride - a row size of the image (in bytes).
        \param [out] histogram - a pointer to the output histogram (array of 256 unsigned 32-bit values).
    */
    SIMD_API void SimdHistogram(const uint8_t * src, size_t width, size_t height, size_t stride, uint32_t * histogram);

    /*! @ingroup histogram

        \fn void SimdHistogramMasked(const uint8_t * src, size_t srcStride, size_t width, size_t height, const uint8_t * mask, size_t maskStride, uint8_t index, uint32_t * histogram);

        \short Calculates a masked histogram for an 8-bit gray image.

        The function clears histogram and counts only source pixels whose mask value is equal to
        index:
        \verbatim
        for(y = 0; y < height; ++y)
            for(x = 0; x < width; ++x)
                if(mask[x, y] == index)
                    histogram[src[x, y]]++;
        \endverbatim

        The output histogram has 256 bins and is overwritten. The input image and mask must have the
        same width and height.

        \note This function has a C++ wrapper Simd::HistogramMasked(const View<A> & src, const View<A> & mask, uint8_t index, uint32_t * histogram).

        \param [in] src - a pointer to pixels data of input 8-bit gray image.
        \param [in] srcStride - a row size of the image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] mask - a pointer to pixels data of the mask 8-bit image.
        \param [in] maskStride - a row size of the mask image (in bytes).
        \param [in] index - a mask value selecting pixels to count.
        \param [out] histogram - a pointer to the output histogram (array of 256 unsigned 32-bit values).
    */
    SIMD_API void SimdHistogramMasked(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        const uint8_t * mask, size_t maskStride, uint8_t index, uint32_t * histogram);

    /*! @ingroup histogram

        \fn void SimdHistogramConditional(const uint8_t * src, size_t srcStride, size_t width, size_t height, const uint8_t * mask, size_t maskStride, uint8_t value, SimdCompareType compareType, uint32_t * histogram);

        \short Calculates a conditional masked histogram for an 8-bit gray image.

        The function clears histogram and counts only source pixels whose mask value satisfies the
        comparison with value:
        \verbatim
        for(y = 0; y < height; ++y)
            for(x = 0; x < width; ++x)
                if(Compare(mask[x, y], value, compareType))
                    histogram[src[x, y]]++;
        \endverbatim

        The output histogram has 256 bins and is overwritten. The input image and mask must have the
        same width and height.

        \note This function has a C++ wrapper Simd::HistogramConditional(const View<A>& src, const View<A>& mask, uint8_t value, SimdCompareType compareType, uint32_t * histogram).

        \param [in] src - a pointer to pixels data of input 8-bit gray image.
        \param [in] srcStride - a row size of the image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] mask - a pointer to pixels data of the mask 8-bit image.
        \param [in] maskStride - a row size of the mask image (in bytes).
        \param [in] value - a value to compare with every mask pixel.
        \param [in] compareType - a compare operation type (see ::SimdCompareType).
        \param [out] histogram - a pointer to the output histogram (array of 256 unsigned 32-bit values).
    */
    SIMD_API void SimdHistogramConditional(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        const uint8_t * mask, size_t maskStride, uint8_t value, SimdCompareType compareType, uint32_t * histogram);

    /*! @ingroup histogram

        \fn void SimdNormalizedColors(const uint32_t * histogram, uint8_t * colors);

        \short Gets a histogram-equalization color map for a given 256-bin histogram.

        The function builds cumulative sums, finds the first non-zero histogram bin minColor with
        count minCount, and calculates:
        \verbatim
        integral[i] = Sum(histogram[j]), j <= i;
        norm = integral[255] - minCount;
        colors[i] = i < minColor ? 0 :
            (norm ? (255*(integral[i] - minCount) + norm/2)/norm : minColor);
        \endverbatim

        The output color map can be used by ::SimdChangeColors.

        \param [in] histogram - a pointer to histogram (array of 256 unsigned 32-bit values).
        \param [out] colors - a pointer to the color map (array of 256 unsigned 8-bit values).
    */
    SIMD_API void SimdNormalizedColors(const uint32_t * histogram, uint8_t * colors);

    /*! @ingroup histogram

        \fn void SimdChangeColors(const uint8_t * src, size_t srcStride, size_t width, size_t height, const uint8_t * colors, uint8_t * dst, size_t dstStride);

        \short Applies an 8-bit lookup table to an 8-bit gray image.

        The input and output images must have the same width and height. For every pixel:
        \verbatim
        for(y = 0; y < height; ++y)
            for(x = 0; x < width; ++x)
                dst[x, y] = colors[src[x, y]];
        \endverbatim

        \note This function has a C++ wrapper Simd::ChangeColors(const View<A> & src, const uint8_t * colors, View<A> & dst).

        \param [in] src - a pointer to pixels data of input 8-bit gray image.
        \param [in] srcStride - a row size of the image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] colors - a pointer to the color map (array of 256 unsigned 8-bit values).
        \param [out] dst - a pointer to pixels data of output 8-bit gray image.
        \param [in] dstStride - a row size of the output gray image (in bytes).
    */
    SIMD_API void SimdChangeColors(const uint8_t * src, size_t srcStride, size_t width, size_t height, const uint8_t * colors, uint8_t * dst, size_t dstStride);

    /*! @ingroup histogram

        \fn void SimdNormalizeHistogram(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

        \short Performs histogram equalization for an 8-bit gray image.

        The input and output images must have the same width and height. The function calculates
        ::SimdHistogram for src, creates the lookup table with ::SimdNormalizedColors, and applies it
        with ::SimdChangeColors.

        \note This function has a C++ wrapper Simd::NormalizeHistogram(const View<A> & src, View<A> & dst).

        \param [in] src - a pointer to pixels data of input 8-bit gray image.
        \param [in] srcStride - a row size of the image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of output 8-bit image with normalized histogram.
        \param [in] dstStride - a row size of the output image (in bytes).
    */
    SIMD_API void SimdNormalizeHistogram(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

    /*! @ingroup hog

        \fn void SimdHogDirectionHistograms(const uint8_t * src, size_t stride, size_t width, size_t height, size_t cellX, size_t cellY, size_t quantization, float * histograms);

        \short Calculates HOG direction histograms for an 8-bit gray image.

        \deprecated This function will be removed in the nearest future.

        The function uses central differences for pixels except the one-pixel image border:
        \verbatim
        dx = src[x + 1, y] - src[x - 1, y];
        dy = src[x, y + 1] - src[x, y - 1];
        magnitude = Sqrt(dx*dx + dy*dy);
        direction = index with maximal absolute dot product against quantization directions;
        \endverbatim

        Pixel magnitudes are bilinearly distributed to neighboring cells. The output buffer is
        cleared and then filled in row-major cell order:
        histograms[(cellYIndex*(width/cellX) + cellXIndex)*quantization + direction].

        \note This function has a C++ wrapper Simd::HogDirectionHistograms(const View<A> & src, const Point<ptrdiff_t> & cell, size_t quantization, float * histograms).

        \param [in] src - a pointer to pixels data of input 8-bit gray image.
        \param [in] stride - a row size of the image (in bytes).
        \param [in] width - an image width. It must be a multiple of cellX.
        \param [in] height - an image height. It must be a multiple of cellY.
        \param [in] cellX - a width of cell.
        \param [in] cellY - a height of cell.
        \param [in] quantization - a direction quantization. Must be even.
        \param [out] histograms - a pointer to buffer with histograms. Array must have size greater or equal to (width/cellX)*(height/cellY)*quantization.
    */
    SIMD_DEPRECATED SIMD_API void SimdHogDirectionHistograms(const uint8_t * src, size_t stride, size_t width, size_t height,
        size_t cellX, size_t cellY, size_t quantization, float * histograms);

    /*! @ingroup hog

        \fn void SimdHogExtractFeatures(const uint8_t * src, size_t stride, size_t width, size_t height, float * features);

        \short Extracts 31 HOG features per 8x8 cell from an 8-bit gray image.

        \deprecated This function will be removed in the nearest future.

        The function builds 18 signed gradient-orientation histograms for 8x8 cells, estimates
        normalization factors from neighboring 2x2 blocks, clips normalized values by 0.2, and writes
        31 features per cell:
        \verbatim
        features[(cellY*(width/8) + cellX)*31 + 0..17]  - contrast-sensitive features;
        features[(cellY*(width/8) + cellX)*31 + 18..26] - contrast-insensitive features;
        features[(cellY*(width/8) + cellX)*31 + 27..30] - texture energy features.
        \endverbatim

        \note This function has a C++ wrapper Simd::HogExtractFeatures(const View<A> & src, float * features).

        \param [in] src - a pointer to pixels data of input 8-bit gray image.
        \param [in] stride - a row size of the image (in bytes).
        \param [in] width - an image width. It must be a multiple of 8. Its minimal value is 16.
        \param [in] height - an image height. It must be a multiple of 8. Its minimal value is 16.
        \param [out] features - a pointer to buffer with features. Array must have size greater or equal to (width/8)*(height/8)*31.
    */
    SIMD_DEPRECATED SIMD_API void SimdHogExtractFeatures(const uint8_t * src, size_t stride, size_t width, size_t height, float * features);

    /*! @ingroup hog

        \fn void SimdHogDeinterleave(const float * src, size_t srcStride, size_t width, size_t height, size_t count, float ** dst, size_t dstStride);

        \short Deinterleaves a 32-bit floating-point image into separate planes.

        \deprecated This function will be removed in the nearest future.

        For every point and plane:
        \verbatim
        dst[i][y*dstStride + x] = src[y*srcStride + x*count + i];
        \endverbatim

        Strides are measured in 32-bit floats.

        \param [in] src - a pointer to the input interleaved 32-bit float point image.
        \param [in] srcStride - a row size of input image (in 32-bit floats).
        \param [in] width - a width of input and output images.
        \param [in] height - a height of input and output images.
        \param [in] count - the number of output planes.
        \param [out] dst - a pointer to array with pointers to output planes.
        \param [in] dstStride - a row size of output images (in 32-bit floats).
    */
    SIMD_DEPRECATED SIMD_API void SimdHogDeinterleave(const float * src, size_t srcStride, size_t width, size_t height, size_t count, float ** dst, size_t dstStride);

    /*! @ingroup hog

        \fn void SimdHogFilterSeparable(const float * src, size_t srcStride, size_t width, size_t height, const float * rowFilter, size_t rowSize, const float * colFilter, size_t colSize, float * dst, size_t dstStride, int add);

        \short Applies a valid-area separable filter to a 32-bit floating-point image.

        \deprecated This function will be removed in the nearest future.

        The destination size is (width - rowSize + 1) by (height - colSize + 1). For every output
        point:
        \verbatim
        sum = 0;
        for(dy = 0; dy < colSize; dy++)
            for(dx = 0; dx < rowSize; dx++)
                sum += src[x + dx, y + dy]*colFilter[dy]*rowFilter[dx];
        if(add)
            dst[x, y] += sum;
        else
            dst[x, y] = sum;
        \endverbatim

        \note Input image has to have size not less than the filter size: width >= rowSize and height >= colSize.

        \param [in] src - a pointer to input 32-bit float point image.
        \param [in] srcStride - a row size of input image (in 32-bit floats).
        \param [in] width - a width of input image. It must be not less than size of row filter.
        \param [in] height - a height of input image. It must be not less than size of column filter.
        \param [in] rowFilter - a pointer to 32-bit float point array with row filter.
        \param [in] rowSize - a size of row filter.
        \param [in] colFilter - a pointer to 32-bit float point array with column filter.
        \param [in] colSize - a size of column filter.
        \param [in, out] dst - a pointer to output 32-bit float point image.
        \param [in] dstStride - a row size of output image (in 32-bit floats).
        \param [in] add - a flag: if non-zero, the filtered result is added to dst; otherwise dst is overwritten.
    */
    SIMD_DEPRECATED SIMD_API void SimdHogFilterSeparable(const float * src, size_t srcStride, size_t width, size_t height, const float * rowFilter, size_t rowSize, const float * colFilter, size_t colSize, float * dst, size_t dstStride, int add);

    /*! @ingroup image_io

        \fn uint8_t* SimdImageSaveToMemory(const uint8_t* src, size_t stride, size_t width, size_t height, SimdPixelFormatType format, SimdImageFileType file, int quality, size_t * size);

        \short Encodes an image to a memory buffer.

        The source image must have non-zero width and height. Supported source pixel formats are
        ::SimdPixelFormatGray8, ::SimdPixelFormatBgr24, ::SimdPixelFormatBgra32,
        ::SimdPixelFormatRgb24 and ::SimdPixelFormatRgba32. Supported output file formats are PGM,
        PPM, PNG, JPEG and BMP variants from ::SimdImageFileType. If file is
        ::SimdImageFileUndefined, the function saves Gray8 input as binary PGM and all other
        supported inputs as binary PPM.

        Encoders convert the input to the pixel layout required by the selected file format. For JPEG,
        quality is clamped to the range [1..100]. The returned buffer is allocated by the library.

        \param [in] src - a pointer to pixels data of input image.
        \param [in] stride - a row size of input image (in bytes).
        \param [in] width - a width of input image.
        \param [in] height - a height of input image.
        \param [in] format - a pixel format of input image.
        \param [in] file - a format of output image file.
        \param [in] quality - a compression quality parameter for formats that use it.
        \param [out] size - a pointer to the size of output image file in bytes.
        \return a pointer to memory buffer with output image file.
            It has to be deleted after use by function ::SimdFree. On error it returns NULL.
    */
    SIMD_API uint8_t* SimdImageSaveToMemory(const uint8_t* src, size_t stride, size_t width, size_t height, SimdPixelFormatType format, SimdImageFileType file, int quality, size_t * size);

    /*! @ingroup image_io

        \fn SimdBool SimdImageSaveToFile(const uint8_t* src, size_t stride, size_t width, size_t height, SimdPixelFormatType format, SimdImageFileType file, int quality, const char * path);

        \short Encodes an image and writes it to a file.

        This function first uses ::SimdImageSaveToMemory and then writes the encoded buffer to path.
        If file is ::SimdImageFileUndefined, the output format is selected from recognized file
        extensions: .pgm, .ppm, .png, .jpg/.jpeg or .bmp. For .jpg/.jpeg with default quality 100,
        the function uses quality 85. If the extension is not recognized, the same default selection
        as ::SimdImageSaveToMemory is used.

        \note This function has a C++ wrapper Simd::View::Save(const std::string & path, ::SimdImageFileType type = ::SimdImageFileUndefined, int quality = 100).

        \param [in] src - a pointer to pixels data of input image.
        \param [in] stride - a row size of input image (in bytes).
        \param [in] width - a width of input image.
        \param [in] height - a height of input image.
        \param [in] format - a pixel format of input image.
        \param [in] file - a format of output image file, or ::SimdImageFileUndefined to infer it from path.
        \param [in] quality - a compression quality parameter for formats that use it.
        \param [in] path - a path to output image file.
        \return ::SimdTrue if encoding, opening the file and writing all bytes succeeded; otherwise ::SimdFalse.
    */
    SIMD_API SimdBool SimdImageSaveToFile(const uint8_t* src, size_t stride, size_t width, size_t height, SimdPixelFormatType format, SimdImageFileType file, int quality, const char * path);

    /*! @ingroup image_io

        \fn uint8_t* SimdNv12SaveAsJpegToMemory(const uint8_t* y, size_t yStride, const uint8_t* uv, size_t uvStride, size_t width, size_t height, SimdYuvType yuvType, int quality, size_t* size);

        \short Encodes an NV12 image to a JPEG memory buffer.

        The input image has one full-size Y plane and one half-size interleaved UV plane. Width and
        height must be even. The only supported YUV type is ::SimdYuvTrect871. Quality is clamped to
        the range [1..100].

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image (in bytes).
        \param [in] uv - a pointer to pixels data of input interleaved UV plane.
        \param [in] uvStride - a row size of the uv image (in bytes).
        \param [in] width - a width of input image. It must be even number.
        \param [in] height - a height of input image. It must be even number.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType). Now only ::SimdYuvTrect871 is supported.
        \param [in] quality - a JPEG compression quality parameter.
        \param [out] size - a pointer to the size of output image file in bytes.
        \return a pointer to memory buffer with output JPEG file.
            It has to be deleted after use by function ::SimdFree. On error it returns NULL.
    */
    SIMD_API uint8_t* SimdNv12SaveAsJpegToMemory(const uint8_t* y, size_t yStride, const uint8_t* uv, size_t uvStride, size_t width, size_t height, SimdYuvType yuvType, int quality, size_t* size);

    /*! @ingroup image_io

        \fn uint8_t* SimdYuv420pSaveAsJpegToMemory(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride, size_t width, size_t height, SimdYuvType yuvType, int quality, size_t* size);

        \short Encodes a YUV420P image to a JPEG memory buffer.

        The input image has one full-size Y plane and two half-size U and V planes. Width and height
        must be even. The only supported YUV type is ::SimdYuvTrect871. Quality is clamped to the
        range [1..100].

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image (in bytes).
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image (in bytes).
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image (in bytes).
        \param [in] width - a width of input image. It must be even number.
        \param [in] height - a height of input image. It must be even number.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType). Now only ::SimdYuvTrect871 is supported.
        \param [in] quality - a JPEG compression quality parameter.
        \param [out] size - a pointer to the size of output image file in bytes.
        \return a pointer to memory buffer with output JPEG file.
            It has to be deleted after use by function ::SimdFree. On error it returns NULL.
    */
    SIMD_API uint8_t* SimdYuv420pSaveAsJpegToMemory(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride, 
        size_t width, size_t height, SimdYuvType yuvType, int quality, size_t* size);

    /*! @ingroup image_io

        \fn uint8_t* SimdImageLoadFromMemory(const uint8_t* data, size_t size, size_t* stride, size_t* width, size_t* height, SimdPixelFormatType * format);

        \short Decodes an image from a memory buffer.

        The function detects PGM/PPM, PNG, JPEG and BMP files from their in-memory signatures. On
        input, *format can request ::SimdPixelFormatGray8, ::SimdPixelFormatBgr24,
        ::SimdPixelFormatBgra32, ::SimdPixelFormatRgb24 or ::SimdPixelFormatRgba32. If *format is
        ::SimdPixelFormatNone, the loader keeps the natural format chosen for the input file. On
        success, stride, width, height and *format are filled with the decoded image properties.

        \note This function has a C++ wrapper Simd::View::Load(const uint8_t * src, size_t size, Simd::View::Format format = Simd::View::None).

        \param [in] data - a pointer to memory buffer with input image file.
        \param [in] size - a size of input image file in bytes.
        \param [out] stride - a pointer to row size of output image in bytes.
        \param [out] width - a pointer to width of output image.
        \param [out] height - a pointer to height of output image.
        \param [in, out] format - a pointer to requested pixel format on input and decoded pixel format on output.
        \return a pointer to pixels data of output image.
            It has to be deleted after use by function ::SimdFree. On error it returns NULL.
    */
    SIMD_API uint8_t* SimdImageLoadFromMemory(const uint8_t* data, size_t size, size_t* stride, size_t* width, size_t* height, SimdPixelFormatType * format);

    /*! @ingroup image_io

        \fn uint8_t* SimdImageLoadFromFile(const char* path, size_t* stride, size_t* width, size_t* height, SimdPixelFormatType * format);

        \short Decodes an image from a file.

        The function reads the whole file into memory and then calls ::SimdImageLoadFromMemory. The
        file type is detected from the file content, not from the extension.

        \note This function has a C++ wrapper Simd::View::Load(const std::string & path, Simd::View::Format format = Simd::View::None).

        \param [in] path - a path to input image file.
        \param [out] stride - a pointer to row size of output image in bytes.
        \param [out] width - a pointer to width of output image.
        \param [out] height - a pointer to height of output image.
        \param [in, out] format - a pointer to requested pixel format on input and decoded pixel format on output.
        \return a pointer to pixels data of output image.
            It has to be deleted after use by function ::SimdFree. On error it returns NULL.
    */
    SIMD_API uint8_t* SimdImageLoadFromFile(const char* path, size_t* stride, size_t* width, size_t* height, SimdPixelFormatType * format);

    /*! @ingroup other_conversion

        \fn void SimdInt16ToGray(const uint8_t * src, size_t width, size_t height, size_t srcStride, uint8_t * dst, size_t dstStride);

        \short Converts a 16-bit signed integer image to an 8-bit gray image with saturation.

        The source pointer is typed as uint8_t for ABI compatibility, but each source pixel is read as
        int16_t. For every point:
        \verbatim
        dst[x, y] = RestrictRange((int)src16[x, y], 0, 255);
        \endverbatim

        All images must have the same width and height.

        \note This function has a C++ wrapper Simd::Int16ToGray(const View<A> & src, View<A> & dst).

        \param [in] src - a pointer to pixels data of input 16-bit signed integer image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] srcStride - a row size of the 16-bit signed integer image (in bytes).
        \param [out] dst - a pointer to pixels data of output 8-bit gray image.
        \param [in] dstStride - a row size of the gray image (in bytes).
    */
    SIMD_API void SimdInt16ToGray(const uint8_t * src, size_t width, size_t height, size_t srcStride, uint8_t * dst, size_t dstStride);

    /*! @ingroup integral

        \fn void SimdIntegral(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * sum, size_t sumStride, uint8_t * sqsum, size_t sqsumStride, uint8_t * tilted, size_t tiltedStride, SimdPixelFormatType sumFormat, SimdPixelFormatType sqsumFormat);

        \short Calculates integral images for an 8-bit gray image.

        The sum, square-sum and tilted-sum images have width + 1 columns and height + 1 rows. The
        first row and first column are initialized to zero for sum and square-sum images. The sum
        image is always 32-bit integer:
        \verbatim
        sum[x + 1, y + 1] = Sum(src[i, j]), 0 <= i <= x, 0 <= j <= y;
        sqsum[x + 1, y + 1] = Sum(src[i, j]*src[i, j]), 0 <= i <= x, 0 <= j <= y;
        \endverbatim

        sqsum and tilted are optional and can be NULL. If sqsum is not NULL, sqsumFormat selects
        32-bit integer or 64-bit floating-point output. If tilted is not NULL, it is written as a
        32-bit integer tilted integral image. sumFormat must be ::SimdPixelFormatInt32.

        \note This function has a C++ wrappers:
        \n Simd::Integral(const View<A>& src, View<A>& sum),
        \n Simd::Integral(const View<A>& src, View<A>& sum, View<A>& sqsum),
        \n Simd::Integral(const View<A>& src, View<A>& sum, View<A>& sqsum, View<A>& tilted).

        \param [in] src - a pointer to pixels data of input 8-bit gray image.
        \param [in] srcStride - a row size of src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sum - a pointer to pixels data of 32-bit integer sum image.
        \param [in] sumStride - a row size of sum image (in bytes).
        \param [out] sqsum - a pointer to pixels data of 32-bit integer or 64-bit float point square sum image. It can be NULL.
        \param [in] sqsumStride - a row size of sqsum image (in bytes).
        \param [out] tilted - a pointer to pixels data of 32-bit integer tilted sum image. It can be NULL.
        \param [in] tiltedStride - a row size of tilted image (in bytes).
        \param [in] sumFormat - a format of sum image and tilted image. It must be equal to ::SimdPixelFormatInt32.
        \param [in] sqsumFormat - a format of sqsum image. It can be equal to ::SimdPixelFormatInt32 or ::SimdPixelFormatDouble.
    */
    SIMD_API void SimdIntegral(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        uint8_t * sum, size_t sumStride, uint8_t * sqsum, size_t sqsumStride, uint8_t * tilted, size_t tiltedStride,
        SimdPixelFormatType sumFormat, SimdPixelFormatType sqsumFormat);

    /*! @ingroup interleave_conversion

        \fn void SimdInterleaveUv(const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * uv, size_t uvStride);

        \short Interleaves 8-bit U and V planar images into one 16-bit UV image.

        For every point:
        \verbatim
        uv[2*x + 0, y] = u[x, y];
        uv[2*x + 1, y] = v[x, y];
        \endverbatim

        All images must have the same width and height. This function is used for YUV420P to NV12 conversion.

        \note This function has a C++ wrapper Simd::InterleaveUv(const View<A>& u, const View<A>& v, View<A>& uv).

        \param [in] u - a pointer to pixels data of input 8-bit U planar image.
        \param [in] uStride - a row size of the u image (in bytes).
        \param [in] v - a pointer to pixels data of input 8-bit V planar image.
        \param [in] vStride - a row size of the v image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] uv - a pointer to pixels data of output 16-bit UV interleaved image.
        \param [in] uvStride - a row size of the uv image (in bytes).
    */
    SIMD_API void SimdInterleaveUv(const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * uv, size_t uvStride);

    /*! @ingroup interleave_conversion

        \fn void SimdInterleaveBgr(const uint8_t * b, size_t bStride, const uint8_t * g, size_t gStride, const uint8_t * r, size_t rStride, size_t width, size_t height, uint8_t * bgr, size_t bgrStride);

        \short Interleaves 8-bit Blue, Green and Red planar images into one 24-bit BGR image.

        For every point:
        \verbatim
        bgr[3*x + 0, y] = b[x, y];
        bgr[3*x + 1, y] = g[x, y];
        bgr[3*x + 2, y] = r[x, y];
        \endverbatim

        All images must have the same width and height.

        \note This function has a C++ wrapper Simd::InterleaveBgr(const View<A>& b, const View<A>& g, const View<A>& r, View<A>& bgr).

        \param [in] b - a pointer to pixels data of input 8-bit Blue planar image.
        \param [in] bStride - a row size of the b image (in bytes).
        \param [in] g - a pointer to pixels data of input 8-bit Green planar image.
        \param [in] gStride - a row size of the g image (in bytes).
        \param [in] r - a pointer to pixels data of input 8-bit Red planar image.
        \param [in] rStride - a row size of the r image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] bgr - a pointer to pixels data of output 24-bit BGR interleaved image.
        \param [in] bgrStride - a row size of the bgr image (in bytes).
    */
    SIMD_API void SimdInterleaveBgr(const uint8_t * b, size_t bStride, const uint8_t * g, size_t gStride, const uint8_t * r, size_t rStride,
        size_t width, size_t height, uint8_t * bgr, size_t bgrStride);

    /*! @ingroup interleave_conversion

        \fn void SimdInterleaveBgra(const uint8_t * b, size_t bStride, const uint8_t * g, size_t gStride, const uint8_t * r, size_t rStride, const uint8_t * a, size_t aStride, size_t width, size_t height, uint8_t * bgra, size_t bgraStride);

        \short Interleaves 8-bit Blue, Green, Red and Alpha planar images into one 32-bit BGRA image.

        For every point:
        \verbatim
        bgra[4*x + 0, y] = b[x, y];
        bgra[4*x + 1, y] = g[x, y];
        bgra[4*x + 2, y] = r[x, y];
        bgra[4*x + 3, y] = a[x, y];
        \endverbatim

        All images must have the same width and height.

        \note This function has a C++ wrapper Simd::InterleaveBgra(const View<A>& b, const View<A>& g, const View<A>& r, const View<A>& a, View<A>& bgra).

        \param [in] b - a pointer to pixels data of input 8-bit Blue planar image.
        \param [in] bStride - a row size of the b image (in bytes).
        \param [in] g - a pointer to pixels data of input 8-bit Green planar image.
        \param [in] gStride - a row size of the g image (in bytes).
        \param [in] r - a pointer to pixels data of input 8-bit Red planar image.
        \param [in] rStride - a row size of the r image (in bytes).
        \param [in] a - a pointer to pixels data of input 8-bit Alpha planar image.
        \param [in] aStride - a row size of the a image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] bgra - a pointer to pixels data of output 32-bit BGRA interleaved image.
        \param [in] bgraStride - a row size of the bgra image (in bytes).
    */
    SIMD_API void SimdInterleaveBgra(const uint8_t * b, size_t bStride, const uint8_t * g, size_t gStride, const uint8_t * r, size_t rStride, const uint8_t * a, size_t aStride,
        size_t width, size_t height, uint8_t * bgra, size_t bgraStride);


    /*! @ingroup laplace_filter

        \fn void SimdLaplace(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

        \short Calculates a signed 3x3 Laplace filter for an 8-bit gray image.

        The destination image stores signed 16-bit values. The dst pointer is typed as uint8_t for ABI
        compatibility, but dstStride is measured in bytes and must be compatible with int16_t rows.
        Border pixels are handled by nearest-pixel replication:
        \verbatim
        sx0 = Max(x - 1, 0);
        sx1 = x;
        sx2 = Min(x + 1, width - 1);
        sy0 = Max(y - 1, 0);
        sy1 = y;
        sy2 = Min(y + 1, height - 1);

        dst[x, y] = 8*src[sx1, sy1] -
            (src[sx0, sy0] + src[sx1, sy0] + src[sx2, sy0] +
             src[sx0, sy1]                  + src[sx2, sy1] +
             src[sx0, sy2] + src[sx1, sy2] + src[sx2, sy2]);
        \endverbatim

        \note This function has a C++ wrapper: Simd::Laplace(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of the input 8-bit gray image.
        \param [in] srcStride - a row size of the input image (in bytes).
        \param [in] width - an image width. It must be greater than 1.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of the output 16-bit signed integer image.
        \param [in] dstStride - a row size of the output image (in bytes).
    */
    SIMD_API void SimdLaplace(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

    /*! @ingroup laplace_filter

        \fn void SimdLaplaceAbs(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

        \short Calculates the absolute value of a 3x3 Laplace filter for an 8-bit gray image.

        The destination image stores signed 16-bit values containing Abs(Laplace(src)). Border pixels
        are handled by nearest-pixel replication as in ::SimdLaplace.

        \note This function has a C++ wrapper: Simd::LaplaceAbs(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of the input 8-bit gray image.
        \param [in] srcStride - a row size of the input image (in bytes).
        \param [in] width - an image width. It must be greater than 1.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of the output 16-bit signed integer image.
        \param [in] dstStride - a row size of the output image (in bytes).
    */
    SIMD_API void SimdLaplaceAbs(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

    /*! @ingroup other_statistic

        \fn void SimdLaplaceAbsSum(const uint8_t * src, size_t stride, size_t width, size_t height, uint64_t * sum);

        \short Calculates the sum of absolute 3x3 Laplace values for an 8-bit gray image.

        The function sets sum[0] to zero and accumulates Abs(Laplace(src)) for every pixel. Border
        pixels are handled by nearest-pixel replication as in ::SimdLaplace.

        \note This function has a C++ wrapper: Simd::LaplaceAbsSum(const View<A>& src, uint64_t & sum).

        \param [in] src - a pointer to pixels data of the input 8-bit gray image.
        \param [in] stride - a row size of the input image (in bytes).
        \param [in] width - an image width. It must be greater than 1.
        \param [in] height - an image height.
        \param [out] sum - a pointer to result sum.
    */
    SIMD_API void SimdLaplaceAbsSum(const uint8_t * src, size_t stride, size_t width, size_t height, uint64_t * sum);

    /*! @ingroup other_filter

        \fn void SimdLbpEstimate(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

        \short Calculates LBP (Local Binary Pattern) codes for an 8-bit gray image.

        The first and last rows and columns of dst are set to zero. For every inner pixel, the center
        value is used as threshold and eight neighbor comparisons are packed clockwise starting from
        the top-left neighbor:
        \verbatim
        t = src[x, y];
        dst[x, y] =
            (src[x - 1, y - 1] >= t ? 0x01 : 0) |
            (src[x,     y - 1] >= t ? 0x02 : 0) |
            (src[x + 1, y - 1] >= t ? 0x04 : 0) |
            (src[x + 1, y    ] >= t ? 0x08 : 0) |
            (src[x + 1, y + 1] >= t ? 0x10 : 0) |
            (src[x,     y + 1] >= t ? 0x20 : 0) |
            (src[x - 1, y + 1] >= t ? 0x40 : 0) |
            (src[x - 1, y    ] >= t ? 0x80 : 0);
        \endverbatim

        \note This function has a C++ wrapper: Simd::LbpEstimate(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of input 8-bit gray image.
        \param [in] srcStride - a row size of src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of output 8-bit gray image with LBP codes.
        \param [in] dstStride - a row size of dst image (in bytes).
    */
    SIMD_API void SimdLbpEstimate(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

    /*! @ingroup max_filter

        \fn void SimdMaxFilterSquare3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride, int threshold);

        \short Performs thresholded 3x3 square maximum filtering of an 8-bit interleaved image.

        The filter is applied independently to every channel. Border pixels are handled by
        nearest-pixel replication. If threshold <= 1, dst receives the maximum value in the 3x3
        window. Otherwise dst receives this maximum only when it occurs at least threshold times in
        the window; if not, dst receives the center pixel.

        \note This function has a C++ wrapper: Simd::MaxFilterSquare3x3(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of original input image.
        \param [in] srcStride - a row size of src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of filtered output image.
        \param [in] dstStride - a row size of dst image (in bytes).
        \param [in] threshold - a minimal count of maximal values required to replace the center pixel.
    */
    SIMD_API void SimdMaxFilterSquare3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        size_t channelCount, uint8_t * dst, size_t dstStride, int threshold);

    /*! @ingroup max_filter

        \fn void SimdMaxFilterSquare5x5(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride, int threshold);

        \short Performs thresholded 5x5 square maximum filtering of an 8-bit interleaved image.

        The filter is applied independently to every channel. Border pixels are handled by
        nearest-pixel replication. If threshold <= 1, dst receives the maximum value in the 5x5
        window. Otherwise dst receives this maximum only when it occurs at least threshold times in
        the window; if not, dst receives the center pixel.

        \note This function has a C++ wrapper: Simd::MaxFilterSquare5x5(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of original input image.
        \param [in] srcStride - a row size of src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of filtered output image.
        \param [in] dstStride - a row size of dst image (in bytes).
        \param [in] threshold - a minimal count of maximal values required to replace the center pixel.
    */
    SIMD_API void SimdMaxFilterSquare5x5(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        size_t channelCount, uint8_t * dst, size_t dstStride, int threshold);

    /*! @ingroup min_filter

        \fn void SimdMinFilterSquare3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride, int threshold);

        \short Performs thresholded 3x3 square minimum filtering of an 8-bit interleaved image.

        The filter is applied independently to every channel. Border pixels are handled by
        nearest-pixel replication. If threshold <= 1, dst receives the minimum value in the 3x3
        window. Otherwise dst receives this minimum only when it occurs at least threshold times in
        the window; if not, dst receives the center pixel.

        \note This function has a C++ wrapper: Simd::MinFilterSquare3x3(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of original input image.
        \param [in] srcStride - a row size of src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of filtered output image.
        \param [in] dstStride - a row size of dst image (in bytes).
        \param [in] threshold - a minimal count of minimal values required to replace the center pixel.
    */
    SIMD_API void SimdMinFilterSquare3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        size_t channelCount, uint8_t * dst, size_t dstStride, int threshold);

    /*! @ingroup min_filter

        \fn void SimdMinFilterSquare5x5(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride, int threshold);

        \short Performs thresholded 5x5 square minimum filtering of an 8-bit interleaved image.

        The filter is applied independently to every channel. Border pixels are handled by
        nearest-pixel replication. If threshold <= 1, dst receives the minimum value in the 5x5
        window. Otherwise dst receives this minimum only when it occurs at least threshold times in
        the window; if not, dst receives the center pixel.

        \note This function has a C++ wrapper: Simd::MinFilterSquare5x5(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of original input image.
        \param [in] srcStride - a row size of src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of filtered output image.
        \param [in] dstStride - a row size of dst image (in bytes).
        \param [in] threshold - a minimal count of minimal values required to replace the center pixel.
    */
    SIMD_API void SimdMinFilterSquare5x5(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        size_t channelCount, uint8_t * dst, size_t dstStride, int threshold);

    /*! @ingroup other_filter

        \fn void SimdMeanFilter3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride);

        \short Performs 3x3 mean filtering of an 8-bit interleaved image.

        The filter is applied independently to every channel. Border pixels are handled by
        nearest-pixel replication. For every channel of every pixel:
        \verbatim
        sum = Sum of the 9 samples in the 3x3 window;
        dst[x, y, c] = (sum + 5) / 9;
        \endverbatim

        \note This function has a C++ wrapper Simd::MeanFilter3x3(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of source image.
        \param [in] srcStride - a row size of the src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of destination image.
        \param [in] dstStride - a row size of the dst image (in bytes).
    */
    SIMD_API void SimdMeanFilter3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        size_t channelCount, uint8_t * dst, size_t dstStride);

    /*! @ingroup median_filter

        \fn void SimdMedianFilterRhomb3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride);

        \short Performs median filtering with a 3x3 rhomb window for an 8-bit interleaved image.

        The filter is applied independently to every channel. Border pixels are handled by
        nearest-pixel replication. The rhomb window contains 5 samples: top, left, center, right and
        bottom. The output is the middle value of these 5 samples.

        \note This function has a C++ wrapper: Simd::MedianFilterRhomb3x3(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of original input image.
        \param [in] srcStride - a row size of src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of filtered output image.
        \param [in] dstStride - a row size of dst image (in bytes).
    */
    SIMD_API void SimdMedianFilterRhomb3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        size_t channelCount, uint8_t * dst, size_t dstStride);

    /*! @ingroup median_filter

        \fn void SimdMedianFilterRhomb5x5(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride);

        \short Performs median filtering with a 5x5 rhomb window for an 8-bit interleaved image.

        The filter is applied independently to every channel. Border pixels are handled by
        nearest-pixel replication. The rhomb window contains 13 samples. The output is the middle
        value of these 13 samples.

        \note This function has a C++ wrapper: Simd::MedianFilterRhomb5x5(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of original input image.
        \param [in] srcStride - a row size of src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of filtered output image.
        \param [in] dstStride - a row size of dst image (in bytes).
    */
    SIMD_API void SimdMedianFilterRhomb5x5(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        size_t channelCount, uint8_t * dst, size_t dstStride);

    /*! @ingroup median_filter

        \fn void SimdMedianFilterSquare3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride);

        \short Performs median filtering with a 3x3 square window for an 8-bit interleaved image.

        The filter is applied independently to every channel. Border pixels are handled by
        nearest-pixel replication. The output is the middle value of 9 samples in the 3x3 window.

        \note This function has a C++ wrapper: Simd::MedianFilterSquare3x3(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of original input image.
        \param [in] srcStride - a row size of src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of filtered output image.
        \param [in] dstStride - a row size of dst image (in bytes).
    */
    SIMD_API void SimdMedianFilterSquare3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        size_t channelCount, uint8_t * dst, size_t dstStride);

    /*! @ingroup median_filter

        \fn void SimdMedianFilterSquare5x5(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride);

        \short Performs median filtering with a 5x5 square window for an 8-bit interleaved image.

        The filter is applied independently to every channel. Border pixels are handled by
        nearest-pixel replication. The output is the middle value of 25 samples in the 5x5 window.

        \note This function has a C++ wrapper: Simd::MedianFilterSquare5x5(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of original input image.
        \param [in] srcStride - a row size of src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of filtered output image.
        \param [in] dstStride - a row size of dst image (in bytes).
    */
    SIMD_API void SimdMedianFilterSquare5x5(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        size_t channelCount, uint8_t * dst, size_t dstStride);

    /*! @ingroup midpoint_filter

        \fn void SimdMidpointFilterSquare3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride);

        \short Performs 3x3 square midpoint filtering of an 8-bit interleaved image.

        The filter is applied independently to every channel. Border pixels are handled by
        nearest-pixel replication. For every output sample:
        \verbatim
        min = minimum value in the 3x3 window;
        max = maximum value in the 3x3 window;
        dst[x, y, c] = (min + max + ((min + max) & 1)) / 2;
        \endverbatim

        \note This function has a C++ wrapper: Simd::MidpointFilterSquare3x3(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of original input image.
        \param [in] srcStride - a row size of src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of filtered output image.
        \param [in] dstStride - a row size of dst image (in bytes).
    */
    SIMD_API void SimdMidpointFilterSquare3x3(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        size_t channelCount, uint8_t * dst, size_t dstStride);

    /*! @ingroup midpoint_filter

        \fn void SimdMidpointFilterSquare5x5(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride);

        \short Performs 5x5 square midpoint filtering of an 8-bit interleaved image.

        The filter is applied independently to every channel. Border pixels are handled by
        nearest-pixel replication. For every output sample:
        \verbatim
        min = minimum value in the 5x5 window;
        max = maximum value in the 5x5 window;
        dst[x, y, c] = (min + max + ((min + max) & 1)) / 2;
        \endverbatim

        \note This function has a C++ wrapper: Simd::MidpointFilterSquare5x5(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of original input image.
        \param [in] srcStride - a row size of src image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of filtered output image.
        \param [in] dstStride - a row size of dst image (in bytes).
    */
    SIMD_API void SimdMidpointFilterSquare5x5(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        size_t channelCount, uint8_t * dst, size_t dstStride);


    /*! @ingroup neural

        \fn void SimdNeuralConvert(const uint8_t * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride, int inversion);

        \short Converts an 8-bit gray image to a 32-bit floating-point image scaled to [0, 1].

        \deprecated This function will be removed in the nearest future.

        For every point:
        \verbatim
        dst[x, y] = inversion ? (255 - src[x, y])/255.0 : src[x, y]/255.0;
        \endverbatim

        \note This function has a C++ wrapper Simd::NeuralConvert(const View<A>& src, float * dst, bool inversion).

        \param [in] src - a pointer to pixels data of input 8-bit gray image.
        \param [in] srcStride - a row size of the input image (in bytes).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] dst - a pointer to output 32-bit float image.
        \param [in] dstStride - a row size of the output image (in 32-bit float values).
        \param [in] inversion - a flag of color inversion.
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralConvert(const uint8_t * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride, int inversion);

    /*! @ingroup neural

        \fn void SimdNeuralDerivativeSigmoid(const float * src, size_t size, const float * slope, float * dst);

        \short Multiplies a 32-bit float array by the derivative of sigmoid values.

        \deprecated This function will be removed in the nearest future.

        For every element:
        \verbatim
        dst[i] *= slope[0]*(1 - src[i])*src[i];
        \endverbatim

        \note This function is used in Simd::Neural::Function.

        \param [in] src - a pointer to sigmoid output values.
        \param [in] size - a size of arrays.
        \param [in] slope - a pointer to the slope parameter.
        \param [in, out] dst - a pointer to cumulative 32-bit float array.
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralDerivativeSigmoid(const float * src, size_t size, const float * slope, float * dst);

    /*! @ingroup neural

        \fn void SimdNeuralDerivativeTanh(const float * src, size_t size, const float * slope, float * dst);

        \short Multiplies a 32-bit float array by the derivative of hyperbolic tangent values.

        \deprecated This function will be removed in the nearest future.

        For every element:
        \verbatim
        dst[i] *= slope[0]*(1 - src[i]*src[i]);
        \endverbatim

        \note This function is used in Simd::Neural::Function.

        \param [in] src - a pointer to tanh output values.
        \param [in] size - a size of arrays.
        \param [in] slope - a pointer to the slope parameter.
        \param [in, out] dst - a pointer to cumulative 32-bit float array.
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralDerivativeTanh(const float * src, size_t size, const float * slope, float * dst);

    /*! @ingroup neural

        \fn void SimdNeuralDerivativeRelu(const float * src, size_t size, const float * slope, float * dst);

        \short Multiplies a 32-bit float array by the derivative of ReLU values.

        \deprecated This function will be removed in the nearest future.

        For every element:
        \verbatim
        dst[i] *= src[i] > 0 ? 1 : slope[0];
        \endverbatim

        \note This function is used in Simd::Neural::Function.

        \param [in] src - a pointer to input values used to choose the derivative branch.
        \param [in] size - a size of arrays.
        \param [in] slope - a pointer to the slope parameter for non-positive values.
        \param [in, out] dst - a pointer to cumulative 32-bit float array.
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralDerivativeRelu(const float * src, size_t size, const float * slope, float * dst);

    /*! @ingroup neural

        \fn void SimdNeuralPow(const float * src, size_t size, const float * exponent, float * dst);

        \short Raises every 32-bit float array element to a scalar exponent.

        \deprecated This function will be removed in the nearest future.

        For every element:
        \verbatim
        dst[i] = Pow(src[i], exponent[0]);
        \endverbatim

        \note This function is used in Simd::Neural::Function.

        \param [in] src - a pointer to the input array.
        \param [in] size - a size of arrays.
        \param [in] exponent - a pointer to exponent parameter.
        \param [out] dst - a pointer to output array.
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralPow(const float * src, size_t size, const float * exponent, float * dst);

    /*! @ingroup neural

        \fn void SimdNeuralProductSum(const float * a, const float * b, size_t size, float * sum);

        \short Calculates the dot product of two 32-bit float arrays.

        \deprecated This function will be removed in the nearest future.

        For every element:
        \verbatim
        sum[0] = Sum(a[i]*b[i]);
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] a - a pointer to the first 32-bit float array.
        \param [in] b - a pointer to the second 32-bit float array.
        \param [in] size - a size of arrays.
        \param [out] sum - a pointer to 32-bit float dot product.
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralProductSum(const float * a, const float * b, size_t size, float * sum);

    /*! @ingroup neural

        \fn void SimdNeuralAddVectorMultipliedByValue(const float * src, size_t size, const float * value, float * dst);

        \short Adds a source vector multiplied by a scalar to a destination vector.

        \deprecated This function will be removed in the nearest future.

        For every element:
        \verbatim
        dst[i] += src[i]*value[0];
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float array.
        \param [in] size - a size of arrays.
        \param [in] value - a pointer to the scalar 32-bit float value.
        \param [in, out] dst - a pointer to cumulative 32-bit float array.
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddVectorMultipliedByValue(const float * src, size_t size, const float * value, float * dst);

    /*! @ingroup neural

        \fn void SimdNeuralAddVector(const float * src, size_t size, float * dst);

        \short Adds a source vector to a destination vector.

        \deprecated This function will be removed in the nearest future.

        For every element:
        \verbatim
        dst[i] += src[i];
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float array.
        \param [in] size - a size of the arrays.
        \param [in, out] dst - a pointer to cumulative 32-bit float array.
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddVector(const float * src, size_t size, float * dst);

    /*! @ingroup neural

        \fn void SimdNeuralAddValue(const float * value, float * dst, size_t size);

        \short Adds a scalar value to every element of a vector.

        \deprecated This function will be removed in the nearest future.

        For every element:
        \verbatim
        dst[i] += value[0];
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] value - a pointer to the scalar 32-bit float value.
        \param [in, out] dst - a pointer to cumulative 32-bit float array.
        \param [in] size - a size of the array.
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddValue(const float * value, float * dst, size_t size);

    /*! @ingroup neural

        \fn void SimdNeuralUpdateWeights(const float * x, size_t size, const float * a, const float * b, float * d, float * w);

        \short Updates weight increments and weights for a 32-bit float vector.

        \deprecated This function will be removed in the nearest future.

        For every element:
        \verbatim
        d[i] = a[0]*d[i] + b[0]*x[i];
        w[i] += d[i];
        \endverbatim

        \param [in] x - a pointer to the input X array.
        \param [in] size - a size of arrays.
        \param [in] a - a pointer to the first scalar parameter.
        \param [in] b - a pointer to the second scalar parameter.
        \param [in, out] d - a pointer to the D array.
        \param [in, out] w - a pointer to the W array.
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralUpdateWeights(const float * x, size_t size, const float * a, const float * b, float * d, float * w);

    /*! @ingroup neural

        \fn void SimdNeuralAdaptiveGradientUpdate(const float * delta, size_t size, size_t batch, const float * alpha, const float * epsilon, float * gradient, float * weight);

        \short Updates neural network weights by the adaptive gradient method.

        \deprecated This function will be removed in the nearest future.

        For every element:
        \verbatim
        d = delta[i]/batch;
        gradient[i] += d*d;
        weight[i] -= alpha[0]*d/Sqrt(gradient[i] + epsilon[0]);
        \endverbatim

        \note All arrays must have the same size. This function is used in Simd::Neural.

        \param [in] delta - a pointer to the array with error gradients.
        \param [in] size - a size of arrays.
        \param [in] batch - a batch size used to normalize delta.
        \param [in] alpha - a pointer to alpha parameter (update speed).
        \param [in] epsilon - a pointer to epsilon parameter (a small number used to avoid division by zero).
        \param [in, out] gradient - a pointer to the accumulated squared gradients.
        \param [in, out] weight - a pointer to the array with weights.
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAdaptiveGradientUpdate(const float * delta, size_t size, size_t batch, const float * alpha, const float * epsilon, float * gradient, float * weight);

    /*! @ingroup neural

        \fn void SimdNeuralAddConvolution2x2Forward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

        \short Adds a valid 2x2 convolution of a 32-bit float image to dst.

        \deprecated This function will be removed in the nearest future.

        For every output point:
        \verbatim
        dst[x, y] += Sum(src[x + kx, y + ky]*weights[ky*2 + kx]), 0 <= kx, ky < 2;
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] width - a width of the output image (input image width must be equal to output image width + 1).
        \param [in] height - a height of the output image (input image height must be equal to output image height + 1).
        \param [in] weights - a pointer to the array with weights (its size must be at least 4).
        \param [in, out] dst - a pointer to the output 32-bit float image.
        \param [in] dstStride - a row size of the output image (in 32-bit float values).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddConvolution2x2Forward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

    /*! @ingroup neural

        \fn void SimdNeuralAddConvolution3x3Forward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

        \short Adds a valid 3x3 convolution of a 32-bit float image to dst.

        \deprecated This function will be removed in the nearest future.

        For every output point:
        \verbatim
        dst[x, y] += Sum(src[x + kx, y + ky]*weights[ky*3 + kx]), 0 <= kx, ky < 3;
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] width - a width of the output image (input image width must be equal to output image width + 2).
        \param [in] height - a height of the output image (input image height must be equal to output image height + 2).
        \param [in] weights - a pointer to the array with weights (its size must be at least 9).
        \param [in, out] dst - a pointer to the output 32-bit float image.
        \param [in] dstStride - a row size of the output image (in 32-bit float values).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddConvolution3x3Forward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

    /*! @ingroup neural

        \fn void SimdNeuralAddConvolution4x4Forward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

        \short Adds a valid 4x4 convolution of a 32-bit float image to dst.

        \deprecated This function will be removed in the nearest future.

        For every output point:
        \verbatim
        dst[x, y] += Sum(src[x + kx, y + ky]*weights[ky*4 + kx]), 0 <= kx, ky < 4;
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] width - a width of the output image (input image width must be equal to output image width + 3).
        \param [in] height - a height of the output image (input image height must be equal to output image height + 3).
        \param [in] weights - a pointer to the array with weights (its size must be at least 16).
        \param [in, out] dst - a pointer to the output 32-bit float image.
        \param [in] dstStride - a row size of the output image (in 32-bit float values).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddConvolution4x4Forward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

    /*! @ingroup neural

        \fn void SimdNeuralAddConvolution5x5Forward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

        \short Adds a valid 5x5 convolution of a 32-bit float image to dst.

        \deprecated This function will be removed in the nearest future.

        For every output point:
        \verbatim
        dst[x, y] += Sum(src[x + kx, y + ky]*weights[ky*5 + kx]), 0 <= kx, ky < 5;
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] width - a width of the output image (input image width must be equal to output image width + 4).
        \param [in] height - a height of the output image (input image height must be equal to output image height + 4).
        \param [in] weights - a pointer to the array with weights (its size must be at least 25).
        \param [in, out] dst - a pointer to the output 32-bit float image.
        \param [in] dstStride - a row size of the output image (in 32-bit float values).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddConvolution5x5Forward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

    /*! @ingroup neural

        \fn void SimdNeuralAddConvolution2x2Backward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

        \short Adds a 2x2 transposed convolution contribution to dst.

        \deprecated This function will be removed in the nearest future.

        For every source point:
        \verbatim
        dst[x + kx, y + ky] += src[x, y]*weights[ky*2 + kx], 0 <= kx, ky < 2;
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] width - a width of the input image (output image width must be equal to input image width + 1).
        \param [in] height - a height of the input image (output image height must be equal to input image height + 1).
        \param [in] weights - a pointer to the array with weights (its size must be at least 4).
        \param [in, out] dst - a pointer to the output 32-bit float image.
        \param [in] dstStride - a row size of the output image (in 32-bit float values).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddConvolution2x2Backward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

    /*! @ingroup neural

        \fn void SimdNeuralAddConvolution3x3Backward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

        \short Adds a 3x3 transposed convolution contribution to dst.

        \deprecated This function will be removed in the nearest future.

        For every source point:
        \verbatim
        dst[x + kx, y + ky] += src[x, y]*weights[ky*3 + kx], 0 <= kx, ky < 3;
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] width - a width of the input image (output image width must be equal to input image width + 2).
        \param [in] height - a height of the input image (output image height must be equal to input image height + 2).
        \param [in] weights - a pointer to the array with weights (its size must be at least 9).
        \param [in, out] dst - a pointer to the output 32-bit float image.
        \param [in] dstStride - a row size of the output image (in 32-bit float values).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddConvolution3x3Backward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

    /*! @ingroup neural

        \fn void SimdNeuralAddConvolution4x4Backward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

        \short Adds a 4x4 transposed convolution contribution to dst.

        \deprecated This function will be removed in the nearest future.

        For every source point:
        \verbatim
        dst[x + kx, y + ky] += src[x, y]*weights[ky*4 + kx], 0 <= kx, ky < 4;
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] width - a width of the input image (output image width must be equal to input image width + 3).
        \param [in] height - a height of the input image (output image height must be equal to input image height + 3).
        \param [in] weights - a pointer to the array with weights (its size must be at least 16).
        \param [in, out] dst - a pointer to the output 32-bit float image.
        \param [in] dstStride - a row size of the output image (in 32-bit float values).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddConvolution4x4Backward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

    /*! @ingroup neural

        \fn void SimdNeuralAddConvolution5x5Backward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

        \short Adds a 5x5 transposed convolution contribution to dst.

        \deprecated This function will be removed in the nearest future.

        For every source point:
        \verbatim
        dst[x + kx, y + ky] += src[x, y]*weights[ky*5 + kx], 0 <= kx, ky < 5;
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] width - a width of the input image (output image width must be equal to input image width + 4).
        \param [in] height - a height of the input image (output image height must be equal to input image height + 4).
        \param [in] weights - a pointer to the array with weights (its size must be at least 25).
        \param [in, out] dst - a pointer to the output 32-bit float image.
        \param [in] dstStride - a row size of the output image (in 32-bit float values).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddConvolution5x5Backward(const float * src, size_t srcStride, size_t width, size_t height, const float * weights, float * dst, size_t dstStride);

    /*! @ingroup neural

        \fn void SimdNeuralAddConvolution2x2Sum(const float * src, size_t srcStride, const float * dst, size_t dstStride, size_t width, size_t height, float * sums);

        \short Accumulates 2x2 convolution weight gradients into sums.

        \deprecated This function will be removed in the nearest future.

        For every weight:
        \verbatim
        sums[ky*2 + kx] += Sum(src[x + kx, y + ky]*dst[x, y]);
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] dst - a pointer to the output-gradient 32-bit float image.
        \param [in] dstStride - a row size of the output-gradient image (in 32-bit float values).
        \param [in] width - a width of the output-gradient image (input image width must be equal to width + 1).
        \param [in] height - a height of the output-gradient image (input image height must be equal to height + 1).
        \param [in, out] sums - a pointer to the array with accumulated weight gradients (its size must be at least 4).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddConvolution2x2Sum(const float * src, size_t srcStride, const float * dst, size_t dstStride, size_t width, size_t height, float * sums);

    /*! @ingroup neural

        \fn void SimdNeuralAddConvolution3x3Sum(const float * src, size_t srcStride, const float * dst, size_t dstStride, size_t width, size_t height, float * sums);

        \short Accumulates 3x3 convolution weight gradients into sums.

        \deprecated This function will be removed in the nearest future.

        For every weight:
        \verbatim
        sums[ky*3 + kx] += Sum(src[x + kx, y + ky]*dst[x, y]);
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] dst - a pointer to the output-gradient 32-bit float image.
        \param [in] dstStride - a row size of the output-gradient image (in 32-bit float values).
        \param [in] width - a width of the output-gradient image (input image width must be equal to width + 2).
        \param [in] height - a height of the output-gradient image (input image height must be equal to height + 2).
        \param [in, out] sums - a pointer to the array with accumulated weight gradients (its size must be at least 9).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddConvolution3x3Sum(const float * src, size_t srcStride, const float * dst, size_t dstStride, size_t width, size_t height, float * sums);

    /*! @ingroup neural

        \fn void SimdNeuralAddConvolution4x4Sum(const float * src, size_t srcStride, const float * dst, size_t dstStride, size_t width, size_t height, float * sums);

        \short Accumulates 4x4 convolution weight gradients into sums.

        \deprecated This function will be removed in the nearest future.

        For every weight:
        \verbatim
        sums[ky*4 + kx] += Sum(src[x + kx, y + ky]*dst[x, y]);
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] dst - a pointer to the output-gradient 32-bit float image.
        \param [in] dstStride - a row size of the output-gradient image (in 32-bit float values).
        \param [in] width - a width of the output-gradient image (input image width must be equal to width + 3).
        \param [in] height - a height of the output-gradient image (input image height must be equal to height + 3).
        \param [in, out] sums - a pointer to the array with accumulated weight gradients (its size must be at least 16).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddConvolution4x4Sum(const float * src, size_t srcStride, const float * dst, size_t dstStride, size_t width, size_t height, float * sums);

    /*! @ingroup neural

        \fn void SimdNeuralAddConvolution5x5Sum(const float * src, size_t srcStride, const float * dst, size_t dstStride, size_t width, size_t height, float * sums);

        \short Accumulates 5x5 convolution weight gradients into sums.

        \deprecated This function will be removed in the nearest future.

        For every weight:
        \verbatim
        sums[ky*5 + kx] += Sum(src[x + kx, y + ky]*dst[x, y]);
        \endverbatim

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] dst - a pointer to the output-gradient 32-bit float image.
        \param [in] dstStride - a row size of the output-gradient image (in 32-bit float values).
        \param [in] width - a width of the output-gradient image (input image width must be equal to width + 4).
        \param [in] height - a height of the output-gradient image (input image height must be equal to height + 4).
        \param [in, out] sums - a pointer to the array with accumulated weight gradients (its size must be at least 25).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralAddConvolution5x5Sum(const float * src, size_t srcStride, const float * dst, size_t dstStride, size_t width, size_t height, float * sums);

    /*! @ingroup neural

        \fn void SimdNeuralPooling1x1Max3x3(const float * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride);

        \short Performs stride-1 max pooling with a clipped 3x3 window.

        \deprecated This function will be removed in the nearest future.

        The output image has the same width and height as the input image. For inner pixels the
        function uses a 3x3 window; at image borders it uses only valid input pixels.

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] width - a width of the input and output images.
        \param [in] height - a height of the input and output images.
        \param [out] dst - a pointer to the output 32-bit float image.
        \param [in] dstStride - a row size of the output image (in 32-bit float values).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralPooling1x1Max3x3(const float * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride);

    /*! @ingroup neural

        \fn void SimdNeuralPooling2x2Max2x2(const float * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride);

        \short Performs stride-2 max pooling with a clipped 2x2 window.

        \deprecated This function will be removed in the nearest future.

        The output image size is (width + 1)/2 by (height + 1)/2. Full 2x2 windows are used where
        available; the last row or column uses only valid input pixels when width or height is odd.

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] width - a width of the input image.
        \param [in] height - a height of the input image.
        \param [out] dst - a pointer to the output 32-bit float image.
        \param [in] dstStride - a row size of the output image (in 32-bit float values).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralPooling2x2Max2x2(const float * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride);

    /*! @ingroup neural

        \fn void SimdNeuralPooling2x2Max3x3(const float * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride);

        \short Performs stride-2 max pooling with a clipped 3x3 window.

        The output image size is width/2 by height/2. Full 3x3 windows are used where available;
        windows touching the last output row or column use only valid input pixels.

        \note This function is used in Simd::Neural.

        \param [in] src - a pointer to the input 32-bit float image.
        \param [in] srcStride - a row size of the input image (in 32-bit float values).
        \param [in] width - a width of the input image.
        \param [in] height - a height of the input image.
        \param [out] dst - a pointer to the output 32-bit float image.
        \param [in] dstStride - a row size of the output image (in 32-bit float values).
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralPooling2x2Max3x3(const float * src, size_t srcStride, size_t width, size_t height, float * dst, size_t dstStride);

    /*! @ingroup neural

        \fn void SimdNeuralConvolutionForward(const float * src, size_t srcWidth, size_t srcHeight, size_t srcDepth, const float * weight, size_t kernelX, size_t kernelY, size_t padX, size_t padY, size_t strideX, size_t strideY, size_t dilationX, size_t dilationY, void * buffer, size_t * size, float * dst, size_t dstWidth, size_t dstHeight, size_t dstDepth, int add);

        \short Performs forward convolution for NCHW-style 32-bit float tensors.

        \deprecated This function will be removed in the nearest future.

        The source tensor is stored as srcDepth planes of size srcHeight*srcWidth. The destination
        tensor is stored as dstDepth planes of size dstHeight*dstWidth. The weight tensor is stored as
        dstDepth filters, each containing srcDepth*kernelY*kernelX values. Input samples outside the
        source image because of padding are treated as zero.

        For every output channel od and output point (dx, dy):
        \verbatim
        if(!add)
            dst[od, dy, dx] = 0;
        for(id = 0; id < srcDepth; ++id)
            for(ky = 0; ky < kernelY; ++ky)
                for(kx = 0; kx < kernelX; ++kx)
                {
                    sx = dx*strideX + kx*dilationX - padX;
                    sy = dy*strideY + ky*dilationY - padY;
                    if(0 <= sx && sx < srcWidth && 0 <= sy && sy < srcHeight)
                        dst[od, dy, dx] += src[id, sy, sx]*weight[od, id, ky, kx];
                }
        \endverbatim

        The output dimensions must satisfy:
        \verbatim
        dstWidth = (srcWidth + 2*padX - (dilationX*(kernelX - 1) + 1))/strideX + 1;
        dstHeight = (srcHeight + 2*padY - (dilationY*(kernelY - 1) + 1))/strideY + 1;
        \endverbatim

        If buffer and size provide a large enough temporary buffer, it can be used by the algorithm;
        otherwise an internal buffer is allocated. When size is not NULL and the supplied buffer is too
        small, size[0] is updated with the required size in bytes.

        \param [in] src - a pointer to the input tensor. Total size is srcWidth*srcHeight*srcDepth.
        \param [in] srcWidth - a width of the input tensor.
        \param [in] srcHeight - a height of the input tensor.
        \param [in] srcDepth - a number of channels in the input tensor.
        \param [in] weight - a pointer to the convolution weights. Total size is kernelX*kernelY*srcDepth*dstDepth.
        \param [in] kernelX - a width of the convolution kernel.
        \param [in] kernelY - a height of the convolution kernel.
        \param [in] padX - a pad to the x-coordinate of the input tensor.
        \param [in] padY - a pad to the y-coordinate of the input tensor.
        \param [in] strideX - a x-stride of the convolution.
        \param [in] strideY - a y-stride of the convolution.
        \param [in] dilationX - a x-dilation of the convolution.
        \param [in] dilationY - a y-dilation of the convolution.
        \param [in, out] buffer - a pointer to an optional external temporary buffer. Can be NULL.
        \param [in, out] size - a pointer to the size of the external temporary buffer. Can be NULL.
        \param [in, out] dst - a pointer to the output tensor. Total size is dstWidth*dstHeight*dstDepth.
        \param [in] dstWidth - a width of the output tensor.
        \param [in] dstHeight - a height of the output tensor.
        \param [in] dstDepth - a number of channels in the output tensor.
        \param [in] add - a flag: if non-zero, convolution is added to dst; otherwise dst is cleared before accumulation.
    */
    SIMD_DEPRECATED SIMD_API void SimdNeuralConvolutionForward(const float * src, size_t srcWidth, size_t srcHeight, size_t srcDepth, const float * weight, size_t kernelX, size_t kernelY, size_t padX, size_t padY, size_t strideX, size_t strideY, size_t dilationX, size_t dilationY, void * buffer, size_t * size, float * dst, size_t dstWidth, size_t dstHeight, size_t dstDepth, int add);

    /*! @ingroup operation

        \fn void SimdOperationBinary8u(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride, size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride, SimdOperationBinary8uType type);

        \short Performs an element-wise binary operation between two 8-bit images.

        All images must have the same width, height and number of channels. The function processes
        width*channelCount unsigned 8-bit values in every row; every channel is handled independently.
        The exact operation is selected by \a type (average, bitwise AND/OR, maximum, minimum,
        saturated subtraction or saturated addition).

        \note This function has a C++ wrapper: Simd::OperationBinary8u(const View<A>& a, const View<A>& b, View<A>& dst, SimdOperationBinary8uType type).

        \param [in] a - a pointer to pixels data of the first input image.
        \param [in] aStride - a row size of the first image.
        \param [in] b - a pointer to pixels data of the second input image.
        \param [in] bStride - a row size of the second image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [out] dst - a pointer to pixels data of the output image.
        \param [in] dstStride - a row size of the output image.
        \param [in] type - a type of operation (see ::SimdOperationBinary8uType).
    */
    SIMD_API void SimdOperationBinary8u(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride,
        size_t width, size_t height, size_t channelCount, uint8_t * dst, size_t dstStride, SimdOperationBinary8uType type);

    /*! @ingroup operation

        \fn void SimdOperationBinary16i(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride, size_t width, size_t height, uint8_t * dst, size_t dstStride, SimdOperationBinary16iType type);

        \short Performs an element-wise binary operation between two signed 16-bit images.

        All images must have the same width, height and ::SimdPixelFormatInt16 pixel format. The
        function treats every row as an array of width signed 16-bit values and applies the non-saturated
        operation selected by \a type (addition or subtraction). Strides are specified in bytes and
        must be multiples of sizeof(int16_t).

        \note This function has a C++ wrapper: Simd::OperationBinary16i(const View<A>& a, const View<A>& b, View<A>& dst, SimdOperationBinary16iType type).

        \param [in] a - a pointer to pixels data of the first input image.
        \param [in] aStride - a row size of the first image.
        \param [in] b - a pointer to pixels data of the second input image.
        \param [in] bStride - a row size of the second image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of the output image.
        \param [in] dstStride - a row size of the output image.
        \param [in] type - a type of operation (see ::SimdOperationBinary16iType).
    */
    SIMD_API void SimdOperationBinary16i(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride,
        size_t width, size_t height, uint8_t * dst, size_t dstStride, SimdOperationBinary16iType type);

    /*! @ingroup operation

        \fn void SimdVectorProduct(const uint8_t * vertical, const uint8_t * horizontal, uint8_t * dst, size_t stride, size_t width, size_t height);

        \short Calculates an 8-bit gray image as the normalized outer product of two 8-bit vectors.

        \deprecated This function will be removed in the nearest future.

        For all points:
        \verbatim
        dst[x, y] = DivideBy255(horizontal[x]*vertical[y]);
        where DivideBy255(v) = (v + 1 + (v >> 8)) >> 8.
        \endverbatim

        \note This function has a C++ wrapper: Simd::VectorProduct(const uint8_t * vertical, const uint8_t * horizontal, View<A>& dst).

        \param [in] vertical - a pointer to the vertical vector. Its length must be equal to the output image height.
        \param [in] horizontal - a pointer to the horizontal vector. Its length must be equal to the output image width.
        \param [out] dst - a pointer to pixels data of the output image.
        \param [in] stride - a row size of the output image in bytes.
        \param [in] width - a width of the output image.
        \param [in] height - a height of the output image.
    */
    SIMD_DEPRECATED SIMD_API void SimdVectorProduct(const uint8_t * vertical, const uint8_t * horizontal,
        uint8_t * dst, size_t stride, size_t width, size_t height);

    /*! @ingroup recursive_bilateral_filter

        \fn void * SimdRecursiveBilateralFilterInit(size_t width, size_t height, size_t channels, const float* sigmaSpatial, const float* sigmaRange, SimdRecursiveBilateralFilterFlags flags);

        \short Creates a recursive bilateral filter context for 8-bit images.

        The context stores image size, channel count, spatial sigma, range sigma and implementation
        flags. It is then reused by ::SimdRecursiveBilateralFilterRun to filter images with the same
        width, height and number of channels. The filter supports 1, 2, 3 or 4 channels. The spatial
        and range sigma values are normalized to the 8-bit range internally. The \a flags argument
        selects fast or precise processing and the color-difference mode (see
        ::SimdRecursiveBilateralFilterFlags).

        Usage example:
        \verbatim
        float sigmaSpatial = 0.2f, sigmaRange = 0.2f;
        void* filter = SimdRecursiveBilateralFilterInit(width, height, channels, &sigmaSpatial, &sigmaRange, SimdRecursiveBilateralFilterFast);
        if (filter)
        {
             SimdRecursiveBilateralFilterRun(filter, src, srcStride, dst, dstStride);
             SimdRelease(filter);
        }
        \endverbatim

        \param [in] width - a width of input and output images.
        \param [in] height - a height of input and output images.
        \param [in] channels - a number of channels in input and output images. It must be in range [1..4].
        \param [in] sigmaSpatial - a pointer to the spatial sigma parameter.
        \param [in] sigmaRange - a pointer to the range sigma parameter.
        \param [in] flags - algorithm flags (see ::SimdRecursiveBilateralFilterFlags).
        \return a pointer to filter context. On error it returns NULL.
                This pointer is used by function ::SimdRecursiveBilateralFilterRun.
                It must be released with function ::SimdRelease.
    */
    SIMD_API void* SimdRecursiveBilateralFilterInit(size_t width, size_t height, size_t channels, 
        const float* sigmaSpatial, const float* sigmaRange, SimdRecursiveBilateralFilterFlags flags);

    /*! @ingroup recursive_bilateral_filter

        \fn void SimdRecursiveBilateralFilterRun(const void* filter, const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);

        \short Runs a recursive bilateral filter for one 8-bit image.

        The input and output images must have the width, height and channel count that were used to
        create \a filter. The source and destination buffers may have different strides. The filter
        performs horizontal and vertical recursive passes and writes the final smoothed image to \a dst.

        \param [in] filter - a filter context. It must be created by function ::SimdRecursiveBilateralFilterInit and released by function ::SimdRelease.
        \param [in] src - a pointer to pixels data of the original input image.
        \param [in] srcStride - a row size (in bytes) of the input image.
        \param [out] dst - a pointer to pixels data of the filtered output image.
        \param [in] dstStride - a row size (in bytes) of the output image.
    */
    SIMD_API void SimdRecursiveBilateralFilterRun(const void* filter, const uint8_t* src, size_t srcStride, uint8_t* dst, size_t dstStride);

    /*! @ingroup resizing

        \fn void SimdReduceColor2x2(const uint8_t * src, size_t srcWidth, size_t srcHeight, size_t srcStride, uint8_t * dst, size_t dstWidth, size_t dstHeight, size_t dstStride, size_t channelCount);

        \short Reduces a multi-channel 8-bit image by two using a 2x2 averaging window.

        The output size must be: dstWidth = (srcWidth + 1)/2, dstHeight = (srcHeight + 1)/2.
        The channel count must be 1, 2, 3 or 4. Border pixels are replicated when the source width
        or height is odd.

        For all points:
        \verbatim
        sx0 = 2*x; sx1 = Min(2*x + 1, srcWidth - 1);
        sy0 = 2*y; sy1 = Min(2*y + 1, srcHeight - 1);
        dst[x, y, c] = (src[sx0, sy0, c] + src[sx1, sy0, c] +
                        src[sx0, sy1, c] + src[sx1, sy1, c] + 2)/4;
        \endverbatim

        \note This function has a C++ wrapper: Simd::Reduce2x2(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of the original input image.
        \param [in] srcWidth - a width of the input image.
        \param [in] srcHeight - a height of the input image.
        \param [in] srcStride - a row size of the input image.
        \param [out] dst - a pointer to pixels data of the reduced output image.
        \param [in] dstWidth - a width of the output image.
        \param [in] dstHeight - a height of the output image.
        \param [in] dstStride - a row size of the output image.
        \param [in] channelCount - a number of channels for input and output images.
    */
    SIMD_API void SimdReduceColor2x2(const uint8_t * src, size_t srcWidth, size_t srcHeight, size_t srcStride,
        uint8_t * dst, size_t dstWidth, size_t dstHeight, size_t dstStride, size_t channelCount);

    /*! @ingroup resizing

        \fn void SimdReduceGray2x2(const uint8_t * src, size_t srcWidth, size_t srcHeight, size_t srcStride, uint8_t * dst, size_t dstWidth, size_t dstHeight, size_t dstStride);

        \short Reduces an 8-bit gray image by two using a 2x2 averaging window.

        The output size must be: dstWidth = (srcWidth + 1)/2, dstHeight = (srcHeight + 1)/2.
        Border pixels are replicated when the source width or height is odd.

        For all points:
        \verbatim
        sx0 = 2*x; sx1 = Min(2*x + 1, srcWidth - 1);
        sy0 = 2*y; sy1 = Min(2*y + 1, srcHeight - 1);
        dst[x, y] = (src[sx0, sy0] + src[sx1, sy0] + src[sx0, sy1] + src[sx1, sy1] + 2)/4;
        \endverbatim

        \note This function has a C++ wrapper: Simd::ReduceGray2x2(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of the original input image.
        \param [in] srcWidth - a width of the input image.
        \param [in] srcHeight - a height of the input image.
        \param [in] srcStride - a row size of the input image.
        \param [out] dst - a pointer to pixels data of the reduced output image.
        \param [in] dstWidth - a width of the output image.
        \param [in] dstHeight - a height of the output image.
        \param [in] dstStride - a row size of the output image.
    */
    SIMD_API void SimdReduceGray2x2(const uint8_t * src, size_t srcWidth, size_t srcHeight, size_t srcStride,
        uint8_t * dst, size_t dstWidth, size_t dstHeight, size_t dstStride);

    /*! @ingroup resizing

        \fn void SimdReduceGray3x3(const uint8_t * src, size_t srcWidth, size_t srcHeight, size_t srcStride, uint8_t * dst, size_t dstWidth, size_t dstHeight, size_t dstStride, int compensation);

        \short Reduces an 8-bit gray image by two using a separable 3x3 Gaussian window.

        The output size must be: dstWidth = (srcWidth + 1)/2, dstHeight = (srcHeight + 1)/2.
        The filter uses kernel [1 2 1] horizontally and vertically. Source coordinates outside the
        image are clamped to the nearest valid pixel. If \a compensation is non-zero, the sum is
        rounded by adding 8 before division by 16; otherwise it is truncated.

        For every point:
        \verbatim
        k = [1, 2, 1];
        sx(i) = Clamp(2*x + i - 1, 0, srcWidth - 1);
        sy(j) = Clamp(2*y + j - 1, 0, srcHeight - 1);
        sum = Sum(k[i]*k[j]*src[sx(i), sy(j)]), 0 <= i,j < 3;
        dst[x, y] = (sum + (compensation ? 8 : 0)) / 16;
        \endverbatim

        \note This function has a C++ wrapper: Simd::ReduceGray3x3(const View<A>& src, View<A>& dst, bool compensation).

        \param [in] src - a pointer to pixels data of the original input image.
        \param [in] srcWidth - a width of the input image.
        \param [in] srcHeight - a height of the input image.
        \param [in] srcStride - a row size of the input image.
        \param [out] dst - a pointer to pixels data of the reduced output image.
        \param [in] dstWidth - a width of the output image.
        \param [in] dstHeight - a height of the output image.
        \param [in] dstStride - a row size of the output image.
        \param [in] compensation - a flag to enable rounding compensation before division.
    */
    SIMD_API void SimdReduceGray3x3(const uint8_t * src, size_t srcWidth, size_t srcHeight, size_t srcStride,
        uint8_t * dst, size_t dstWidth, size_t dstHeight, size_t dstStride, int compensation);

    /*! @ingroup resizing

        \fn void SimdReduceGray4x4(const uint8_t * src, size_t srcWidth, size_t srcHeight, size_t srcStride, uint8_t * dst, size_t dstWidth, size_t dstHeight, size_t dstStride);

        \short Reduces an 8-bit gray image by two using a separable 4x4 Gaussian-like window.

        The output size must be: dstWidth = (srcWidth + 1)/2, dstHeight = (srcHeight + 1)/2.
        The filter uses kernel [1 3 3 1] horizontally and vertically and rounds by adding 32 before
        division by 64. Source coordinates outside the image are clamped to the nearest valid pixel.

        For every point:
        \verbatim
        k = [1, 3, 3, 1];
        sx(i) = Clamp(2*x + i - 1, 0, srcWidth - 1);
        sy(j) = Clamp(2*y + j - 1, 0, srcHeight - 1);
        sum = Sum(k[i]*k[j]*src[sx(i), sy(j)]), 0 <= i,j < 4;
        dst[x, y] = (sum + 32) / 64;
        \endverbatim

        \note This function has a C++ wrapper: Simd::ReduceGray4x4(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of the original input image.
        \param [in] srcWidth - a width of the input image.
        \param [in] srcHeight - a height of the input image.
        \param [in] srcStride - a row size of the input image.
        \param [out] dst - a pointer to pixels data of the reduced output image.
        \param [in] dstWidth - a width of the output image.
        \param [in] dstHeight - a height of the output image.
        \param [in] dstStride - a row size of the output image.
    */
    SIMD_API void SimdReduceGray4x4(const uint8_t * src, size_t srcWidth, size_t srcHeight, size_t srcStride,
        uint8_t * dst, size_t dstWidth, size_t dstHeight, size_t dstStride);

    /*! @ingroup resizing

        \fn void SimdReduceGray5x5(const uint8_t * src, size_t srcWidth, size_t srcHeight, size_t srcStride, uint8_t * dst, size_t dstWidth, size_t dstHeight, size_t dstStride, int compensation);

        \short Reduces an 8-bit gray image by two using a separable 5x5 Gaussian window.

        The output size must be: dstWidth = (srcWidth + 1)/2, dstHeight = (srcHeight + 1)/2.
        The filter uses kernel [1 4 6 4 1] horizontally and vertically. Source coordinates outside
        the image are clamped to the nearest valid pixel. If \a compensation is non-zero, the sum is
        rounded by adding 128 before division by 256; otherwise it is truncated.

        For every point:
        \verbatim
        k = [1, 4, 6, 4, 1];
        sx(i) = Clamp(2*x + i - 2, 0, srcWidth - 1);
        sy(j) = Clamp(2*y + j - 2, 0, srcHeight - 1);
        sum = Sum(k[i]*k[j]*src[sx(i), sy(j)]), 0 <= i,j < 5;
        dst[x, y] = (sum + (compensation ? 128 : 0)) / 256;
        \endverbatim

        \note This function has a C++ wrapper: Simd::ReduceGray5x5(const View<A>& src, View<A>& dst, bool compensation).

        \param [in] src - a pointer to pixels data of the original input image.
        \param [in] srcWidth - a width of the input image.
        \param [in] srcHeight - a height of the input image.
        \param [in] srcStride - a row size of the input image.
        \param [out] dst - a pointer to pixels data of the reduced output image.
        \param [in] dstWidth - a width of the output image.
        \param [in] dstHeight - a height of the output image.
        \param [in] dstStride - a row size of the output image.
        \param [in] compensation - a flag to enable rounding compensation before division.
    */
    SIMD_API void SimdReduceGray5x5(const uint8_t * src, size_t srcWidth, size_t srcHeight, size_t srcStride,
        uint8_t * dst, size_t dstWidth, size_t dstHeight, size_t dstStride, int compensation);

    /*! @ingroup reordering

        \fn void SimdReorder16bit(const uint8_t * src, size_t size, uint8_t * dst);

        \short Reverses byte order inside every 16-bit element of a data array.

        This function changes endian representation of each 2-byte element independently. The order
        of elements in the array is not changed.

        For every 2 bytes:
        \verbatim
        dst[2*i + 0] = src[2*i + 1];
        dst[2*i + 1] = src[2*i + 0];
        \endverbatim

        The data size must be a multiple of 2 bytes.

        \param [in] src - a pointer to the input data.
        \param [in] size - a size of input and output data in bytes.
        \param [out] dst - a pointer to the output data.
    */
    SIMD_API void SimdReorder16bit(const uint8_t * src, size_t size, uint8_t * dst);

    /*! @ingroup reordering

        \fn void SimdReorder32bit(const uint8_t * src, size_t size, uint8_t * dst);

        \short Reverses byte order inside every 32-bit element of a data array.

        This function changes endian representation of each 4-byte element independently. The order
        of elements in the array is not changed.

        For every 4 bytes:
        \verbatim
        dst[4*i + 0] = src[4*i + 3];
        dst[4*i + 1] = src[4*i + 2];
        dst[4*i + 2] = src[4*i + 1];
        dst[4*i + 3] = src[4*i + 0];
        \endverbatim

        The data size must be a multiple of 4 bytes.

        \param [in] src - a pointer to the input data.
        \param [in] size - a size of input and output data in bytes.
        \param [out] dst - a pointer to the output data.
    */
    SIMD_API void SimdReorder32bit(const uint8_t * src, size_t size, uint8_t * dst);

    /*! @ingroup reordering

        \fn void SimdReorder64bit(const uint8_t * src, size_t size, uint8_t * dst);

        \short Reverses byte order inside every 64-bit element of a data array.

        This function changes endian representation of each 8-byte element independently. The order
        of elements in the array is not changed.

        For every 8 bytes:
        \verbatim
        dst[8*i + 0] = src[8*i + 7];
        dst[8*i + 1] = src[8*i + 6];
        dst[8*i + 2] = src[8*i + 5];
        dst[8*i + 3] = src[8*i + 4];
        dst[8*i + 4] = src[8*i + 3];
        dst[8*i + 5] = src[8*i + 2];
        dst[8*i + 6] = src[8*i + 1];
        dst[8*i + 7] = src[8*i + 0];
        \endverbatim

        The data size must be a multiple of 8 bytes.

        \param [in] src - a pointer to the input data.
        \param [in] size - a size of input and output data in bytes.
        \param [out] dst - a pointer to the output data.
    */
    SIMD_API void SimdReorder64bit(const uint8_t * src, size_t size, uint8_t * dst);

    /*! @ingroup resizing

        \fn void * SimdResizerInit(size_t srcX, size_t srcY, size_t dstX, size_t dstY, size_t channels, SimdResizeChannelType type, SimdResizeMethodType method);

        \short Creates a reusable image resize context.

        The context stores source size, destination size, channel count, channel type and resize
        method. It precomputes interpolation indices and coefficients used by ::SimdResizerRun.
        Supported combinations are selected from real implementations: nearest methods for all
        channel types; bilinear for byte, short, float and BFloat16 channels; OpenCV-compatible
        bilinear, bicubic and area methods for byte channels; Caffe and PyTorch bilinear variants
        for float and BFloat16 channels. Unsupported combinations return NULL.

        Usage example (resize of RGBA64 image):
        \verbatim
        void * resizer = SimdResizerInit(srcX, srcY, dstX, dstY, 4, SimdResizeChannelShort, SimdResizeMethodBilinear);
        if (resizer)
        {
             SimdResizerRun(resizer, (uint8_t*)src, srcStride, (uint8_t*)dst, dstStride);
             SimdRelease(resizer);
        }
        \endverbatim

        \param [in] srcX - a width of the input image.
        \param [in] srcY - a height of the input image.
        \param [in] dstX - a width of the output image.
        \param [in] dstY - a height of the output image.
        \param [in] channels - a number of channels in input and output images.
        \param [in] type - a type of input and output image channel (see ::SimdResizeChannelType).
        \param [in] method - a resize method (see ::SimdResizeMethodType).
        \return a pointer to resize context. On error or unsupported parameter combination it returns NULL.
                This pointer is used by function ::SimdResizerRun.
                It must be released with function ::SimdRelease.
    */
    SIMD_API void * SimdResizerInit(size_t srcX, size_t srcY, size_t dstX, size_t dstY, size_t channels, SimdResizeChannelType type, SimdResizeMethodType method);

    /*! @ingroup resizing

        \fn void SimdResizerRun(const void * resizer, const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);

        \short Resizes one image using a context created by ::SimdResizerInit.

        The input and output images must have the sizes, channel count, channel type and resize method
        stored in \a resizer. Strides are specified in bytes. The context can be reused for multiple
        images with the same geometry and format parameters.

        \param [in] resizer - a resize context. It must be created by function ::SimdResizerInit and released by function ::SimdRelease.
        \param [in] src - a pointer to pixels data of the original input image.
        \param [in] srcStride - a row size (in bytes) of the input image.
        \param [out] dst - a pointer to pixels data of the resized output image.
        \param [in] dstStride - a row size (in bytes) of the output image.
    */
    SIMD_API void SimdResizerRun(const void * resizer, const uint8_t * src, size_t srcStride, uint8_t * dst, size_t dstStride);

    /*! @ingroup rgb_conversion

        \fn void SimdRgbToBgra(const uint8_t * rgb, size_t width, size_t height, size_t rgbStride, uint8_t * bgra, size_t bgraStride, uint8_t alpha);

        \short Converts a 24-bit RGB image to a 32-bit BGRA image.

        All images must have the same width and height.
        For every pixel:
        \verbatim
        bgra[4*i + 0] = rgb[3*i + 2]; // blue
        bgra[4*i + 1] = rgb[3*i + 1]; // green
        bgra[4*i + 2] = rgb[3*i + 0]; // red
        bgra[4*i + 3] = alpha;
        \endverbatim
        If the input is treated as BGR and the output as RGBA, the same byte shuffle performs
        BGR-to-RGBA conversion.

        \note This function has C++ wrappers: Simd::RgbToBgra(const View<A>& rgb, View<A>& bgra, uint8_t alpha)
            and Simd::BgrToRgba(const View<A>& bgr, View<A>& rgba, uint8_t alpha).

        \param [in] rgb - a pointer to pixels data of input 24-bit RGB image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] rgbStride - a row size of the rgb image in bytes.
        \param [out] bgra - a pointer to pixels data of output 32-bit BGRA image.
        \param [in] bgraStride - a row size of the bgra image in bytes.
        \param [in] alpha - a value of alpha channel.
    */
    SIMD_API void SimdRgbToBgra(const uint8_t* rgb, size_t width, size_t height, size_t rgbStride, uint8_t* bgra, size_t bgraStride, uint8_t alpha);

    /*! @ingroup rgb_conversion

        \fn void SimdRgbToGray(const uint8_t * rgb, size_t width, size_t height, size_t rgbStride, uint8_t * gray, size_t grayStride);

        \short Converts a 24-bit RGB image to an 8-bit gray image.

        All images must have the same width and height.
        For every pixel:
        \verbatim
        gray[i] = (0.299*R + 0.587*G + 0.114*B) rounded to nearest integer,
        where R = rgb[3*i + 0], G = rgb[3*i + 1], B = rgb[3*i + 2].
        \endverbatim

        \note This function has a C++ wrapper Simd::RgbToGray(const View<A>& rgb, View<A>& gray).

        \param [in] rgb - a pointer to pixels data of input 24-bit RGB image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] rgbStride - a row size of the rgb image in bytes.
        \param [out] gray - a pointer to pixels data of output 8-bit gray image.
        \param [in] grayStride - a row size of the gray image in bytes.
    */
    SIMD_API void SimdRgbToGray(const uint8_t* rgb, size_t width, size_t height, size_t rgbStride, uint8_t* gray, size_t grayStride);

    /*! @ingroup rgba_conversion

        \fn void SimdRgbaToGray(const uint8_t * rgba, size_t width, size_t height, size_t rgbaStride, uint8_t * gray, size_t grayStride);

        \short Converts a 32-bit RGBA image to an 8-bit gray image.

        All images must have the same width and height.
        For every pixel:
        \verbatim
        gray[i] = (0.299*R + 0.587*G + 0.114*B) rounded to nearest integer,
        where R = rgba[4*i + 0], G = rgba[4*i + 1], B = rgba[4*i + 2].
        The alpha channel is ignored.
        \endverbatim

        \note This function has a C++ wrapper Simd::RgbaToGray(const View<A>& rgba, View<A>& gray).

        \param [in] rgba - a pointer to pixels data of input 32-bit RGBA image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] rgbaStride - a row size of the rgba image in bytes.
        \param [out] gray - a pointer to pixels data of output 8-bit gray image.
        \param [in] grayStride - a row size of the gray image in bytes.
    */
    SIMD_API void SimdRgbaToGray(const uint8_t* rgba, size_t width, size_t height, size_t rgbaStride, uint8_t* gray, size_t grayStride);

    /*! @ingroup segmentation

        \fn void SimdSegmentationChangeIndex(uint8_t * mask, size_t stride, size_t width, size_t height, uint8_t oldIndex, uint8_t newIndex);

        \short Replaces one segmentation index with another inside a mask.

        The mask must have 8-bit gray pixel format. Only pixels equal to \a oldIndex are modified;
        all other pixels keep their values.

        For every point:
        \verbatim
        if(mask[i] == oldIndex)
            mask[i] = newIndex;
        \endverbatim

        \note This function has a C++ wrappers: Simd::SegmentationChangeIndex(View<A> & mask, uint8_t oldIndex, uint8_t newIndex).

        \param [in, out] mask - a pointer to pixels data of 8-bit gray mask image.
        \param [in] stride - a row size of the mask image in bytes.
        \param [in] width - a mask width.
        \param [in] height - a mask height.
        \param [in] oldIndex - an index value to replace.
        \param [in] newIndex - a replacement index value.
    */
    SIMD_API void SimdSegmentationChangeIndex(uint8_t * mask, size_t stride, size_t width, size_t height, uint8_t oldIndex, uint8_t newIndex);

    /*! @ingroup segmentation

        \fn void SimdSegmentationFillSingleHoles(uint8_t * mask, size_t stride, size_t width, size_t height, uint8_t index);

        \short Fills isolated single-pixel holes in a segmentation mask.

        The mask must have 8-bit gray pixel format. Only inner pixels are tested; border pixels are
        not changed. An inner pixel is set to \a index when its upper, lower, left and right neighbors
        are all equal to \a index.

        For every inner point:
        \verbatim
        if(mask[x, y - 1] == index && mask[x, y + 1] == index &&
           mask[x - 1, y] == index && mask[x + 1, y] == index)
            mask[x, y] = index;
        \endverbatim

        \note This function has a C++ wrappers: Simd::SegmentationFillSingleHoles(View<A> & mask, uint8_t index).

        \param [in, out] mask - a pointer to pixels data of 8-bit gray mask image.
        \param [in] stride - a row size of the mask image in bytes.
        \param [in] width - a mask width.
        \param [in] height - a mask height.
        \param [in] index - a mask index used to fill single-pixel holes.
    */
    SIMD_API void SimdSegmentationFillSingleHoles(uint8_t * mask, size_t stride, size_t width, size_t height, uint8_t index);

    /*! @ingroup segmentation

        \fn void SimdSegmentationPropagate2x2(const uint8_t * parent, size_t parentStride, size_t width, size_t height, uint8_t * child, size_t childStride, const uint8_t * difference, size_t differenceStride, uint8_t currentIndex, uint8_t invalidIndex, uint8_t emptyIndex, uint8_t differenceThreshold);

        \short Propagates a segmentation index from a parent mask pyramid level to a child level.

        The parent and child sizes must satisfy: parentWidth = (childWidth + 1)/2,
        parentHeight = (childHeight + 1)/2. All images must have 8-bit gray pixel format, and the
        difference image must have the same size as the child image. The function scans each 2x2
        parent window and updates the corresponding inner 2x2 child pixels. A child pixel is updated
        only when its current value is less than \a invalidIndex. It becomes \a currentIndex when all
        four parent pixels equal \a currentIndex, or when at least one parent pixel equals \a currentIndex
        and the corresponding difference pixel is greater than \a differenceThreshold. Otherwise it
        becomes \a emptyIndex.

        \note This function has a C++ wrappers: Simd::SegmentationPropagate2x2(const View<A> & parent, View<A> & child, const View<A> & difference, uint8_t currentIndex, uint8_t invalidIndex, uint8_t emptyIndex, uint8_t thresholdDifference).

        \param [in] parent - a pointer to pixels data of 8-bit gray parent mask image.
        \param [in] parentStride - a row size of the parent mask image in bytes.
        \param [in] width - a parent mask width. It must be at least 2.
        \param [in] height - a parent mask height. It must be at least 2.
        \param [in, out] child - a pointer to pixels data of 8-bit gray child mask image.
        \param [in] childStride - a row size of the child mask image in bytes.
        \param [in] difference - a pointer to pixels data of 8-bit gray difference image.
        \param [in] differenceStride - a row size of the difference image in bytes.
        \param [in] currentIndex - a mask index to propagate.
        \param [in] invalidIndex - a minimum value of child pixels that must not be overwritten.
        \param [in] emptyIndex - an index written when propagation condition is false.
        \param [in] differenceThreshold - a threshold for conditional propagation by difference image.
    */
    SIMD_API void SimdSegmentationPropagate2x2(const uint8_t * parent, size_t parentStride, size_t width, size_t height,
        uint8_t * child, size_t childStride, const uint8_t * difference, size_t differenceStride,
        uint8_t currentIndex, uint8_t invalidIndex, uint8_t emptyIndex, uint8_t differenceThreshold);

    /*! @ingroup segmentation

        \fn void SimdSegmentationShrinkRegion(const uint8_t * mask, size_t stride, size_t width, size_t height, uint8_t index, ptrdiff_t * left, ptrdiff_t * top, ptrdiff_t * right, ptrdiff_t * bottom);

        \short Shrinks a rectangular region to the bounding box of a mask index.

        The mask must have 8-bit gray pixel format. The input rectangle is passed through \a left,
        \a top, \a right and \a bottom. The function searches only inside this rectangle and replaces
        it with the minimal half-open rectangle [left, right) x [top, bottom) that contains all pixels
        equal to \a index. If the index is not found, all four rectangle coordinates are set to 0.

        \note This function has a C++ wrappers: Simd::SegmentationShrinkRegion(const View<A> & mask, uint8_t index, Rectangle<ptrdiff_t> & rect).

        \param [in] mask - a pointer to pixels data of 8-bit gray mask image.
        \param [in] stride - a row size of the mask image in bytes.
        \param [in] width - a mask width.
        \param [in] height - a mask height.
        \param [in] index - a mask index to search for.
        \param [in, out] left - a pointer to the left side of the search/result rectangle.
        \param [in, out] top - a pointer to the top side of the search/result rectangle.
        \param [in, out] right - a pointer to the right side of the search/result rectangle.
        \param [in, out] bottom - a pointer to the bottom side of the search/result rectangle.
    */
    SIMD_API void SimdSegmentationShrinkRegion(const uint8_t * mask, size_t stride, size_t width, size_t height, uint8_t index,
        ptrdiff_t * left, ptrdiff_t * top, ptrdiff_t * right, ptrdiff_t * bottom);

    /*! @ingroup shifting

        \fn void SimdShiftBilinear(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount, const uint8_t * bkg, size_t bkgStride, const double * shiftX, const double * shiftY, size_t cropLeft, size_t cropTop, size_t cropRight, size_t cropBottom, uint8_t * dst, size_t dstStride);

        \short Shifts an image inside a crop rectangle with bilinear interpolation.

        All images must have the same width, height and number of 8-bit channels. The function first
        copies the area outside [cropLeft, cropRight) x [cropTop, cropBottom) from \a src to \a dst.
        Inside the crop rectangle it shifts \a src by (\a shiftX[0], \a shiftY[0]) and writes bilinear
        interpolated pixels to \a dst. Pixels uncovered by the shift are copied from \a bkg; border
        pixels where source and background overlap are mixed by the same bilinear weights.
        The shift values must be smaller than the crop rectangle width and height.

        \note This function has a C++ wrappers: Simd::ShiftBilinear(const View<A> & src, const View<A> & bkg, const Point<double> & shift, const Rectangle<ptrdiff_t> & crop, View<A> & dst).

        \param [in] src - a pointer to pixels data of the foreground input image.
        \param [in] srcStride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channelCount - a number of 8-bit channels per pixel.
        \param [in] bkg - a pointer to pixels data of the background input image.
        \param [in] bkgStride - a row size of the background image in bytes.
        \param [in] shiftX - a pointer to the image shift along the X axis.
        \param [in] shiftY - a pointer to the image shift along the Y axis.
        \param [in] cropLeft - a left side of the crop rectangle.
        \param [in] cropTop - a top side of the crop rectangle.
        \param [in] cropRight - a right side of the crop rectangle.
        \param [in] cropBottom - a bottom side of the crop rectangle.
        \param [out] dst - a pointer to pixels data of the output image.
        \param [in] dstStride - a row size of the output image in bytes.
    */
    SIMD_API void SimdShiftBilinear(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t channelCount,
        const uint8_t * bkg, size_t bkgStride, const double * shiftX, const double * shiftY,
        size_t cropLeft, size_t cropTop, size_t cropRight, size_t cropBottom, uint8_t * dst, size_t dstStride);

    /*! @ingroup shifting

        \fn void * SimdShiftDetectorInitBuffers(size_t bkgWidth, size_t bkgHeight, size_t levelCount, SimdShiftDetectorTextureType textureType, SimdShiftDetectorDifferenceType differenceType);

        \short Creates a shift detector context and its image pyramids.

        The detector is used to estimate translation of a current gray image relative to a background
        image. It stores background/current pyramids of \a levelCount levels with base size
        \a bkgWidth by \a bkgHeight. The \a textureType parameter selects whether matching is done on
        the original gray image or on the saturated sum of absolute X/Y gradients. The \a differenceType
        parameter selects the difference metric minimized during search.

        \note This function used in class Simd::ShiftDetector.

        \param [in] bkgWidth - a width of the background image.
        \param [in] bkgHeight - a height of the background image.
        \param [in] levelCount - the number of levels in the internal image pyramids used to find shift.
        \param [in] textureType - a type of texture used to detect shift (see ::SimdShiftDetectorTextureType).
        \param [in] differenceType - a difference metric used to detect shift (see ::SimdShiftDetectorDifferenceType).
        \return a pointer to shift detector context. On error it returns NULL.
                This pointer is used by functions ::SimdShiftDetectorSetBackground, ::SimdShiftDetectorEstimate and ::SimdShiftDetectorGetShift.
                It must be released with function ::SimdRelease.
    */
    SIMD_API void * SimdShiftDetectorInitBuffers(size_t bkgWidth, size_t bkgHeight, size_t levelCount, SimdShiftDetectorTextureType textureType, SimdShiftDetectorDifferenceType differenceType);

    /*! @ingroup shifting
     
        \fn void SimdShiftDetectorSetBackground(void *context, const uint8_t* bkg, size_t bkgStride, SimdBool makeCopy);
     
        \short Sets the background image for a shift detector context.

        The background image size is fixed by ::SimdShiftDetectorInitBuffers. If the detector texture
        type is ::SimdShiftDetectorTextureGray and \a makeCopy is SimdFalse, the context stores a view
        of the external background buffer and the caller must keep it valid. Otherwise the background
        data or its gradient texture is stored inside the context. After setting the base image the
        function builds the internal 2x downsampled pyramid.

        \note This function used in class Simd::ShiftDetector.

        \param [in] context - a shift detector context. It must be created by function ::SimdShiftDetectorInitBuffers and released by function ::SimdRelease.
        \param [in] bkg - a pointer to pixels data of the 8-bit gray background image.
        \param [in] bkgStride - a row size of the background image in bytes.
        \param [in] makeCopy - a flag to copy the gray background data into the context.
    */
    SIMD_API void SimdShiftDetectorSetBackground(void *context, const uint8_t* bkg, size_t bkgStride, SimdBool makeCopy);

    /*! @ingroup shifting
     
        \fn SimdBool SimdShiftDetectorEstimate(void *context, const uint8_t* curr, size_t currStride, size_t currWidth, size_t currHeight, size_t initShiftX, size_t initShiftY, size_t maxShiftX, size_t maxShiftY, const double* hiddenAreaPenalty, ptrdiff_t regionAreaMin);
        
        \short Estimates translation of a current image relative to the background.

        The background must be set by ::SimdShiftDetectorSetBackground before this call. The current
        image is interpreted as an 8-bit gray image. The rectangle
        [initShiftX, initShiftX + currWidth) x [initShiftY, initShiftY + currHeight) defines the
        current image position in background coordinates before applying the estimated shift. The search
        is performed from coarse to fine pyramid levels and is limited by \a maxShiftX and \a maxShiftY.
        \a hiddenAreaPenalty[0] penalizes candidate shifts that hide part of the initial region outside
        the background; \a regionAreaMin selects the finest pyramid levels whose search region area is
        large enough. On success, call ::SimdShiftDetectorGetShift to read the result.

        \note This function used in class Simd::ShiftDetector.

        \param [in] context - a shift detector context. It must be created by function ::SimdShiftDetectorInitBuffers and released by function ::SimdRelease.
        \param [in] curr - a pointer to pixels data of the 8-bit gray current image.
        \param [in] currStride - a row size of the current image in bytes.
        \param [in] currWidth - a width of the current image.
        \param [in] currHeight - a height of the current image.
        \param [in] initShiftX - an initial X position of current image in background coordinates.
        \param [in] initShiftY - an initial Y position of current image in background coordinates.
        \param [in] maxShiftX - maximal absolute shift along X axis.
        \param [in] maxShiftY - maximal absolute shift along Y axis.
        \param [in] hiddenAreaPenalty - a pointer to a penalty factor for shifts near/outside the background border.
        \param [in] regionAreaMin - a minimal search-region area used to choose active pyramid levels.
        \return a result of shift estimation (SimdTrue or SimdFalse). On success use ::SimdShiftDetectorGetShift to get shift and other parameters.
    */
    SIMD_API SimdBool SimdShiftDetectorEstimate(void* context, const uint8_t* curr, size_t currStride, size_t currWidth, size_t currHeight,
        size_t initShiftX, size_t initShiftY, size_t maxShiftX, size_t maxShiftY, const double* hiddenAreaPenalty, ptrdiff_t regionAreaMin);

    /*! @ingroup shifting

        \fn void SimdShiftDetectorGetShift(const void* context, ptrdiff_t* shift, double * refinedShift, double * stability, double * correlation);

        \short Gets shift and quality values estimated by ::SimdShiftDetectorEstimate.

        Any output pointer can be NULL. The integer shift is the best discrete translation at the base
        pyramid level. The refined shift adds sub-pixel refinement estimated from the 3x3 neighborhood
        of difference values. Stability characterizes how well the minimum is separated from its
        neighborhood. Correlation is derived from the best average difference and is close to 1 for
        similar images.

        \note This function used in class Simd::ShiftDetector.

        \param [in] context - a shift detector context. It must be created by function ::SimdShiftDetectorInitBuffers and released by function ::SimdRelease.
        \param [out] shift - a pointer to array[2] that receives integer X and Y shift. Can be NULL.
        \param [out] refinedShift - a pointer to array[2] that receives sub-pixel X and Y shift. Can be NULL.
        \param [out] stability - a pointer to a value that receives stability of the found shift. Can be NULL.
        \param [out] correlation - a pointer to a value that receives correlation of background and current image. Can be NULL.
    */
    SIMD_API void SimdShiftDetectorGetShift(const void* context, ptrdiff_t* shift, double * refinedShift, double * stability, double * correlation);

    /*! @ingroup sobel_filter

        \fn void SimdSobelDx(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

        \short Calculates the horizontal Sobel derivative of an 8-bit gray image.

        Input image must have 8-bit gray format, output image must have signed 16-bit integer format.
        All images must have the same width and height, and width must be greater than 1. At image
        borders the nearest valid source row or column is reused.

        For every point:
        \verbatim
        x0 = Max(x - 1, 0); x2 = Min(x + 1, width - 1);
        y0 = Max(y - 1, 0); y2 = Min(y + 1, height - 1);
        dst[x, y] = (src[x2, y0] + 2*src[x2, y] + src[x2, y2]) -
                    (src[x0, y0] + 2*src[x0, y] + src[x0, y2]);
        \endverbatim

        \note This function has a C++ wrappers: Simd::SobelDx(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of the input image.
        \param [in] srcStride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of the signed 16-bit output image.
        \param [in] dstStride - a row size of the output image in bytes. It must be a multiple of sizeof(int16_t).
    */
    SIMD_API void SimdSobelDx(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

    /*! @ingroup sobel_filter

        \fn void SimdSobelDxAbs(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

        \short Calculates the absolute horizontal Sobel derivative of an 8-bit gray image.

        Input image must have 8-bit gray format, output image must have signed 16-bit integer format.
        All images must have the same width and height, and width must be greater than 1. At image
        borders the nearest valid source row or column is reused.

        For every point:
        \verbatim
        x0 = Max(x - 1, 0); x2 = Min(x + 1, width - 1);
        y0 = Max(y - 1, 0); y2 = Min(y + 1, height - 1);
        dst[x, y] = Abs((src[x2, y0] + 2*src[x2, y] + src[x2, y2]) -
                        (src[x0, y0] + 2*src[x0, y] + src[x0, y2]));
        \endverbatim

        \note This function has a C++ wrappers: Simd::SobelDxAbs(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of the input image.
        \param [in] srcStride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of the signed 16-bit output image.
        \param [in] dstStride - a row size of the output image in bytes. It must be a multiple of sizeof(int16_t).
    */
    SIMD_API void SimdSobelDxAbs(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

    /*! @ingroup sobel_statistic

        \fn void SimdSobelDxAbsSum(const uint8_t * src, size_t stride, size_t width, size_t height, uint64_t * sum);

        \short Calculates the sum of absolute horizontal Sobel derivatives.

        Input image must have 8-bit gray format, and width must be greater than 1. At image borders
        the nearest valid source row or column is reused. The output sum is initialized to zero inside
        the function before accumulation.

        For every point:
        \verbatim
        x0 = Max(x - 1, 0); x2 = Min(x + 1, width - 1);
        y0 = Max(y - 1, 0); y2 = Min(y + 1, height - 1);
        sum += Abs((src[x2, y0] + 2*src[x2, y] + src[x2, y2]) -
                   (src[x0, y0] + 2*src[x0, y] + src[x0, y2]));
        \endverbatim

        \note This function has a C++ wrappers: Simd::SobelDxAbsSum(const View<A>& src, uint64_t & sum).

        \param [in] src - a pointer to pixels data of the input image.
        \param [in] stride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sum - a pointer to unsigned 64-bit integer value with result sum.
    */
    SIMD_API void SimdSobelDxAbsSum(const uint8_t * src, size_t stride, size_t width, size_t height, uint64_t * sum);

    /*! @ingroup sobel_filter

        \fn void SimdSobelDy(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

        \short Calculates the vertical Sobel derivative of an 8-bit gray image.

        Input image must have 8-bit gray format, output image must have signed 16-bit integer format.
        All images must have the same width and height, and width must be greater than 1. At image
        borders the nearest valid source row or column is reused.

        For every point:
        \verbatim
        x0 = Max(x - 1, 0); x2 = Min(x + 1, width - 1);
        y0 = Max(y - 1, 0); y2 = Min(y + 1, height - 1);
        dst[x, y] = (src[x0, y2] + 2*src[x, y2] + src[x2, y2]) -
                    (src[x0, y0] + 2*src[x, y0] + src[x2, y0]);
        \endverbatim

        \note This function has a C++ wrappers: Simd::SobelDy(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of the input image.
        \param [in] srcStride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of the signed 16-bit output image.
        \param [in] dstStride - a row size of the output image in bytes. It must be a multiple of sizeof(int16_t).
    */
    SIMD_API void SimdSobelDy(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

    /*! @ingroup sobel_filter

        \fn void SimdSobelDyAbs(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

        \short Calculates the absolute vertical Sobel derivative of an 8-bit gray image.

        Input image must have 8-bit gray format, output image must have signed 16-bit integer format.
        All images must have the same width and height, and width must be greater than 1. At image
        borders the nearest valid source row or column is reused.

        For every point:
        \verbatim
        x0 = Max(x - 1, 0); x2 = Min(x + 1, width - 1);
        y0 = Max(y - 1, 0); y2 = Min(y + 1, height - 1);
        dst[x, y] = Abs((src[x0, y2] + 2*src[x, y2] + src[x2, y2]) -
                        (src[x0, y0] + 2*src[x, y0] + src[x2, y0]));
        \endverbatim

        \note This function has a C++ wrappers: Simd::SobelDyAbs(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of the input image.
        \param [in] srcStride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of the signed 16-bit output image.
        \param [in] dstStride - a row size of the output image in bytes. It must be a multiple of sizeof(int16_t).
    */
    SIMD_API void SimdSobelDyAbs(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

    /*! @ingroup sobel_statistic

        \fn void SimdSobelDyAbsSum(const uint8_t * src, size_t stride, size_t width, size_t height, uint64_t * sum);

        \short Calculates the sum of absolute vertical Sobel derivatives.

        Input image must have 8-bit gray format, and width must be greater than 1. At image borders
        the nearest valid source row or column is reused. The output sum is initialized to zero inside
        the function before accumulation.

        For every point:
        \verbatim
        x0 = Max(x - 1, 0); x2 = Min(x + 1, width - 1);
        y0 = Max(y - 1, 0); y2 = Min(y + 1, height - 1);
        sum += Abs((src[x0, y2] + 2*src[x, y2] + src[x2, y2]) -
                   (src[x0, y0] + 2*src[x, y0] + src[x2, y0]));
        \endverbatim

        \note This function has a C++ wrappers: Simd::SobelDyAbsSum(const View<A>& src, uint64_t & sum).

        \param [in] src - a pointer to pixels data of the input image.
        \param [in] stride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sum - a pointer to unsigned 64-bit integer value with result sum.
    */
    SIMD_API void SimdSobelDyAbsSum(const uint8_t * src, size_t stride, size_t width, size_t height, uint64_t * sum);

    /*! @ingroup contour

        \fn void SimdContourMetrics(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride)

        \short Calculates contour metrics from horizontal and vertical Sobel derivatives.

        Input image must have 8-bit gray format, output image must have 16-bit integer format.
        All images must have the same width and height, and width must be greater than 1. At image
        borders the nearest valid source row or column is reused. The output value packs contour
        magnitude and dominant direction: the high bits contain dx + dy, and the low bit is 0 when
        dx >= dy and 1 otherwise.
        This function is used for contour extraction.

        For every point:
        \verbatim
        x0 = Max(x - 1, 0); x2 = Min(x + 1, width - 1);
        y0 = Max(y - 1, 0); y2 = Min(y + 1, height - 1);
        dx = Abs((src[x2, y0] + 2*src[x2, y] + src[x2, y2]) -
                 (src[x0, y0] + 2*src[x0, y] + src[x0, y2]));
        dy = Abs((src[x0, y2] + 2*src[x, y2] + src[x2, y2]) -
                 (src[x0, y0] + 2*src[x, y0] + src[x2, y0]));
        dst[x, y] = (dx + dy)*2 + (dx >= dy ? 0 : 1);
        \endverbatim

        \note This function has a C++ wrappers: Simd::ContourMetrics(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of the gray 8-bit input image.
        \param [in] srcStride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] dst - a pointer to pixels data of the output 16-bit image.
        \param [in] dstStride - a row size of the output image in bytes. It must be a multiple of sizeof(uint16_t).
    */
    SIMD_API void SimdContourMetrics(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t * dst, size_t dstStride);

    /*! @ingroup contour

        \fn void SimdContourMetricsMasked(const uint8_t * src, size_t srcStride, size_t width, size_t height, const uint8_t * mask, size_t maskStride, uint8_t indexMin, uint8_t * dst, size_t dstStride)

        \short Calculates masked contour metrics from horizontal and vertical Sobel derivatives.

        Input and mask images must have 8-bit gray format, output image must have 16-bit integer
        format. All images must have the same width and height, and width must be greater than 1.
        At image borders the nearest valid source row or column is reused. Pixels with mask value
        lower than \a indexMin get zero output; other pixels use the same packed metric as
        ::SimdContourMetrics.
        This function is used for contour extraction.

        For every point:
        \verbatim
        x0 = Max(x - 1, 0); x2 = Min(x + 1, width - 1);
        y0 = Max(y - 1, 0); y2 = Min(y + 1, height - 1);
        dx = Abs((src[x2, y0] + 2*src[x2, y] + src[x2, y2]) -
                 (src[x0, y0] + 2*src[x0, y] + src[x0, y2]));
        dy = Abs((src[x0, y2] + 2*src[x, y2] + src[x2, y2]) -
                 (src[x0, y0] + 2*src[x, y0] + src[x2, y0]));
        dst[x, y] = mask[x, y] < indexMin ? 0 : (dx + dy)*2 + (dx >= dy ? 0 : 1);
        \endverbatim

        \note This function has a C++ wrappers: Simd::ContourMetrics(const View<A>& src, const View<A>& mask, uint8_t indexMin, View<A>& dst).

        \param [in] src - a pointer to pixels data of the gray 8-bit input image.
        \param [in] srcStride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] mask - a pointer to pixels data of the mask 8-bit image.
        \param [in] maskStride - a row size of the mask image in bytes.
        \param [in] indexMin - a minimal mask index that enables metric calculation.
        \param [out] dst - a pointer to pixels data of the output 16-bit image.
        \param [in] dstStride - a row size of the output image in bytes. It must be a multiple of sizeof(uint16_t).
    */
    SIMD_API void SimdContourMetricsMasked(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        const uint8_t * mask, size_t maskStride, uint8_t indexMin, uint8_t * dst, size_t dstStride);

    /*! @ingroup contour

        \fn void SimdContourAnchors(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t step, int16_t threshold, uint8_t * dst, size_t dstStride);

        \short Extracts contour anchor points from packed contour metrics.

        Input image must have 16-bit integer format produced by ::SimdContourMetrics or
        ::SimdContourMetricsMasked, output image must have 8-bit gray format. All images must have
        the same width and height. The first and last rows are cleared to zero. For processed inner
        rows, the first and last columns are also set to zero. Rows are processed with increment
        \a step beginning at row 1.
        Input image with metrics can be estimated by using ::SimdContourMetrics or ::SimdContourMetricsMasked functions.
        This function is used for contour extraction.

        For every processed inner point:
        \verbatim
        a[x, y] = src[x, y] >> 1;
        if(src[x, y] & 1)
            dst[x, y] = a[x, y] > 0 && (a[x, y] - a[x + 1, y] >= threshold) && (a[x, y] - a[x - 1, y] >= threshold) ? 255 : 0;
        else
            dst[x, y] = a[x, y] > 0 && (a[x, y] - a[x, y + 1] >= threshold) && (a[x, y] - a[x, y - 1] >= threshold) ? 255 : 0;
        \endverbatim

        \note This function has a C++ wrappers: Simd::ContourAnchors(const View<A>& src, size_t step, int16_t threshold, View<A>& dst).

        \param [in] src - a pointer to pixels data of the 16-bit input image.
        \param [in] srcStride - a row size of the input image in bytes. It must be a multiple of sizeof(uint16_t).
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] step - a row step for anchor extraction.
        \param [in] threshold - a minimal metric difference required to create an anchor.
        \param [out] dst - a pointer to pixels data of the output 8-bit gray image.
        \param [in] dstStride - a row size of the output image in bytes.
    */
    SIMD_API void SimdContourAnchors(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t step, int16_t threshold, uint8_t * dst, size_t dstStride);

    /*! @ingroup correlation

        \fn void SimdSquaredDifferenceSum(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride, size_t width, size_t height, uint64_t * sum);

        \short Calculates the sum of squared differences for two 8-bit gray images.

        All images must have the same width and height. The output sum is initialized to zero inside
        the function before accumulation.

        For every point:
        \verbatim
        sum[0] += (a[i] - b[i])*(a[i] - b[i]);
        \endverbatim

        \note This function has a C++ wrappers: Simd::SquaredDifferenceSum(const View<A>& a, const View<A>& b, uint64_t & sum).

        \param [in] a - a pointer to pixels data of the first image.
        \param [in] aStride - a row size of the first image in bytes.
        \param [in] b - a pointer to pixels data of the second image.
        \param [in] bStride - a row size of the second image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sum - a pointer to unsigned 64-bit integer value with result sum.
    */
    SIMD_API void SimdSquaredDifferenceSum(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride,
        size_t width, size_t height, uint64_t * sum);

    /*! @ingroup correlation

        \fn void SimdSquaredDifferenceSumMasked(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride, const uint8_t * mask, size_t maskStride, uint8_t index, size_t width, size_t height, uint64_t * sum);

        \short Calculates the masked sum of squared differences for two 8-bit gray images.

        All images must have the same width and height. The input and mask images must have 8-bit
        gray format. Only pixels where mask equals \a index contribute to the result. The output sum
        is initialized to zero inside the function before accumulation.

        For every point:
        \verbatim
        if(mask[i] == index)
            sum[0] += (a[i] - b[i])*(a[i] - b[i]);
        \endverbatim

        \note This function has a C++ wrappers: Simd::SquaredDifferenceSum(const View<A>& a, const View<A>& b, const View<A>& mask, uint8_t index, uint64_t & sum).

        \param [in] a - a pointer to pixels data of the first image.
        \param [in] aStride - a row size of the first image in bytes.
        \param [in] b - a pointer to pixels data of the second image.
        \param [in] bStride - a row size of the second image in bytes.
        \param [in] mask - a pointer to pixels data of the mask image.
        \param [in] maskStride - a row size of the mask image in bytes.
        \param [in] index - a mask index that enables accumulation.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sum - a pointer to unsigned 64-bit integer value with result sum.
    */
    SIMD_API void SimdSquaredDifferenceSumMasked(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride,
        const uint8_t * mask, size_t maskStride, uint8_t index, size_t width, size_t height, uint64_t * sum);

    /*! @ingroup correlation

        \fn void SimdSquaredDifferenceSum32f(const float * a, const float * b, size_t size, float * sum);

        \short Calculates the sum of squared differences for two 32-bit float arrays.

        All arrays must have the same size. The output sum is overwritten by the calculated value.

        For every element:
        \verbatim
        sum[0] = Sum((a[i] - b[i])*(a[i] - b[i]));
        \endverbatim

        \param [in] a - a pointer to the first array.
        \param [in] b - a pointer to the second array.
        \param [in] size - a size of arrays.
        \param [out] sum - a pointer to the 32-bit float sum of squared differences.
    */
    SIMD_API void SimdSquaredDifferenceSum32f(const float * a, const float * b, size_t size, float * sum);

    /*! @ingroup correlation

        \fn void SimdSquaredDifferenceKahanSum32f(const float * a, const float * b, size_t size, float * sum);

        \short Calculates the sum of squared differences for two 32-bit float arrays using Kahan summation.

        All arrays must have the same size. This variant applies compensated summation to reduce
        floating-point accumulation error. The output sum is overwritten by the calculated value.

        Algorithm pseudo code:
        \verbatim
        sum[0] = 0; corr = 0;
        for(i = 0; i < size; ++i)
        {
            diff = (a[i] - b[i])*(a[i] - b[i]) - corr;
            temp = sum[0] + diff;
            corr = (temp - sum[0]) - diff;
            sum[0] = temp;
        }
        \endverbatim

        \param [in] a - a pointer to the first array.
        \param [in] b - a pointer to the second array.
        \param [in] size - a size of arrays.
        \param [out] sum - a pointer to the 32-bit float sum of squared differences.
    */
    SIMD_API void SimdSquaredDifferenceKahanSum32f(const float * a, const float * b, size_t size, float * sum);

    /*! @ingroup other_statistic

        \fn void SimdGetStatistic(const uint8_t * src, size_t stride, size_t width, size_t height, uint8_t * min, uint8_t * max, uint8_t * average);

        \short Finds minimal, maximal and rounded average pixel values for an 8-bit gray image.

        The image must have 8-bit gray format and non-zero area. The average is rounded to the
        nearest integer as (sum + width*height/2)/(width*height).

        \note This function has a C++ wrappers: Simd::GetStatistic(const View<A>& src, uint8_t & min, uint8_t & max, uint8_t & average).

        \param [in] src - a pointer to pixels data of the input image.
        \param [in] stride - a row size of the image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] min - a pointer to unsigned 8-bit integer value with found minimal pixel value.
        \param [out] max - a pointer to unsigned 8-bit integer value with found maximal pixel value.
        \param [out] average - a pointer to unsigned 8-bit integer value with found rounded average pixel value.
    */
    SIMD_API void SimdGetStatistic(const uint8_t * src, size_t stride, size_t width, size_t height,
        uint8_t * min, uint8_t * max, uint8_t * average);

    /*! @ingroup other_statistic

        \fn void SimdGetMoments(const uint8_t * mask, size_t stride, size_t width, size_t height, uint8_t index, uint64_t * area, uint64_t * x, uint64_t * y, uint64_t * xx, uint64_t * xy, uint64_t * yy);

        \short Calculates geometric moments of mask pixels with a given index.

        The mask image must have 8-bit gray format. All output values are initialized to zero inside
        the function before accumulation.

        For every point:
        \verbatim
        if(mask[X, Y] == index)
        {
            area[0] += 1;
            x[0] += X;
            y[0] += Y;
            xx[0] += X*X;
            xy[0] += X*Y;
            yy[0] += Y*Y;
        }
        \endverbatim

        \note This function has a C++ wrappers: Simd::GetMoments(const View<A>& mask, uint8_t index, uint64_t & area, uint64_t & x, uint64_t & y, uint64_t & xx, uint64_t & xy, uint64_t & yy).

        \param [in] mask - a pointer to pixels data of the mask image.
        \param [in] stride - a row size of the mask image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] index - a mask index to include in moment calculation.
        \param [out] area - a pointer to unsigned 64-bit integer value with found area (number of pixels with given index).
        \param [out] x - a pointer to unsigned 64-bit integer value with found first-order moment x.
        \param [out] y - a pointer to unsigned 64-bit integer value with found first-order moment y.
        \param [out] xx - a pointer to unsigned 64-bit integer value with found second-order moment xx.
        \param [out] xy - a pointer to unsigned 64-bit integer value with found second-order moment xy.
        \param [out] yy - a pointer to unsigned 64-bit integer value with found second-order moment yy.
    */
    SIMD_API void SimdGetMoments(const uint8_t * mask, size_t stride, size_t width, size_t height, uint8_t index,
        uint64_t * area, uint64_t * x, uint64_t * y, uint64_t * xx, uint64_t * xy, uint64_t * yy);

    /*! @ingroup other_statistic

        \fn void SimdGetObjectMoments(const uint8_t * src, size_t srcStride, size_t width, size_t height, const uint8_t * mask, size_t maskStride, uint8_t index, uint64_t * n, uint64_t * s,  uint64_t * sx, uint64_t * sy, uint64_t * sxx, uint64_t * sxy, uint64_t * syy);

        \short Calculates weighted geometric moments of an object.

        The images must have 8-bit gray format and equal size. Either \a src or \a mask can be NULL,
        but not both. If \a mask is NULL, every source pixel is included. If \a src is NULL, every
        selected mask pixel has weight 1. All output values are initialized to zero inside the function
        before accumulation.

        For every point:
        \verbatim
        if(mask == NULL || mask[X, Y] == index)
        {
            S = src ? src[X, Y] : 1;
            n[0] += 1;
            s[0] += S;
            sx[0] += S*X;
            sy[0] += S*Y;
            sxx[0] += S*X*X;
            sxy[0] += S*X*Y;
            syy[0] += S*Y*Y;
        }
        \endverbatim

        \note This function has a C++ wrappers: Simd::GetObjectMoments(const View<A> & src, const View<A> & mask, uint8_t index, uint64_t & n, uint64_t & s,  uint64_t & sx, uint64_t & sy, uint64_t & sxx, uint64_t & sxy, uint64_t & syy).

        \param [in] src - a pointer to pixels data of the input image. Can be NULL to use weight 1 for selected pixels.
        \param [in] srcStride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] mask - a pointer to pixels data of the mask image. Can be NULL to include every pixel.
        \param [in] maskStride - a row size of the mask image in bytes.
        \param [in] index - a mask index to include when \a mask is not NULL.
        \param [out] n - a pointer to unsigned 64-bit integer value with found number of selected pixels.
        \param [out] s - a pointer to unsigned 64-bit integer value with sum of image values of given object.
        \param [out] sx - a pointer to unsigned 64-bit integer value with found first-order moment x of given object.
        \param [out] sy - a pointer to unsigned 64-bit integer value with found first-order moment y of given object.
        \param [out] sxx - a pointer to unsigned 64-bit integer value with found second-order moment xx of given object.
        \param [out] sxy - a pointer to unsigned 64-bit integer value with found second-order moment xy of given object.
        \param [out] syy - a pointer to unsigned 64-bit integer value with found second-order moment yy of given object.
    */
    SIMD_API void SimdGetObjectMoments(const uint8_t * src, size_t srcStride, size_t width, size_t height, const uint8_t * mask, size_t maskStride, uint8_t index,
        uint64_t * n, uint64_t * s,  uint64_t * sx, uint64_t * sy, uint64_t * sxx, uint64_t * sxy, uint64_t * syy);

    /*! @ingroup row_statistic

        \fn void SimdGetRowSums(const uint8_t * src, size_t stride, size_t width, size_t height, uint32_t * sums);

        \short Calculates row sums for an 8-bit gray image.

        The output array is overwritten by the calculated row sums.

        For all rows:
        \verbatim
        sums[y] = Sum(src[x, y]), 0 <= x < width;
        \endverbatim

        \note This function has a C++ wrappers: Simd::GetRowSums(const View<A>& src, uint32_t * sums).

        \param [in] src - a pointer to pixels data of the input image.
        \param [in] stride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sums - a pointer to array of unsigned 32-bit integer row sums. Its length must be at least height.
    */
    SIMD_API void SimdGetRowSums(const uint8_t * src, size_t stride, size_t width, size_t height, uint32_t * sums);

    /*! @ingroup col_statistic

        \fn void SimdGetColSums(const uint8_t * src, size_t stride, size_t width, size_t height, uint32_t * sums);

        \short Calculates column sums for an 8-bit gray image.

        The output array is cleared to zero inside the function before accumulation.

        For all columns:
        \verbatim
        sums[x] = Sum(src[x, y]), 0 <= y < height;
        \endverbatim

        \note This function has a C++ wrappers: Simd::GetColSums(const View<A>& src, uint32_t * sums).

        \param [in] src - a pointer to pixels data of the input image.
        \param [in] stride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sums - a pointer to array of unsigned 32-bit integer column sums. Its length must be at least width.
    */
    SIMD_API void SimdGetColSums(const uint8_t * src, size_t stride, size_t width, size_t height, uint32_t * sums);

    /*! @ingroup row_statistic

        \fn void SimdGetAbsDyRowSums(const uint8_t * src, size_t stride, size_t width, size_t height, uint32_t * sums);

        \short Calculates row sums of absolute vertical differences for an 8-bit gray image.

        For all rows except the last:
        \verbatim
        sums[y] = Sum(Abs(src[x, y + 1] - src[x, y])), 0 <= x < width;
        \endverbatim
        For the last row:
        \verbatim
        sums[height-1] = 0;
        \endverbatim

        \note This function has a C++ wrappers: Simd::GetAbsDyRowSums(const View<A>& src, uint32_t * sums).

        \param [in] src - a pointer to pixels data of the input image.
        \param [in] stride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sums - a pointer to array of unsigned 32-bit integer row sums. Its length must be at least height.
    */
    SIMD_API void SimdGetAbsDyRowSums(const uint8_t * src, size_t stride, size_t width, size_t height, uint32_t * sums);

    /*! @ingroup col_statistic

        \fn void SimdGetAbsDxColSums(const uint8_t * src, size_t stride, size_t width, size_t height, uint32_t * sums);

        \short Calculates column sums of absolute horizontal differences for an 8-bit gray image.

        The output array is cleared to zero inside the function before accumulation.

        For all columns except the last:
        \verbatim
        sums[x] = Sum(Abs(src[x + 1, y] - src[x, y])), 0 <= y < height;
        \endverbatim
        For the last column:
        \verbatim
        sums[width-1] = 0;
        \endverbatim

        \note This function has a C++ wrappers: Simd::GetAbsDxColSums(const View<A>& src, uint32_t * sums).

        \param [in] src - a pointer to pixels data of the input image.
        \param [in] stride - a row size of the input image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sums - a pointer to array of unsigned 32-bit integer column sums. Its length must be at least width.
    */
    SIMD_API void SimdGetAbsDxColSums(const uint8_t * src, size_t stride, size_t width, size_t height, uint32_t * sums);

    /*! @ingroup other_statistic

        \fn void SimdValueSum(const uint8_t * src, size_t stride, size_t width, size_t height, uint64_t * sum);

        \short Calculates the sum of pixel values for an 8-bit gray image.

        The output sum is initialized to zero inside the function before accumulation.

        For every point:
        \verbatim
        sum[0] += src[x, y];
        \endverbatim

        \note This function has a C++ wrappers: Simd::ValueSum(const View<A>& src, uint64_t & sum).

        \param [in] src - a pointer to pixels data of the image.
        \param [in] stride - a row size of the image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sum - a pointer to unsigned 64-bit integer result sum.
    */
    SIMD_API void SimdValueSum(const uint8_t * src, size_t stride, size_t width, size_t height, uint64_t * sum);

    /*! @ingroup other_statistic

        \fn void SimdSquareSum(const uint8_t * src, size_t stride, size_t width, size_t height, uint64_t * sum);

        \short Calculates the sum of squared pixel values for an 8-bit gray image.

        The output sum is initialized to zero inside the function before accumulation.

        For every point:
        \verbatim
        sum[0] += src[x, y]*src[x, y];
        \endverbatim

        \note This function has a C++ wrappers: Simd::SquareSum(const View<A>& src, uint64_t & sum).

        \param [in] src - a pointer to pixels data of the image.
        \param [in] stride - a row size of the image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sum - a pointer to unsigned 64-bit integer result sum.
    */
    SIMD_API void SimdSquareSum(const uint8_t * src, size_t stride, size_t width, size_t height, uint64_t * sum);

    /*! @ingroup other_statistic

        \fn void SimdValueSquareSum(const uint8_t * src, size_t stride, size_t width, size_t height, uint64_t * valueSum, uint64_t * squareSum);

        \short Calculates value sum and squared value sum for an 8-bit gray image.

        Output sums are initialized to zero inside the function before accumulation.

        For every point:
        \verbatim
        valueSum[0] += src[x, y];
        squareSum[0] += src[x, y]*src[x, y];
        \endverbatim

        \note This function has a C++ wrappers: Simd::ValueSquareSum(const View<A>& src, uint64_t & valueSum, uint64_t & squareSum).

        \param [in] src - a pointer to pixels data of the image.
        \param [in] stride - a row size of the image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] valueSum - a pointer to unsigned 64-bit integer value sum.
        \param [out] squareSum - a pointer to unsigned 64-bit integer squared value sum.
    */
    SIMD_API void SimdValueSquareSum(const uint8_t * src, size_t stride, size_t width, size_t height, uint64_t * valueSum, uint64_t * squareSum);

    /*! @ingroup other_statistic

        \fn void SimdValueSquareSums(const uint8_t* src, size_t stride, size_t width, size_t height, size_t channels, uint64_t* valueSums, uint64_t* squareSums);

        \short Calculates per-channel value sums and squared value sums for an 8-bit image.

        The image must have 8-bit depth per channel, and \a channels must be 1, 2, 3 or 4. Output
        arrays are initialized to zero inside the function before accumulation.

        \verbatim
        for(c = 0; c < channels; c++)
        {
            valueSums[c] = 0;
            squareSums[c] = 0;
        }
        for(y = 0; y < height; y++) 
            for(x = 0; x < width; x++)
                for(c = 0; c < channels; c++)
                {
                    value = src[y * stride + x * channels + c];
                    valueSums[c] += value;
                    squareSums[c] += value * value;
                }
        \endverbatim

        \note This function has a C++ wrappers: Simd::ValueSquareSums(const View<A>& src, uint64_t * valueSums, uint64_t * squareSums).

        \param [in] src - a pointer to pixels data of the image.
        \param [in] stride - a row size of the image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] channels - a number of image channels. It must be 1, 2, 3 or 4.
        \param [out] valueSums - a pointer to output buffer with per-channel value sums. Its size must be at least \a channels.
        \param [out] squareSums - a pointer to output buffer with per-channel squared value sums. Its size must be at least \a channels.
    */
    SIMD_API void SimdValueSquareSums(const uint8_t* src, size_t stride, size_t width, size_t height, size_t channels, uint64_t* valueSums, uint64_t* squareSums);
    
    /*! @ingroup other_statistic

        \fn void SimdCorrelationSum(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride, size_t width, size_t height, uint64_t * sum);

        \short Calculates the sum of pixel-wise products for two 8-bit gray images.

        All images must have the same width and height and 8-bit gray pixel format. The output sum is
        initialized to zero inside the function before accumulation.

        For all points:
        \verbatim
        sum[0] += a[i]*b[i];
        \endverbatim

        \note This function has a C++ wrappers: Simd::CorrelationSum(const View<A> & a, const View<A> & b, uint64_t & sum).

        \param [in] a - a pointer to pixels data of the first image.
        \param [in] aStride - a row size of the first image in bytes.
        \param [in] b - a pointer to pixels data of the second image.
        \param [in] bStride - a row size of the second image in bytes.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] sum - a pointer to unsigned 64-bit integer result sum.
    */
    SIMD_API void SimdCorrelationSum(const uint8_t * a, size_t aStride, const uint8_t * b, size_t bStride, size_t width, size_t height, uint64_t * sum);

    /*! @ingroup resizing

        \fn void SimdStretchGray2x2(const uint8_t * src, size_t srcWidth, size_t srcHeight, size_t srcStride, uint8_t * dst, size_t dstWidth, size_t dstHeight, size_t dstStride);

        \short Stretches an 8-bit gray image by two in both dimensions using pixel replication.

        The output size must be exactly: dstWidth = 2*srcWidth, dstHeight = 2*srcHeight.
        For every source pixel:
        \verbatim
        dst[2*x + 0, 2*y + 0] = src[x, y];
        dst[2*x + 1, 2*y + 0] = src[x, y];
        dst[2*x + 0, 2*y + 1] = src[x, y];
        dst[2*x + 1, 2*y + 1] = src[x, y];
        \endverbatim

        \note This function has a C++ wrappers: Simd::StretchGray2x2(const View<A>& src, View<A>& dst).

        \param [in] src - a pointer to pixels data of the original input image.
        \param [in] srcWidth - a width of the input image.
        \param [in] srcHeight - a height of the input image.
        \param [in] srcStride - a row size of the input image in bytes.
        \param [out] dst - a pointer to pixels data of the stretched output image.
        \param [in] dstWidth - a width of the output image.
        \param [in] dstHeight - a height of the output image.
        \param [in] dstStride - a row size of the output image in bytes.
    */
    SIMD_API void SimdStretchGray2x2(const uint8_t * src, size_t srcWidth, size_t srcHeight, size_t srcStride,
        uint8_t * dst, size_t dstWidth, size_t dstHeight, size_t dstStride);

    /*! @ingroup synet_add

        \fn void* SimdSynetAdd16bInit(const size_t* aShape, size_t aCount, SimdTensorDataType aType, const size_t* bShape, size_t bCount, SimdTensorDataType bType, SimdTensorDataType dstType, SimdTensorFormatType format);

        \short Initializes element-wise addition of two tensors in FP32 or BF16 format.

        The created context adds two tensors with equal shapes:
        \verbatim
        for(i = 0; i < shapeSize; ++i)
        {
            A = aType == SimdTensorData16b ? BFloat16ToFloat32(a[i]) : a[i];
            B = bType == SimdTensorData16b ? BFloat16ToFloat32(b[i]) : b[i];
            D = A + B;
            dst[i] = dstType == SimdTensorData16b ? Float32ToBFloat16(D) : D;
        }
        \endverbatim

        The current implementation creates a context only for equal input shapes, FP32/BF16 input and output tensor types,
        and SimdTensorFormatUnknown, SimdTensorFormatNchw or SimdTensorFormatNhwc tensor format.

        \param [in] aShape - a pointer to shape of input A tensor.
        \param [in] aCount - a count of dimensions of input A tensor.
        \param [in] aType - a type of input A tensor. Can be FP32 or BF16.
        \param [in] bShape - a pointer to shape of input B tensor.
        \param [in] bCount - a count of dimensions of input B tensor.
        \param [in] bType - a type of input B tensor. Can be FP32 or BF16.
        \param [in] dstType - a type of output tensor. Can be FP32 or BF16.
        \param [in] format - a format of input / output tensors.
        \return a pointer to add context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in function ::SimdSynetAdd16bForward.
    */
    SIMD_API void* SimdSynetAdd16bInit(const size_t* aShape, size_t aCount, SimdTensorDataType aType, const size_t* bShape, size_t bCount, SimdTensorDataType bType, SimdTensorDataType dstType, SimdTensorFormatType format);

    /*! @ingroup synet_add

        \fn void SimdSynetAdd16bForward(void* context, const uint8_t* a, const uint8_t* b, uint8_t* dst);

        \short Performs element-wise addition of two FP32/BF16 tensors.

        The function adds corresponding elements of input tensors A and B using a context created by ::SimdSynetAdd16bInit.
        The actual data types, tensor shape and output type are stored in the context. BF16 input values are converted to
        FP32 before addition, and BF16 output values are converted from FP32 after addition.

        \param [in] context - a pointer to add context. It must be created by function ::SimdSynetAdd16bInit and released by function ::SimdRelease.
        \param [in] a - a pointer to input A tensor.
        \param [in] b - a pointer to input B tensor.
        \param [out] dst - a pointer to output tensor.
    */
    SIMD_API void SimdSynetAdd16bForward(void* context, const uint8_t* a, const uint8_t* b, uint8_t* dst);

    /*! @ingroup synet_add

        \fn void SimdSynetAddBias(const float * bias, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format);

        \short Adds per-channel bias to an FP32 tensor in place.

        Algorithm's details (example for NCHW tensor format):
        \verbatim
        for(c = 0; c < channels; ++c)
            for(s = 0; s < spatial; ++s)
                 dst[c*spatial + s] += bias[c];
        \endverbatim
        Algorithm's details (example for NHWC tensor format):
        \verbatim
        for(s = 0; s < spatial; ++s)
            for(c = 0; c < channels; ++c)
                 dst[s*channels + c] += bias[c];
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] bias - a pointer to the 32-bit float array with bias coefficients. The size of the array is equal to channels.
        \param [in] channels - a number of channels in the tensor.
        \param [in] spatial - a spatial size (height * width) of the tensor.
        \param [in, out] dst - a pointer to FP32 tensor updated in place. The size of the array is equal to channels * spatial.
        \param [in] format - a format of the tensor.
    */
    SIMD_API void SimdSynetAddBias(const float * bias, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format);

    /*! @ingroup synet_add

        \fn void SimdSynetAdd8i(const uint8_t * aData, const float * aScale, const float* aShift, const uint8_t* bData, const float* bScale, const float* bShift, uint8_t* cData, const float* cScale, const float* cShift, size_t batch, size_t channels, size_t spatial, SimdTensorFormatType format, SimdSynetCompatibilityType compatibility);

        \short Dequantizes, adds and requantizes two UINT8 tensors.

        Algorithm's details (example for NCHW tensor format):
        \verbatim
        upper = isNarrowed(compatibility) ? 180 : 255;
        for(b = 0; b < batch; ++b)
            for(c = 0; c < channels; ++c)
                for(s = 0; s < spatial; ++s)
                {
                     offs = (b*channels + c)*spatial + s;
                     A = aData[offs]*aScale[c] + aShift[c];
                     B = bData[offs]*bScale[c] + bShift[c];
                     C = round((A + B)*cScale[c] + cShift[c]);
                     cData[offs] = restrict(C, 0, upper);
                }
        \endverbatim
        For NHWC tensor format the same calculation uses offset (b*spatial + s)*channels + c.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] aData - a pointer to the first input UINT8 tensor.
        \param [in] aScale - a pointer to the 32-bit float array with per-channel scale coefficients of the first input tensor.
        \param [in] aShift - a pointer to the 32-bit float array with per-channel shift coefficients of the first input tensor.
        \param [in] bData - a pointer to the second input UINT8 tensor.
        \param [in] bScale - a pointer to the 32-bit float array with per-channel scale coefficients of the second input tensor.
        \param [in] bShift - a pointer to the 32-bit float array with per-channel shift coefficients of the second input tensor.
        \param [out] cData - a pointer to the output UINT8 tensor.
        \param [in] cScale - a pointer to the 32-bit float array with per-channel scale coefficients of the output tensor.
        \param [in] cShift - a pointer to the 32-bit float array with per-channel shift coefficients of the output tensor.
        \param [in] batch - a batch size of input and output tensors.
        \param [in] channels - a number of channels in input and output tensors.
        \param [in] spatial - a spatial size (height * width) of input and output tensors.
        \param [in] format - a format of input and output tensors. Can be NCHW or NHWC.
        \param [in] compatibility - calculation compatibility flags. When narrowed 8-bit mode is active, output is limited to [0, 180], otherwise to [0, 255].
    */
    SIMD_API void SimdSynetAdd8i(const uint8_t * aData, const float * aScale, const float* aShift, const uint8_t* bData, const float* bScale, const float* bShift,
        uint8_t* cData, const float* cScale, const float* cShift, size_t batch, size_t channels, size_t spatial, SimdTensorFormatType format, SimdSynetCompatibilityType compatibility);

    /*! @ingroup synet_other

        \fn void SimdSynetChannelSum16b(const uint16_t* src, size_t channels, size_t spatial, SimdTensorFormatType format, float* sum);

        \short Calculates per-channel sums of a BF16 tensor in FP32 format.

        Algorithm's details (example for NCHW tensor format):
        \verbatim
        for(c = 0; c < channels; ++c)
        {
            sum[c] = 0;
            for(s = 0; s < spatial; ++s)
                sum[c] += BFloat16ToFloat32(src[c*spatial + s]);
        }
        \endverbatim
        Algorithm's details (example for NHWC tensor format):
        \verbatim
        for(c = 0; c < channels; ++c)
            sum[c] = 0;
        for(s = 0; s < spatial; ++s)
            for(c = 0; c < channels; ++c)
                sum[c] += BFloat16ToFloat32(src[s*channels + c]);
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input BF16 tensor.
        \param [in] channels - a number of channels in input tensor.
        \param [in] spatial - a spatial (width * height) size of input tensor.
        \param [in] format - a format of input tensor.
        \param [out] sum - a pointer to output 32-bit float array with channels sums.
    */
    SIMD_API void SimdSynetChannelSum16b(const uint16_t* src, size_t channels, size_t spatial, SimdTensorFormatType format, float* sum);

    /*! @ingroup synet_conversion

        \fn void SimdSynetConvert32fTo8u(const float * src, size_t batch, size_t channels, size_t height, size_t width, SimdTensorFormatType format, const float* scale, const float * shift, uint8_t * dst, SimdSynetCompatibilityType compatibility);

        \short Converts an FP32 tensor to a UINT8 tensor using per-channel scale and shift.

        Algorithm's details (example for NCHW tensor format):
        \verbatim
        upper = isNarrowed(compatibility) ? 180 : 255;
        for(b = 0; b < batch; ++b)
            for(c = 0; c < channels; ++c)
                for(h = 0; h < height; ++h)
                    for(w = 0; w < width; ++w)
                    {
                        offs = ((b*channels + c)*height + h)*width + w;
                        dst[offs] = restrict(round(src[offs]*scale[c] + shift[c]), 0, upper);
                    }
        \endverbatim
        For NHWC tensor format the same calculation uses offset ((b*height + h)*width + w)*channels + c.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>. 

        \param [in] src - a pointer to the FP32 input tensor.
        \param [in] batch - a batch size of input and output tensors.
        \param [in] channels - a number of channels in input and output tensors.
        \param [in] height - a height of input and output tensors.
        \param [in] width - a width of input and output tensors.
        \param [in] format - a format of input and output tensors. Can be NCHW or NHWC.
        \param [in] scale - a pointer to the 32-bit float array with per-channel scale coefficients.
        \param [in] shift - a pointer to the 32-bit float array with per-channel shift coefficients.
        \param [out] dst - a pointer to the UINT8 output tensor.
        \param [in] compatibility - calculation compatibility flags. When narrowed 8-bit mode is active, output is limited to [0, 180], otherwise to [0, 255].
    */
    SIMD_API void SimdSynetConvert32fTo8u(const float * src, size_t batch, size_t channels, size_t height, size_t width, SimdTensorFormatType format, const float* scale, const float * shift, uint8_t* dst, SimdSynetCompatibilityType compatibility);

    /*! @ingroup synet_conversion

        \fn void SimdSynetConvert8uTo32f(const uint8_t* src, size_t batch, size_t channels, size_t height, size_t width, SimdTensorFormatType format, const float* scale, const float* shift, float* dst, SimdSynetCompatibilityType compatibility);

        \short Converts a UINT8 tensor to an FP32 tensor using per-channel scale and shift.

        Algorithm's details (example for NCHW tensor format):
        \verbatim
        for(b = 0; b < batch; ++b)
            for(c = 0; c < channels; ++c)
                for(h = 0; h < height; ++h)
                    for(w = 0; w < width; ++w)
                    {
                        offs = ((b*channels + c)*height + h)*width + w;
                        dst[offs] = src[offs]*scale[c] + shift[c];
                    }
        \endverbatim
        For NHWC tensor format the same calculation uses offset ((b*height + h)*width + w)*channels + c.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the UINT8 input tensor.
        \param [in] batch - a batch size of input and output tensors.
        \param [in] channels - a number of channels in input and output tensors.
        \param [in] height - a height of input and output tensors.
        \param [in] width - a width of input and output tensors.
        \param [in] format - a format of input and output tensors. Can be NCHW or NHWC.
        \param [in] scale - a pointer to the 32-bit float array with per-channel scale coefficients.
        \param [in] shift - a pointer to the 32-bit float array with per-channel shift coefficients.
        \param [out] dst - a pointer to the FP32 output tensor.
        \param [in] compatibility - calculation compatibility flags.
    */
    SIMD_API void SimdSynetConvert8uTo32f(const uint8_t* src, size_t batch, size_t channels, size_t height, size_t width, SimdTensorFormatType format, const float* scale, const float* shift, float* dst, SimdSynetCompatibilityType compatibility);

    /*! @ingroup synet_convolution_fp32

        \fn void * SimdSynetConvolution32fInit(size_t batch, const SimdConvolutionParameters * conv);

        \short Initializes an FP32 convolution context.

        The function validates convolution parameters and chooses a suitable implementation (direct, depthwise,
        Winograd, NHWC-specialized or GEMM-based). It supports FP32 source and destination tensors with matching
        NCHW or NHWC format. The destination spatial size must match convolution parameters:
        \verbatim
        dstH = (srcH + padY + padH - (dilationY*(kernelY - 1) + 1)) / strideY + 1
        dstW = (srcW + padX + padW - (dilationX*(kernelX - 1) + 1)) / strideX + 1
        \endverbatim

        A created context stores tensor shape, format, convolution geometry, group count and activation type.
        Weights, bias and activation parameters are attached later by ::SimdSynetConvolution32fSetParams.

        \param [in] batch - a batch size.
        \param [in] conv - a pointer to convolution parameters. Source and destination tensor types must be FP32.
        \return a pointer to FP32 convolution context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in functions ::SimdSynetConvolution32fExternalBufferSize, ::SimdSynetConvolution32fInternalBufferSize, 
            ::SimdSynetConvolution32fInfo, ::SimdSynetConvolution32fSetParams and ::SimdSynetConvolution32fForward.
    */
    SIMD_API void * SimdSynetConvolution32fInit(size_t batch, const SimdConvolutionParameters * conv);

    /*! @ingroup synet_convolution_fp32

        \fn size_t SimdSynetConvolution32fExternalBufferSize(const void * context);

        \short Gets the size of caller-provided temporary buffer for FP32 convolution.

        The returned value is a number of 32-bit float elements, not bytes. It depends on the implementation selected
        during initialization and can be used to allocate the \a buf argument of ::SimdSynetConvolution32fForward.
        Some implementations return 1 when they do not need external temporary storage.

        \param [in] context - a pointer to FP32 convolution context. It must be created by function ::SimdSynetConvolution32fInit and released by function ::SimdRelease.
        \return a number of FP32 elements required for external temporary buffer.
    */
    SIMD_API size_t SimdSynetConvolution32fExternalBufferSize(const void * context);

    /*! @ingroup synet_convolution_fp32

        \fn size_t SimdSynetConvolution32fInternalBufferSize(const void * context);

        \short Gets the size of internal storage used by an FP32 convolution context.

        The returned value is a number of 32-bit float elements, not bytes. It reports internal storage tracked by
        the selected implementation, such as internal temporary buffers and implementation-specific reordered weights,
        bias or activation parameters already allocated by the context.

        \param [in] context - a pointer to FP32 convolution context. It must be created by function ::SimdSynetConvolution32fInit and released by function ::SimdRelease.
        \return a number of FP32 elements used by internal buffers.
    */
    SIMD_API size_t SimdSynetConvolution32fInternalBufferSize(const void * context);

    /*! @ingroup synet_convolution_fp32

        \fn const char* SimdSynetConvolution32fInfo(const void* context);

        \short Gets a short description of the selected FP32 convolution implementation.

        The returned string contains the implementation extension and algorithm name, for example a direct, depthwise,
        Winograd, NHWC direct or GEMM-based variant. The returned pointer is owned by the context and remains valid
        until the next call of this function for the same context or until the context is released.

        \param [in] context - a pointer to FP32 convolution context. It must be created by function ::SimdSynetConvolution32fInit and released by function ::SimdRelease.
        \return a string with description of internal implementation of FP32 convolution algorithm.
    */
    SIMD_API const char* SimdSynetConvolution32fInfo(const void* context);

    /*! @ingroup synet_convolution_fp32

        \fn void SimdSynetConvolution32fSetParams(void * context, const float * weight, SimdBool * internal, const float * bias, const float * params);

        \short Sets weights, bias and activation parameters for FP32 convolution.

        This function must be called before ::SimdSynetConvolution32fForward. The \a weight array contains FP32
        convolution weights with kernelY*kernelX*srcC*dstC/group elements. Depending on the selected implementation,
        weights can be used directly or transformed and stored inside the context. If \a internal is not NULL, the
        selected implementation writes the weight storage mode to it: SimdTrue means that weights were transformed and
        stored internally, while SimdFalse means that the implementation may use the original \a weight array directly,
        so the caller must keep it valid for later forward calls. Bias and activation parameters can also be copied
        internally by some implementations; otherwise their pointers are stored in the context.

        \param [in, out] context - a pointer to FP32 convolution context. It must be created by function ::SimdSynetConvolution32fInit and released by function ::SimdRelease.
        \param [in] weight - a pointer to FP32 convolution weights.
        \param [out] internal - a pointer to a flag receiving weight ownership mode. Can be NULL.
        \param [in] bias - a pointer to FP32 bias array with dstC elements. Can be NULL.
        \param [in] params - a pointer to FP32 parameters of activation function (see ::SimdConvolutionActivationType).
            Can be NULL when activation does not require parameters.
    */
    SIMD_API void SimdSynetConvolution32fSetParams(void * context, const float * weight, SimdBool * internal, const float * bias, const float * params);

    /*! @ingroup synet_convolution_fp32

        \fn void SimdSynetConvolution32fForward(void * context, const float * src, float * buf, float * dst);

        \short Performs forward propagation of FP32 convolution.

        The function convolves each image in the batch, adds bias when it was set, and applies the activation specified
        in ::SimdConvolutionParameters:
        \verbatim
        sum = bias == NULL ? 0 : bias[dc];
        for(sc = 0; sc < srcC/group; ++sc)
            for(ky = 0; ky < kernelY; ++ky)
                for(kx = 0; kx < kernelX; ++kx)
                    sum += src[inputOffset] * weight[weightOffset];
        dst[outputOffset] = Activate(sum, activation, params);
        \endverbatim
        The exact offsets depend on tensor format, padding, dilation, stride and group. The input and output tensors
        use the shape and format from the context created by ::SimdSynetConvolution32fInit.

        \param [in] context - a pointer to FP32 convolution context. It must be created by function ::SimdSynetConvolution32fInit and released by function ::SimdRelease.
        \param [in] src - a pointer to FP32 input tensor.
        \param [out] buf - a pointer to external temporary FP32 buffer. The required number of elements is determined by function ::SimdSynetConvolution32fExternalBufferSize. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to FP32 output tensor.
    */
    SIMD_API void SimdSynetConvolution32fForward(void * context, const float * src, float * buf, float * dst);

    /*! @ingroup synet_convolution_bf16

        \fn void * SimdSynetConvolution16bInit(size_t batch, const SimdConvolutionParameters * conv, SimdSynetCompatibilityType compatibility);

        \short Initializes a BF16/FP32 convolution context.

        The function validates convolution parameters and chooses a suitable BF16-oriented implementation (GEMM,
        NCHW/NHWC GEMM, NHWC depthwise, NHWC special convolution or AMX-BF16 variant when available). It supports
        FP32 or BF16 source and destination tensors with matching NCHW or NHWC format. The destination spatial size
        must match convolution parameters:
        \verbatim
        dstH = (srcH + padY + padH - (dilationY*(kernelY - 1) + 1)) / strideY + 1
        dstW = (srcW + padX + padW - (dilationX*(kernelX - 1) + 1)) / strideX + 1
        \endverbatim

        A created context stores tensor shape, data types, format, convolution geometry, group count, activation type
        and compatibility flags. FP32 weights, bias and activation parameters are attached later by
        ::SimdSynetConvolution16bSetParams.

        \param [in] batch - a batch size.
        \param [in] conv - a pointer to convolution parameters. Source and destination tensor types must be FP32 or BF16.
        \param [in] compatibility - calculation compatibility flags.
        \return a pointer to BF16 convolution context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in functions ::SimdSynetConvolution16bExternalBufferSize, ::SimdSynetConvolution16bInternalBufferSize,
            ::SimdSynetConvolution16bInfo, ::SimdSynetConvolution16bSetParams and ::SimdSynetConvolution16bForward.
    */
    SIMD_API void* SimdSynetConvolution16bInit(size_t batch, const SimdConvolutionParameters* conv, SimdSynetCompatibilityType compatibility);

    /*! @ingroup synet_convolution_bf16

        \fn size_t SimdSynetConvolution16bExternalBufferSize(const void * context);

        \short Gets the size in bytes of caller-provided temporary buffer for BF16 convolution.

        The returned value is a number of bytes. It depends on the implementation selected during initialization and
        can be used to allocate the \a buf argument of ::SimdSynetConvolution16bForward. Some implementations return 1
        or 0 when they do not need external temporary storage.

        \param [in] context - a pointer to BF16 convolution context. It must be created by function ::SimdSynetConvolution16bInit and released by function ::SimdRelease.
        \return a number of bytes required for external temporary buffer.
    */
    SIMD_API size_t SimdSynetConvolution16bExternalBufferSize(const void* context);

    /*! @ingroup synet_convolution_bf16

        \fn size_t SimdSynetConvolution16bInternalBufferSize(const void * context);

        \short Gets the size in bytes of internal storage used by a BF16 convolution context.

        The returned value reports internal storage tracked by the selected implementation, including internal
        temporary buffers, transformed weights, copied bias and copied activation parameters.

        \param [in] context - a pointer to BF16 convolution context. It must be created by function ::SimdSynetConvolution16bInit and released by function ::SimdRelease.
        \return a number of bytes used by internal buffers.
    */
    SIMD_API size_t SimdSynetConvolution16bInternalBufferSize(const void* context);

    /*! @ingroup synet_convolution_bf16

        \fn const char* SimdSynetConvolution16bInfo(const void* context);

        \short Gets a short description of the selected BF16 convolution implementation.

        The returned string contains the implementation extension and algorithm name, for example a GEMM, NCHW/NHWC
        GEMM, NHWC depthwise, NHWC special or AMX-BF16 variant. The returned pointer is owned by the context and
        remains valid until the next call of this function for the same context or until the context is released.

        \param [in] context - a pointer to BF16 convolution context. It must be created by function ::SimdSynetConvolution16bInit and released by function ::SimdRelease.
        \return a string with description of internal implementation of BF16 convolution algorithm.
    */
    SIMD_API const char* SimdSynetConvolution16bInfo(const void* context);

    /*! @ingroup synet_convolution_bf16

        \fn void SimdSynetConvolution16bSetParams(void * context, const float * weight, const float * bias, const float * params);

        \short Sets weights, bias and activation parameters for BF16 convolution.

        This function must be called before ::SimdSynetConvolution16bForward. The \a weight array contains FP32
        convolution weights with kernelY*kernelX*srcC*dstC/group elements. The selected implementation transforms
        weights to its internal representation (usually BF16 and reordered; some depthwise paths keep FP32 weights).
        Bias is copied to an internal FP32 array; when \a bias is NULL, zeros are used. Activation parameters are
        copied or expanded to the internal FP32 array according to ::SimdConvolutionActivationType.

        \param [in, out] context - a pointer to BF16 convolution context. It must be created by function ::SimdSynetConvolution16bInit and released by function ::SimdRelease.
        \param [in] weight - a pointer to FP32 convolution weights.
        \param [in] bias - a pointer to FP32 bias array with dstC elements. Can be NULL.
        \param [in] params - a pointer to FP32 parameters of activation function (see ::SimdConvolutionActivationType).
            Can be NULL when activation does not require parameters.
    */
    SIMD_API void SimdSynetConvolution16bSetParams(void* context, const float* weight, const float* bias, const float* params);

    /*! @ingroup synet_convolution_bf16

        \fn void SimdSynetConvolution16bForward(void * context, const uint8_t * src, uint8_t * buf, uint8_t * dst);

        \short Performs forward propagation of BF16/FP32 convolution.

        The function converts FP32 input to BF16 when the context source type is FP32, uses BF16 input directly when
        the source type is BF16, accumulates convolution sums in FP32, adds bias, applies activation and writes FP32
        or BF16 output according to the context destination type:
        \verbatim
        sum = bias[dc];
        for(sc = 0; sc < srcC/group; ++sc)
            for(ky = 0; ky < kernelY; ++ky)
                for(kx = 0; kx < kernelX; ++kx)
                    sum += inputValue * weightValue;
        value = Activate(sum, activation, params);
        dst[outputOffset] = dstT == SimdTensorData16b ? Float32ToBFloat16(value) : value;
        \endverbatim
        The input value is read as BF16 or converted from FP32 to BF16 according to srcT. The weight value comes from
        the internal representation prepared by ::SimdSynetConvolution16bSetParams.
        The exact offsets depend on tensor format, padding, dilation, stride and group. The input and output tensors
        use the shape, data types and format from the context created by ::SimdSynetConvolution16bInit.

        \param [in] context - a pointer to BF16 convolution context. It must be created by function ::SimdSynetConvolution16bInit and released by function ::SimdRelease.
        \param [in] src - a pointer to input tensor. Actual element type is defined by srcT in convolution parameters.
        \param [out] buf - a pointer to external temporary byte buffer. The required size is determined by function ::SimdSynetConvolution16bExternalBufferSize. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to output tensor. Actual element type is defined by dstT in convolution parameters.
    */
    SIMD_API void SimdSynetConvolution16bForward(void* context, const uint8_t* src, uint8_t* buf, uint8_t* dst);

    /*! @ingroup synet_convolution_int8

        \fn void * SimdSynetConvolution8iInit(size_t batch, const SimdConvolutionParameters * conv, SimdSynetCompatibilityType compatibility);

        \short Initializes an INT8 convolution context.

        The function validates convolution parameters and chooses a suitable implementation (GEMM, NHWC direct,
        NHWC depthwise or architecture-specific VNNI/AMX/NEON variant when available). It supports FP32 or UINT8
        source and destination tensors with matching NCHW or NHWC format. The destination spatial size must match
        convolution parameters:
        \verbatim
        dstH = (srcH + padY + padH - (dilationY*(kernelY - 1) + 1)) / strideY + 1
        dstW = (srcW + padX + padW - (dilationX*(kernelX - 1) + 1)) / strideX + 1
        \endverbatim

        A created context stores tensor shape, data types, format, convolution geometry, group count, activation type
        and compatibility flags. FP32 weights, bias, activation parameters and tensor statistics are attached later by
        ::SimdSynetConvolution8iSetParams.

        \param [in] batch - a batch size.
        \param [in] conv - a pointer to convolution parameters. Source and destination tensor types must be FP32 or UINT8.
        \param [in] compatibility - calculation compatibility flags. They select precise, overflow or narrowed INT8
            calculation mode. Narrowed mode uses unsigned range [0, 180] and signed range [-90, 90]; otherwise
            ranges are [0, 255] and [-128, 127].
        \return a pointer to INT8 convolution context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in functions ::SimdSynetConvolution8iExternalBufferSize, ::SimdSynetConvolution8iInternalBufferSize, 
            ::SimdSynetConvolution8iInfo, ::SimdSynetConvolution8iSetParams and ::SimdSynetConvolution8iForward.
    */
    SIMD_API void * SimdSynetConvolution8iInit(size_t batch, const SimdConvolutionParameters * conv, SimdSynetCompatibilityType compatibility);

    /*! @ingroup synet_convolution_int8

        \fn size_t SimdSynetConvolution8iExternalBufferSize(const void * context);

        \short Gets the size in bytes of caller-provided temporary buffer for INT8 convolution.

        The returned value is a number of bytes. It depends on the implementation selected during initialization and
        can be used to allocate the \a buf argument of ::SimdSynetConvolution8iForward. The buffer can contain temporary
        UINT8 source conversion data, im2col/padded input data, INT32 sums and temporary FP32 output data.

        \param [in] context - a pointer to INT8 convolution context. It must be created by function ::SimdSynetConvolution8iInit and released by function ::SimdRelease.
        \return a number of bytes required for external temporary buffer.
    */
    SIMD_API size_t SimdSynetConvolution8iExternalBufferSize(const void * context);

    /*! @ingroup synet_convolution_int8

        \fn size_t SimdSynetConvolution8iInternalBufferSize(const void * context);

        \short Gets the size in bytes of internal storage used by an INT8 convolution context.

        The returned value reports internal storage tracked by the selected implementation, including internal
        temporary buffers, quantized/reordered INT8 weights, source and destination conversion parameters,
        normalization, bias and activation parameters.

        \param [in] context - a pointer to INT8 convolution context. It must be created by function ::SimdSynetConvolution8iInit and released by function ::SimdRelease.
        \return a number of bytes used by internal buffers.
    */
    SIMD_API size_t SimdSynetConvolution8iInternalBufferSize(const void * context);

    /*! @ingroup synet_convolution_int8

        \fn const char* SimdSynetConvolution8iInfo(const void* context);

        \short Gets a short description of the selected INT8 convolution implementation.

        The returned string contains the implementation extension and algorithm name, for example a GEMM, NHWC direct
        or NHWC depthwise variant, with a suffix for precise, overflow or narrowed mode when applicable. The returned
        pointer is owned by the context and remains valid until the next call of this function for the same context or
        until the context is released.

        \param [in] context - a pointer to INT8 convolution context. It must be created by function ::SimdSynetConvolution8iInit and released by function ::SimdRelease.
        \return a string with description of internal implementation of INT8 convolution algorithm.
    */
    SIMD_API const char* SimdSynetConvolution8iInfo(const void* context);

    /*! @ingroup synet_convolution_int8

        \fn void SimdSynetConvolution8iSetParams(void * context, const float * weight, const float * bias, const float * params, const float * const * stats);

        \short Sets weights, bias, activation parameters and tensor statistics for INT8 convolution.

        This function must be called before ::SimdSynetConvolution8iForward. The \a weight array contains FP32
        convolution weights with kernelY*kernelX*srcC*dstC/group elements. Source statistics (\a stats[0],
        \a stats[1], each with srcC elements) define per-channel source quantization parameters; destination statistics
        (\a stats[2], \a stats[3], each with dstC elements) define per-channel output quantization parameters. The
        selected implementation converts weights to INT8, may reorder them, and computes per-output-channel normalization
        and bias terms used to convert INT32 sums back to FP32. Activation parameters are copied or expanded internally
        according to ::SimdConvolutionActivationType.

        \param [in, out] context - a pointer to INT8 convolution context. It must be created by function ::SimdSynetConvolution8iInit and released by function ::SimdRelease.
        \param [in] weight - a pointer to FP32 convolution weights.
        \param [in] bias - a pointer to FP32 bias array with dstC elements. Can be NULL.
        \param [in] params - a pointer to FP32 parameters of activation function (see ::SimdConvolutionActivationType).
            Can be NULL when activation does not require parameters.
        \param [in] stats - a pointer to pointers with per-channel tensor statistics:
            source minimum stats[0], source maximum stats[1], destination minimum stats[2], destination maximum stats[3].
    */
    SIMD_API void SimdSynetConvolution8iSetParams(void * context, const float * weight, const float * bias, const float * params, const float * const* stats);

    /*! @ingroup synet_convolution_int8

        \fn void SimdSynetConvolution8iForward(void * context, const uint8_t * src, uint8_t * buf, uint8_t * dst);

        \short Performs forward propagation of INT8 convolution.

        The function converts FP32 input to UINT8 when the context source type is FP32, uses UINT8 input directly when
        the source type is UINT8, accumulates convolution sums in INT32 with INT8 weights, converts sums to FP32 using
        internal normalization and bias, applies activation, and writes FP32 or UINT8 output according to the context
        destination type:
        \verbatim
        if(srcT == SimdTensorData32f)
            src8u = restrict(round(src32f*srcScale[c] + srcShift[c]), srcLower, srcUpper);
        sum = convolution_int32(src8u, weight8i, zero);
        value = Activate(sum*norm[dc] + bias[dc], activation, params);
        dst[outputOffset] = dstT == SimdTensorData8u ?
            restrict(round(value*dstScale[dc] + dstShift[dc]), dstLower, dstUpper) : value;
        \endverbatim
        The exact offsets depend on tensor format, padding, dilation, stride and group. The input and output tensors
        use the shape, data types and format from the context created by ::SimdSynetConvolution8iInit.

        \param [in] context - a pointer to INT8 convolution context. It must be created by function ::SimdSynetConvolution8iInit and released by function ::SimdRelease.
        \param [in] src - a pointer to input tensor. Actual element type is defined by srcT in convolution parameters.
        \param [out] buf - a pointer to external temporary byte buffer. The required size is determined by function ::SimdSynetConvolution8iExternalBufferSize. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to output tensor. Actual element type is defined by dstT in convolution parameters.
    */
    SIMD_API void SimdSynetConvolution8iForward(void * context, const uint8_t * src, uint8_t * buf, uint8_t * dst);

    /*! @ingroup synet_deconvolution_fp32

        \fn void * SimdSynetDeconvolution32fInit(size_t batch, const SimdConvolutionParameters * conv, SimdSynetCompatibilityType compatibility);

        \short Initializes an FP32 deconvolution context.

        The function validates deconvolution parameters and chooses a suitable implementation (GEMM-based or
        NHWC direct 2x2 when available). It supports FP32 source and destination tensors with matching NCHW format,
        or matching NHWC format when group is 1. The destination spatial size must match deconvolution parameters:
        \verbatim
        dstH = strideY*(srcH - 1) + dilationY*(kernelY - 1) + 1 - padY - padH
        dstW = strideX*(srcW - 1) + dilationX*(kernelX - 1) + 1 - padX - padW
        \endverbatim

        A created context stores tensor shape, format, deconvolution geometry, group count, activation type and
        compatibility flags. Weights, bias and activation parameters are attached later by
        ::SimdSynetDeconvolution32fSetParams.

        \param [in] batch - a batch size.
        \param [in] conv - a pointer to deconvolution parameters. Source and destination tensor types must be FP32.
        \param [in] compatibility - calculation compatibility flags.
        \return a pointer to FP32 deconvolution context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in functions ::SimdSynetDeconvolution32fExternalBufferSize, ::SimdSynetDeconvolution32fInternalBufferSize, 
            ::SimdSynetDeconvolution32fInfo, ::SimdSynetDeconvolution32fSetParams and ::SimdSynetDeconvolution32fForward.
    */
    SIMD_API void * SimdSynetDeconvolution32fInit(size_t batch, const SimdConvolutionParameters * conv, SimdSynetCompatibilityType compatibility);

    /*! @ingroup synet_deconvolution_fp32

        \fn size_t SimdSynetDeconvolution32fExternalBufferSize(const void * context);

        \short Gets the size of caller-provided temporary buffer for FP32 deconvolution.

        The returned value is a number of 32-bit float elements, not bytes. It depends on the implementation selected
        during initialization and can be used to allocate the \a buf argument of ::SimdSynetDeconvolution32fForward.
        Some implementations return 1 when they do not need external temporary storage.

        \param [in] context - a pointer to FP32 deconvolution context. It must be created by function ::SimdSynetDeconvolution32fInit and released by function ::SimdRelease.
        \return a number of FP32 elements required for external temporary buffer.
    */
    SIMD_API size_t SimdSynetDeconvolution32fExternalBufferSize(const void * context);

    /*! @ingroup synet_deconvolution_fp32

        \fn size_t SimdSynetDeconvolution32fInternalBufferSize(const void * context);

        \short Gets the size of internal storage used by an FP32 deconvolution context.

        The returned value is a number of 32-bit float elements, not bytes. It reports internal storage tracked by
        the selected implementation, such as internal temporary buffers and implementation-specific reordered weights,
        bias or activation parameters already allocated by the context.

        \param [in] context - a pointer to FP32 deconvolution context. It must be created by function ::SimdSynetDeconvolution32fInit and released by function ::SimdRelease.
        \return a number of FP32 elements used by internal buffers.
    */
    SIMD_API size_t SimdSynetDeconvolution32fInternalBufferSize(const void * context);

    /*! @ingroup synet_deconvolution_fp32

        \fn const char* SimdSynetDeconvolution32fInfo(const void* context);

        \short Gets a short description of the selected FP32 deconvolution implementation.

        The returned string contains the implementation extension and algorithm name, for example a GEMM-based or
        NHWC direct 2x2 variant. The returned pointer is owned by the context and remains valid until the next call
        of this function for the same context or until the context is released.

        \param [in] context - a pointer to FP32 deconvolution context. It must be created by function ::SimdSynetDeconvolution32fInit and released by function ::SimdRelease.
        \return a string with description of internal implementation of FP32 deconvolution algorithm.
    */
    SIMD_API const char* SimdSynetDeconvolution32fInfo(const void* context);

    /*! @ingroup synet_deconvolution_fp32

        \fn void SimdSynetDeconvolution32fSetParams(void * context, const float * weight, SimdBool * internal, const float * bias, const float * params);

        \short Sets weights, bias and activation parameters for FP32 deconvolution.

        This function must be called before ::SimdSynetDeconvolution32fForward. The \a weight array contains FP32
        deconvolution weights with kernelY*kernelX*srcC*dstC/group elements. Depending on the selected implementation,
        weights can be used directly or transformed and stored inside the context. If \a internal is not NULL, the
        selected implementation writes the weight storage mode to it: SimdTrue means that weights were transformed and
        stored internally, while SimdFalse means that the implementation may use the original \a weight array directly,
        so the caller must keep it valid for later forward calls. Bias and activation parameters can also be copied
        internally by some implementations; otherwise their pointers are stored in the context.

        \param [in, out] context - a pointer to FP32 deconvolution context. It must be created by function ::SimdSynetDeconvolution32fInit and released by function ::SimdRelease.
        \param [in] weight - a pointer to FP32 deconvolution weights.
        \param [out] internal - a pointer to a flag receiving weight ownership mode. Can be NULL.
        \param [in] bias - a pointer to FP32 bias array with dstC elements. Can be NULL.
        \param [in] params - a pointer to FP32 parameters of activation function (see ::SimdConvolutionActivationType).
            Can be NULL when activation does not require parameters.
    */
    SIMD_API void SimdSynetDeconvolution32fSetParams(void * context, const float * weight, SimdBool * internal, const float * bias, const float * params);

    /*! @ingroup synet_deconvolution_fp32

        \fn void SimdSynetDeconvolution32fForward(void * context, const float * src, float * buf, float * dst);

        \short Performs forward propagation of FP32 deconvolution.

        The function applies transposed convolution to each image in the batch, adds bias when it was set, and applies
        the activation specified in ::SimdConvolutionParameters:
        \verbatim
        dst[:] = 0;
        for(sc = 0; sc < srcC/group; ++sc)
            for(sy = 0; sy < srcH; ++sy)
                for(sx = 0; sx < srcW; ++sx)
                    for(ky = 0; ky < kernelY; ++ky)
                        for(kx = 0; kx < kernelX; ++kx)
                            dst[outputOffset] += src[inputOffset] * weight[weightOffset];
        dst[outputOffset] = Activate(dst[outputOffset] + bias[dc], activation, params);
        \endverbatim
        The exact offsets depend on tensor format, padding, dilation, stride and group. The input and output tensors
        use the shape and format from the context created by ::SimdSynetDeconvolution32fInit.

        \param [in] context - a pointer to FP32 deconvolution context. It must be created by function ::SimdSynetDeconvolution32fInit and released by function ::SimdRelease.
        \param [in] src - a pointer to FP32 input tensor.
        \param [out] buf - a pointer to external temporary FP32 buffer. The required number of elements is determined by
            function ::SimdSynetDeconvolution32fExternalBufferSize. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to FP32 output tensor.
    */
    SIMD_API void SimdSynetDeconvolution32fForward(void * context, const float * src, float * buf, float * dst);

    /*! @ingroup synet_deconvolution_bf16

    \fn void * SimdSynetDeconvolution16bInit(size_t batch, const SimdConvolutionParameters * conv, SimdSynetCompatibilityType compatibility);

    \short Initializes a BF16/FP32 deconvolution context.

    The function validates deconvolution parameters and chooses a suitable BF16-oriented implementation (GEMM-based
    or NHWC GEMM-based variant, including AMX-BF16 when available). It supports FP32 or BF16 source and destination
    tensors with matching NCHW format, or matching NHWC format when group is 1. The destination spatial size must
    match deconvolution parameters:
    \verbatim
    dstH = strideY*(srcH - 1) + dilationY*(kernelY - 1) + 1 - padY - padH
    dstW = strideX*(srcW - 1) + dilationX*(kernelX - 1) + 1 - padX - padW
    \endverbatim

    A created context stores tensor shape, data types, format, deconvolution geometry, group count, activation type
    and compatibility flags. FP32 weights, bias and activation parameters are attached later by
    ::SimdSynetDeconvolution16bSetParams.

    \param [in] batch - a batch size.
    \param [in] conv - a pointer to deconvolution parameters. Source and destination tensor types must be FP32 or BF16.
    \param [in] compatibility - calculation compatibility flags.
    \return a pointer to BF16 deconvolution context. On error it returns NULL. It must be released with using of function ::SimdRelease.
        This pointer is used in functions ::SimdSynetDeconvolution16bExternalBufferSize, ::SimdSynetDeconvolution16bInternalBufferSize,
        ::SimdSynetDeconvolution16bInfo, ::SimdSynetDeconvolution16bSetParams and ::SimdSynetDeconvolution16bForward.
*/
    SIMD_API void* SimdSynetDeconvolution16bInit(size_t batch, const SimdConvolutionParameters* conv, SimdSynetCompatibilityType compatibility);

    /*! @ingroup synet_deconvolution_bf16

        \fn size_t SimdSynetDeconvolution16bExternalBufferSize(const void * context);

        \short Gets the size in bytes of caller-provided temporary buffer for BF16 deconvolution.

        The returned value is a number of bytes. It depends on the implementation selected during initialization and
        can be used to allocate the \a buf argument of ::SimdSynetDeconvolution16bForward. Some implementations return
        1 or 0 when they do not need external temporary storage.

        \param [in] context - a pointer to BF16 deconvolution context. It must be created by function ::SimdSynetDeconvolution16bInit and released by function ::SimdRelease.
        \return a number of bytes required for external temporary buffer.
    */
    SIMD_API size_t SimdSynetDeconvolution16bExternalBufferSize(const void* context);

    /*! @ingroup synet_deconvolution_bf16

        \fn size_t SimdSynetDeconvolution16bInternalBufferSize(const void * context);

        \short Gets the size in bytes of internal storage used by a BF16 deconvolution context.

        The returned value reports internal storage tracked by the selected implementation, including internal
        temporary buffers, transformed weights, copied bias and copied activation parameters.

        \param [in] context - a pointer to BF16 deconvolution context. It must be created by function ::SimdSynetDeconvolution16bInit and released by function ::SimdRelease.
        \return a number of bytes used by internal buffers.
    */
    SIMD_API size_t SimdSynetDeconvolution16bInternalBufferSize(const void* context);

    /*! @ingroup synet_deconvolution_bf16

        \fn const char* SimdSynetDeconvolution16bInfo(const void* context);

        \short Gets a short description of the selected BF16 deconvolution implementation.

        The returned string contains the implementation extension and algorithm name, for example a GEMM or NHWC GEMM
        variant. The returned pointer is owned by the context and remains valid until the next call of this function
        for the same context or until the context is released.

        \param [in] context - a pointer to BF16 deconvolution context. It must be created by function ::SimdSynetDeconvolution16bInit and released by function ::SimdRelease.
        \return a string with description of internal implementation of BF16 deconvolution algorithm.
    */
    SIMD_API const char* SimdSynetDeconvolution16bInfo(const void* context);

    /*! @ingroup synet_deconvolution_bf16

        \fn void SimdSynetDeconvolution16bSetParams(void * context, const float * weight, const float * bias, const float * params);

        \short Sets weights, bias and activation parameters for BF16 deconvolution.

        This function must be called before ::SimdSynetDeconvolution16bForward. The \a weight array contains FP32
        deconvolution weights with kernelY*kernelX*srcC*dstC/group elements. The selected implementation transforms
        weights to its internal BF16/reordered representation. Bias is copied to an internal FP32 array; when \a bias
        is NULL, zeros are used. Activation parameters are copied or expanded to the internal FP32 array according to
        ::SimdConvolutionActivationType.

        \param [in, out] context - a pointer to BF16 deconvolution context. It must be created by function ::SimdSynetDeconvolution16bInit and released by function ::SimdRelease.
        \param [in] weight - a pointer to FP32 deconvolution weights.
        \param [in] bias - a pointer to FP32 bias array with dstC elements. Can be NULL.
        \param [in] params - a pointer to FP32 parameters of activation function (see ::SimdConvolutionActivationType).
            Can be NULL when activation does not require parameters.
    */
    SIMD_API void SimdSynetDeconvolution16bSetParams(void* context, const float* weight, const float* bias, const float* params);

    /*! @ingroup synet_deconvolution_bf16

        \fn void SimdSynetDeconvolution16bForward(void * context, const uint8_t * src, uint8_t * buf, uint8_t * dst);

        \short Performs forward propagation of BF16/FP32 deconvolution.

        The function converts FP32 input to BF16 when the context source type is FP32, uses BF16 input directly when
        the source type is BF16, accumulates transposed convolution sums in FP32, adds bias, applies activation and
        writes FP32 or BF16 output according to the context destination type:
        \verbatim
        dst[:] = 0;
        for(sc = 0; sc < srcC/group; ++sc)
            for(sy = 0; sy < srcH; ++sy)
                for(sx = 0; sx < srcW; ++sx)
                    for(ky = 0; ky < kernelY; ++ky)
                        for(kx = 0; kx < kernelX; ++kx)
                            dst[outputOffset] += inputValue * weightValue;
        value = Activate(dst[outputOffset] + bias[dc], activation, params);
        dst[outputOffset] = dstT == SimdTensorData16b ? Float32ToBFloat16(value) : value;
        \endverbatim
        The input value is read as BF16 or converted from FP32 to BF16 according to srcT. The weight value comes from
        the internal representation prepared by ::SimdSynetDeconvolution16bSetParams.
        The exact offsets depend on tensor format, padding, dilation, stride and group. The input and output tensors
        use the shape, data types and format from the context created by ::SimdSynetDeconvolution16bInit.

        \param [in] context - a pointer to BF16 deconvolution context. It must be created by function ::SimdSynetDeconvolution16bInit and released by function ::SimdRelease.
        \param [in] src - a pointer to input tensor. Actual element type is defined by srcT in deconvolution parameters.
        \param [out] buf - a pointer to external temporary byte buffer. The required size is determined by function ::SimdSynetDeconvolution16bExternalBufferSize. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to output tensor. Actual element type is defined by dstT in deconvolution parameters.
    */
    SIMD_API void SimdSynetDeconvolution16bForward(void* context, const uint8_t* src, uint8_t* buf, uint8_t* dst);

    /*! @ingroup synet_quantized_other

        \fn void SimdSynetDequantizeLinear(const uint8_t* src, size_t size, int32_t bias, const float* norm, float* dst);

        \short Dequantizes a UINT8 tensor to FP32 with a single scale and zero-point correction.

        Algorithm's details for ::SimdSynetDequantizeLinear:
        \verbatim
        for(i = 0; i < size; ++i)
            dst[i] = (src[i] + bias) * norm[0];
        \endverbatim

        This corresponds to dst = (src - zero) * scale when \a bias is equal to -zero and \a norm[0] is equal to scale.

        \param [in] src - a pointer to UINT8 input tensor.
        \param [in] size - a number of elements in the input and output tensors.
        \param [in] bias - an integer value added to every input element, typically negative zero-point.
        \param [in] norm - a pointer to FP32 dequantization scale. Only norm[0] is used.
        \param [out] dst - a pointer to FP32 output tensor.
    */
    SIMD_API void SimdSynetDequantizeLinear(const uint8_t* src, size_t size, int32_t bias, const float* norm, float* dst);

    /*! @ingroup synet_other

        \fn void SimdSynetEltwiseLayerForward(float const * const * src, const float * weight, size_t count, size_t size, SimdSynetEltwiseOperationType type, float * dst);

        \short Performs element-wise product, weighted sum, maximum or minimum over several FP32 tensors.

        The function reads \a count input arrays of equal length \a size and writes one output array.
        The \a weight array is used only for ::SimdSynetEltwiseOperationSum.

        Algorithm's details for ::SimdSynetEltwiseOperationProduct:
        \verbatim
        for(j = 0; j < size; ++j)
            dst[j] = src[0][j] * src[1][j];
        for(i = 2; i < count; ++i)
            for(j = 0; j < size; ++j)
                dst[j] *= src[i][j];
        \endverbatim

        Algorithm's details for ::SimdSynetEltwiseOperationSum:
        \verbatim
        for(j = 0; j < size; ++j)
            dst[j] = src[0][j]*weight[0] + src[1][j]*weight[1];
        for(i = 2; i < count; ++i)
            for(j = 0; j < size; ++j)
                dst[j] += src[i][j]*weight[i];
        \endverbatim

        Algorithm's details for ::SimdSynetEltwiseOperationMax:
        \verbatim
        for(j = 0; j < size; ++j)
            dst[j] = Max(src[0][j], src[1][j]);
        for(i = 2; i < count; ++i)
            for(j = 0; j < size; ++j)
                dst[j] = Max(dst[j], src[i][j]);
        \endverbatim

        Algorithm's details for ::SimdSynetEltwiseOperationMin:
        \verbatim
        for(j = 0; j < size; ++j)
            dst[j] = Min(src[0][j], src[1][j]);
        for(i = 2; i < count; ++i)
            for(j = 0; j < size; ++j)
                dst[j] = Min(dst[j], src[i][j]);
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to \a count pointers to input FP32 arrays.
        \param [in] weight - a pointer to FP32 weighted-sum coefficients. It is used only for ::SimdSynetEltwiseOperationSum; otherwise it can be NULL.
        \param [in] count - a count of input arrays. Must be at least 2.
        \param [in] size - a number of elements in each input and output array.
        \param [in] type - a type of operation (see ::SimdSynetEltwiseOperationType).
        \param [out] dst - a pointer to the output FP32 array.
    */
    SIMD_API void SimdSynetEltwiseLayerForward(float const * const * src, const float * weight, size_t count, size_t size, SimdSynetEltwiseOperationType type, float * dst);

    /*! @ingroup synet_activation

        \fn void SimdSynetElu32f(const float * src, size_t size, const float * alpha, float * dst);

        \short Calculates ELU activation for an FP32 array.

        The input and output arrays must have the same size.

        Algorithm's details:
        \verbatim
        for(i = 0; i < size; ++i)
            dst[i] = src[i] >= 0 ? src[i] : alpha[0]*(exp(src[i]) - 1);
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 array.
        \param [in] size - a number of elements in the input and output arrays.
        \param [in] alpha - a pointer to ELU alpha parameter. Only alpha[0] is used.
        \param [out] dst - a pointer to the output FP32 array.
    */
    SIMD_API void SimdSynetElu32f(const float * src, size_t size, const float * alpha, float * dst);

    /*! @ingroup synet_gather_elements

        \fn void* SimdSynetGatherElementsInit(SimdTensorDataType dataType, SimdTensorDataType indexType, SimdBool indexConst, size_t indexUsers, const size_t * outer, size_t outerSize, size_t srcCount, size_t inner, size_t idxCount);

        \short Initializes a gather-elements context.

        The function creates a context for ONNX-style GatherElements along the dimension of length \a srcCount.
        It supports FP32, BF16 and UINT8 data tensors and INT32 or INT64 index tensors. The input tensor shape is:
        \verbatim
        outer[0] * ... * outer[outerSize - 1] * srcCount * inner
        \endverbatim
        The index and output tensor shape is:
        \verbatim
        outer[0] * ... * outer[outerSize - 1] * idxCount * inner
        \endverbatim

        Algorithm's details:
        \verbatim
        for(b = 0; b < outer[0]*...*outer[outerSize - 1]; ++b)
            for(c = 0; c < idxCount; ++c)
                for(i = 0; i < inner; ++i)
                {
                    ic = idx[b, c, i];
                    if (ic < 0)
                        ic += srcCount;
                    dst[b, c, i] = src[b, ic, i];
                }
        \endverbatim

        If \a indexConst is SimdTrue, constant indexes can be analyzed by ::SimdSynetGatherElementsSetIndex to avoid
        repeated negative-index checks and to reduce repeated outer index processing when possible.

        \param [in] dataType - a type of input and output tensor. It can be FP32, BF16 or UINT8.
        \param [in] indexType - a type of index tensor. It can be INT32 or INT64.
        \param [in] indexConst - a flag indicating that index tensor is constant and can be set once.
        \param [in] indexUsers - a number of consumers sharing the same constant index tensor. The current implementation stores this value but does not otherwise use it.
        \param [in] outer - a pointer to outer shape dimensions before the gathered dimension.
        \param [in] outerSize - a number of dimensions in \a outer.
        \param [in] srcCount - a length of the gathered dimension in the input tensor.
        \param [in] inner - a product of dimensions after the gathered dimension.
        \param [in] idxCount - a length of the gathered dimension in the index and output tensors.
        \return a pointer to gather elements context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in functions ::SimdSynetGatherElementsSetIndex, ::SimdSynetGatherElementsInternalBufferSize and ::SimdSynetGatherElementsForward.
    */
    SIMD_API void* SimdSynetGatherElementsInit(SimdTensorDataType dataType, SimdTensorDataType indexType, 
        SimdBool indexConst, size_t indexUsers, const size_t * outer, size_t outerSize, size_t srcCount, size_t inner, size_t idxCount);

    /*! @ingroup synet_gather_elements

        \fn void SimdSynetGatherElementsSetIndex(const void* context, const uint8_t* idx);

        \short Sets and analyzes constant gather-elements indexes.

        The function has an effect only when the context was created with \a indexConst equal to SimdTrue. It detects
        whether negative-index correction is needed and may collapse repeated outer batches of identical indexes.
        The current implementation still expects the index pointer to be passed to ::SimdSynetGatherElementsForward.

        \param [in] context - a pointer to gather elements context. It must be created by function ::SimdSynetGatherElementsInit and released by function ::SimdRelease.
        \param [in] idx - a pointer to INT32 or INT64 index tensor. Its shape is outer[0] * ... * outer[outerSize - 1] * idxCount * inner.
    */
    SIMD_API void SimdSynetGatherElementsSetIndex(const void* context, const uint8_t* idx);

    /*! @ingroup synet_gather_elements

        \fn size_t SimdSynetGatherElementsInternalBufferSize(const void* context);

        \short Gets the size in bytes of internal storage used by a gather-elements context.

        The returned value reports implementation-specific buffers used by the context.

        \param [in] context - a pointer to gather elements context. It must be created by function ::SimdSynetGatherElementsInit and released by function ::SimdRelease.
        \return size of internal buffer in bytes used inside gather elements algorithm.
    */
    SIMD_API size_t SimdSynetGatherElementsInternalBufferSize(const void* context);

    /*! @ingroup synet_gather_elements

        \fn void SimdSynetGatherElementsForward(void* context, const uint8_t* src, const uint8_t* idx, uint8_t* dst);

        \short Performs gather-elements forward propagation.

        The function gathers elements from \a src according to \a idx. If ::SimdSynetGatherElementsSetIndex was called,
        the context can use the analysis results, but \a idx must still point to the index tensor in the current
        implementation. Negative indexes are interpreted relative to \a srcCount.

        \param [in] context - a pointer to gather elements context. It must be created by function ::SimdSynetGatherElementsInit and released by function ::SimdRelease.
        \param [in] src - a pointer to input tensor. Its shape is outer[0] * ... * outer[outerSize - 1] * srcCount * inner.
        \param [in] idx - a pointer to INT32 or INT64 index tensor. Its shape is outer[0] * ... * outer[outerSize - 1] * idxCount * inner.
        \param [out] dst - a pointer to output tensor. Its shape is outer[0] * ... * outer[outerSize - 1] * idxCount * inner.
    */
    SIMD_API void SimdSynetGatherElementsForward(void* context, const uint8_t* src, const uint8_t* idx, uint8_t* dst);

    /*! @ingroup synet_activation

        \fn void SimdSynetGelu32f(const float* src, size_t size, float* dst);

        \short Calculates exact GELU activation for an FP32 array.

        Algorithm's details:
        \verbatim
        for(i = 0; i < size; ++i)
            dst[i] = src[i] * (1 + erf(src[i]/sqrt(2))) / 2;
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 array.
        \param [in] size - a number of elements in the input and output arrays.
        \param [out] dst - a pointer to the output FP32 array.
    */
    SIMD_API void SimdSynetGelu32f(const float* src, size_t size, float* dst);

    /*! @ingroup synet_grid_sample

        \fn void* SimdSynetGridSample2dInit(size_t batch, size_t channels, size_t srcH, size_t srcW, size_t dstH, size_t dstW, SimdTensorDataType type, SimdGridSampleInterpType interp, SimdGridSamplePaddingType padding, SimdBool align);

        \short Initializes an ONNX-compatible GridSample-2D context.

        The function creates a context for NCHW tensors. The input tensor shape is batch*channels*srcH*srcW, the
        grid tensor shape is batch*dstH*dstW*2 and the output tensor shape is batch*channels*dstH*dstW. The grid
        stores normalized coordinates in the range [-1, 1] as pairs (x, y). The current implementation supports FP32
        source, grid and destination tensors.

        \param [in] batch - a batch size.
        \param [in] channels - a number of channels in the input and output tensors.
        \param [in] srcH - a height of input tensor.
        \param [in] srcW - a width of input tensor.
        \param [in] dstH - a height of output tensor.
        \param [in] dstW - a width of output tensor.
        \param [in] type - a type of input, grid and output tensor. Currently only FP32 is supported.
        \param [in] interp - an interpolation type: bilinear, nearest or bicubic.
        \param [in] padding - a padding type for out-of-bound grid coordinates.
        \param [in] align - a flag corresponding to ONNX/PyTorch align_corners.
        \return a pointer to grid sample 2D context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in functions ::SimdSynetGridSample2dInternalBufferSize, and ::SimdSynetGridSample2dForward.
    */
    SIMD_API void* SimdSynetGridSample2dInit(size_t batch, size_t channels, size_t srcH, size_t srcW, size_t dstH, size_t dstW, 
        SimdTensorDataType type, SimdGridSampleInterpType interp, SimdGridSamplePaddingType padding, SimdBool align);

    /*! @ingroup synet_grid_sample

        \fn size_t SimdSynetGridSample2dInternalBufferSize(const void* context);

        \short Gets the size in bytes of internal storage used by a GridSample-2D context.

        The optimized bilinear/zero-padding FP32 implementation stores padded source rows, precomputed source indexes
        and interpolation coefficients internally. The reference implementation returns 0.

        \param [in] context - a pointer to grid sample 2D context. It must be created by function ::SimdSynetGridSample2dInit and released by function ::SimdRelease.
        \return a number of bytes used by internal buffers.
    */
    SIMD_API size_t SimdSynetGridSample2dInternalBufferSize(const void* context);

    /*! @ingroup synet_grid_sample

        \fn void SimdSynetGridSample2dForward(void* context, const uint8_t* src, const uint8_t* grd, uint8_t* dst);

        \short Performs GridSample-2D forward propagation.

        For every output pixel the function denormalizes grid coordinates to input image coordinates, applies the
        selected padding rule for out-of-bound coordinates and samples the input with nearest, bilinear or bicubic
        interpolation:
        \verbatim
        if(align)
            x = (gridX + 1) * (srcW - 1) / 2;
        else
            x = ((gridX + 1) * srcW - 1) / 2;
        y is computed in the same way from gridY and srcH.
        dst[b, c, dy, dx] = Interpolate(src[b, c], x, y, interp, padding);
        \endverbatim

        \param [in] context - a pointer to grid sample 2D context. It must be created by function ::SimdSynetGridSample2dInit and released by function ::SimdRelease.
        \param [in] src - a pointer to input tensor. Its shape is batch * channels * srcH * srcW.
        \param [in] grd - a pointer to grid tensor. Its shape is batch * dstH * dstW * 2.
        \param [out] dst - a pointer to output tensor. Its shape is batch * channels * dstH * dstW.
    */
    SIMD_API void SimdSynetGridSample2dForward(void* context, const uint8_t* src, const uint8_t* grd, uint8_t* dst);

    /*! @ingroup synet_activation

        \fn void SimdSynetHardSigmoid32f(const float * src, size_t size, const float * scale, const float * shift, float * dst);

        \short Calculates HardSigmoid activation for an FP32 array.

        The input and output arrays must have the same size.

        Algorithm's details:
        \verbatim
        for(i = 0; i < size; ++i)
            dst[i] = Max(0, Min(src[i] * scale[0] + shift[0], 1));
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 array.
        \param [in] size - a number of elements in the input and output arrays.
        \param [in] scale - a pointer to scale parameter. Only scale[0] is used. This parameter is equal to 1/6 in PyTorch documentation.
        \param [in] shift - a pointer to shift parameter. Only shift[0] is used. This parameter is equal to 1/2 in PyTorch documentation.
        \param [out] dst - a pointer to the output FP32 array.
    */
    SIMD_API void SimdSynetHardSigmoid32f(const float * src, size_t size, const float * scale, const float * shift, float * dst);

    /*! @ingroup synet_activation

        \fn void SimdSynetHswish32f(const float * src, size_t size, const float * shift, const float * scale, float * dst);

        \short Calculates H-Swish activation for an FP32 array.

        The input and output arrays must have the same size.

        Algorithm's details:
        \verbatim
        for(i = 0; i < size; ++i)
            dst[i] = Max(Min(src[i], shift[0]) + shift[0], 0)*scale[0]*src[i];
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 array.
        \param [in] size - a number of elements in the input and output arrays.
        \param [in] shift - a pointer to shift parameter. Only shift[0] is used. It is equal to 3 in the original paper.
        \param [in] scale - a pointer to scale parameter. Only scale[0] is used. It is equal to 1/6 in the original paper.
        \param [out] dst - a pointer to the output FP32 array.
    */
    SIMD_API void SimdSynetHswish32f(const float* src, size_t size, const float* shift, const float* scale, float* dst);

    /*! @ingroup synet_inner_product

        \fn void * SimdSynetInnerProduct32fInit(size_t M, size_t N, size_t K, SimdBool transB, SimdBool constB, SimdBool bias, SimdConvolutionActivationType activation);

        \short Initializes an FP32 inner-product (matrix multiplication) context.

        The context computes C = A*B, optionally adds bias and applies activation:
        \verbatim
        for(i = 0; i < M; ++i)
            for(j = 0; j < N; ++j)
            {
                sum = bias ? bias[j] : 0;
                for(k = 0; k < K; ++k)
                    sum += A[i, k] * (transB ? B[j, k] : B[k, j]);
                C[i, j] = Activate(sum, activation, params);
            }
        \endverbatim

        When \a constB is SimdTrue, matrix B must be supplied to ::SimdSynetInnerProduct32fSetParams and can be
        reordered or cached inside the context.

        \param [in] M - a height of A and C matrices.
        \param [in] N - a width of B and C matrices.
        \param [in] K - a width of A and height of B matrices.
        \param [in] transB - a flag indicating that B is stored as N*K instead of K*N.
        \param [in] constB - a flag indicating that matrix B is constant and can be set once.
        \param [in] bias - a flag to add bias to output matrix C.
        \param [in] activation - an activation function type used after inner product.
        \return a pointer to FP32 inner product context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in functions ::SimdSynetInnerProduct32fInternalBufferSize, ::SimdSynetInnerProduct32fExternalBufferSize, 
            ::SimdSynetInnerProduct32fSetParams and ::SimdSynetInnerProduct32fForward.
    */
    SIMD_API void* SimdSynetInnerProduct32fInit(size_t M, size_t N, size_t K, SimdBool transB, SimdBool constB, SimdBool bias, SimdConvolutionActivationType activation);

    /*! @ingroup synet_inner_product

        \fn size_t SimdSynetInnerProduct32fInternalBufferSize(const void * context);

        \short Gets the size of internal storage used by an FP32 inner-product context.

        The returned value is a number of FP32 elements. It reports implementation-specific storage such as reordered
        constant weights and copied bias.

        \param [in] context - a pointer to FP32 inner product context. It must be created by function ::SimdSynetInnerProduct32fInit and released by function ::SimdRelease.
        \return a number of FP32 elements used by internal buffers.
    */
    SIMD_API size_t SimdSynetInnerProduct32fInternalBufferSize(const void* context);

    /*! @ingroup synet_inner_product

        \fn size_t SimdSynetInnerProduct32fExternalBufferSize(const void * context);

        \short Gets the size of caller-provided temporary buffer for FP32 inner product.

        The returned value is a number of FP32 elements. The current FP32 implementations do not require an external
        buffer and return 0, but callers can use this value when allocating the \a buf argument of
        ::SimdSynetInnerProduct32fForward.

        \param [in] context - a pointer to FP32 inner product context. It must be created by function ::SimdSynetInnerProduct32fInit and released by function ::SimdRelease.
        \return a number of FP32 elements required for external temporary buffer.
    */
    SIMD_API size_t SimdSynetInnerProduct32fExternalBufferSize(const void* context);

    /*! @ingroup synet_inner_product

        \fn void SimdSynetInnerProduct32fSetParams(void* context, const float* weight, SimdBool* internal, const float* bias, const float* params);

        \short Sets weights, bias and activation parameters for FP32 inner product.

        This function must be called before ::SimdSynetInnerProduct32fForward. If \a constB was SimdTrue during
        initialization, \a weight provides matrix B and the implementation may reorder and store it internally. If
        \a internal is not NULL, SimdTrue means the weights were copied/reordered into the context; SimdFalse means
        the original \a weight pointer can be used by later forward calls and must remain valid. Bias and activation
        parameters are stored or referenced according to the selected implementation.

        \param [in, out] context - a pointer to FP32 inner product context. It must be created by function ::SimdSynetInnerProduct32fInit and released by function ::SimdRelease.
        \param [in] weight - a pointer to FP32 matrix B weights.
        \param [out] internal - a pointer to a flag receiving weight storage mode. Can be NULL.
        \param [in] bias - a pointer to FP32 bias array with N elements. Can be NULL.
        \param [in] params - a pointer to FP32 parameters of activation function (see ::SimdConvolutionActivationType). Can be NULL when activation does not require parameters.
    */
    SIMD_API void SimdSynetInnerProduct32fSetParams(void* context, const float* weight, SimdBool* internal, const float* bias, const float* params);

    /*! @ingroup synet_inner_product

        \fn void SimdSynetInnerProduct32fForward(void* context, const float* A, const float* B, float *buf, float* C);

        \short Performs FP32 inner-product forward propagation.

        \param [in] context - a pointer to FP32 inner product context. 
            It must be created by function ::SimdSynetInnerProduct32fInit and released by function ::SimdRelease.
        \param [in] A - a pointer to FP32 A matrix with M*K elements.
        \param [in] B - a pointer to FP32 B matrix. Can be NULL if B is constant; in that case B must be set by function ::SimdSynetInnerProduct32fSetParams.
        \param [out] buf - a pointer to external temporary FP32 buffer. The required number of elements is determined by function ::SimdSynetInnerProduct32fExternalBufferSize.
            Can be NULL (it causes usage of internal buffer).
        \param [out] C - a pointer to FP32 output matrix with M*N elements.
    */
    SIMD_API void SimdSynetInnerProduct32fForward(void* context, const float* A, const float* B, float *buf, float* C);

    /*! @ingroup synet_inner_product

        \fn void SimdSynetInnerProductLayerForward(const float * src, const float * weight, const float * bias, size_t count, size_t size, float * dst);

        \short Performs FP32 forward propagation of a single inner-product layer.

        Algorithm's details:
        \verbatim
        for(i = 0; i < count; ++i)
        {
            dst[i] = (bias ? bias[i] : 0);
            for(j = 0; j < size; ++j)
               dst[i] += src[j]*weight[i*size + j];
        }
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 array with \a size elements.
        \param [in] weight - a pointer to FP32 weight coefficients with count*size elements, stored as count rows.
        \param [in] bias - a pointer to FP32 bias coefficients with \a count elements. Can be NULL.
        \param [in] count - a number of output elements.
        \param [in] size - a number of input elements.
        \param [out] dst - a pointer to the output FP32 array with \a count elements.
    */
    SIMD_API void SimdSynetInnerProductLayerForward(const float * src, const float * weight, const float * bias, size_t count, size_t size, float * dst);

    /*! @ingroup synet_inner_product_bf16

        \fn void* SimdSynetInnerProduct16bInit(size_t M, size_t N, size_t K, SimdTensorDataType typeA, SimdTensorDataType typeB, SimdTensorDataType typeC, SimdBool transB, SimdBool constB, SimdBool bias, SimdConvolutionActivationType activation);

        \short Initializes a BF16/FP32 inner-product (matrix multiplication) context.

        The context computes C = A*B with FP32 accumulation, optionally adds bias and applies activation. A, B and C
        can be FP32 or BF16 according to \a typeA, \a typeB and \a typeC:
        \verbatim
        for(i = 0; i < M; ++i)
            for(j = 0; j < N; ++j)
            {
                sum = bias ? bias[j] : 0;
                for(k = 0; k < K; ++k)
                    sum += A[i, k] * (transB ? B[j, k] : B[k, j]);
                C[i, j] = ConvertToTypeC(Activate(sum, activation, params));
            }
        \endverbatim

        When \a constB is SimdTrue, matrix B must be supplied to ::SimdSynetInnerProduct16bSetParams and is converted
        or reordered into internal storage.

        \param [in] M - a height of A and C matrices.
        \param [in] N - a width of B and C matrices.
        \param [in] K - a width of A and height of B matrices.
        \param [in] typeA - a type of A matrix. It can be FP32 or BF16.
        \param [in] typeB - a type of B matrix. It can be FP32 or BF16.
        \param [in] typeC - a type of C matrix. It can be FP32 or BF16.
        \param [in] transB - a flag indicating that B is stored as N*K instead of K*N.
        \param [in] constB - a flag indicating that matrix B is constant and can be set once.
        \param [in] bias - a flag to add bias to output matrix C.
        \param [in] activation - an activation function type used after inner product.
        \return a pointer to BF16 inner product context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in functions ::SimdSynetInnerProduct16bInternalBufferSize, ::SimdSynetInnerProduct16bExternalBufferSize, 
            ::SimdSynetInnerProduct16bInfo, ::SimdSynetInnerProduct16bSetParams and ::SimdSynetInnerProduct16bForward.
    */
    SIMD_API void* SimdSynetInnerProduct16bInit(size_t M, size_t N, size_t K, SimdTensorDataType typeA, SimdTensorDataType typeB, SimdTensorDataType typeC, SimdBool transB, SimdBool constB, SimdBool bias, SimdConvolutionActivationType activation);

    /*! @ingroup synet_inner_product_bf16

        \fn size_t SimdSynetInnerProduct16bInternalBufferSize(const void * context);

        \short Gets the size in bytes of internal storage used by a BF16 inner-product context.

        The returned value reports internal temporary storage, reordered constant weights, copied bias and copied
        activation parameters.

        \param [in] context - a pointer to BF16 inner product context. It must be created by function ::SimdSynetInnerProduct16bInit and released by function ::SimdRelease.
        \return a number of bytes used by internal buffers.
    */
    SIMD_API size_t SimdSynetInnerProduct16bInternalBufferSize(const void* context);

    /*! @ingroup synet_inner_product_bf16

        \fn size_t SimdSynetInnerProduct16bExternalBufferSize(const void * context);

        \short Gets the size in bytes of caller-provided temporary buffer for BF16 inner product.

        The returned value depends on matrix types and implementation. It covers temporary BF16 copies of FP32 inputs,
        packed non-constant B matrices, FP32 accumulation buffers and optional post-processing buffers. It can be used
        to allocate the \a buf argument of ::SimdSynetInnerProduct16bForward.

        \param [in] context - a pointer to BF16 inner product context. It must be created by function ::SimdSynetInnerProduct16bInit and released by function ::SimdRelease.
        \return a number of bytes required for external temporary buffer.
    */
    SIMD_API size_t SimdSynetInnerProduct16bExternalBufferSize(const void* context);

    /*! @ingroup synet_inner_product_bf16

        \fn const char* SimdSynetInnerProduct16bInfo(const void * context);

        \short Gets a short description of the selected BF16 inner-product implementation.

        The returned string contains the implementation extension, algorithm name and parameter summary. The returned
        pointer is owned by the context and remains valid until the next call of this function for the same context or
        until the context is released.

        \param [in] context - a pointer to BF16 inner product context. It must be created by function ::SimdSynetInnerProduct16bInit and released by function ::SimdRelease.
        \return a string with description of internal implementation of BF16 inner product algorithm.
    */
    SIMD_API const char* SimdSynetInnerProduct16bInfo(const void* context);

    /*! @ingroup synet_inner_product_bf16

        \fn void SimdSynetInnerProduct16bSetParams(void* context, const float* weight, const float* bias, const float* params);

        \short Sets weights, bias and activation parameters for BF16 inner product.

        This function must be called before ::SimdSynetInnerProduct16bForward. If \a constB was SimdTrue during
        initialization, \a weight provides matrix B in FP32 form and the implementation converts it to BF16 and may
        reorder it into internal storage. Bias is copied to an internal FP32 array; when \a bias is NULL, zeros are
        used. Activation parameters are copied or expanded to the internal FP32 array according to
        ::SimdConvolutionActivationType.

        \param [in, out] context - a pointer to BF16 inner product context. It must be created by function ::SimdSynetInnerProduct16bInit and released by function ::SimdRelease.
        \param [in] weight - a pointer to FP32 matrix B weights. Can be NULL only when B is not constant.
        \param [in] bias - a pointer to FP32 bias array with N elements. Can be NULL.
        \param [in] params - a pointer to FP32 parameters of activation function (see ::SimdConvolutionActivationType). Can be NULL when activation does not require parameters.
    */
    SIMD_API void SimdSynetInnerProduct16bSetParams(void* context, const float* weight, const float* bias, const float* params);

    /*! @ingroup synet_inner_product_bf16

        \fn void SimdSynetInnerProduct16bForward(void* context, const uint8_t* A, const uint8_t* B, uint8_t* buf, uint8_t* C);

        \short Performs BF16/FP32 inner-product forward propagation.

        The function converts FP32 A or B inputs to BF16 when requested by the context, uses BF16 inputs directly
        otherwise, accumulates the matrix product in FP32, adds bias, applies activation and writes FP32 or BF16
        output according to \a typeC.

        \param [in] context - a pointer to BF16 inner product context. 
            It must be created by function ::SimdSynetInnerProduct16bInit and released by function ::SimdRelease.
        \param [in] A - a pointer to A matrix. Actual element type is defined by \a typeA in initialization.
        \param [in] B - a pointer to B matrix. Can be NULL if B is constant; in that case B must be set by function ::SimdSynetInnerProduct16bSetParams.
            Actual element type is defined by \a typeB in initialization for non-constant B.
        \param [out] buf - a pointer to external temporary byte buffer.
            The required size is determined by function ::SimdSynetInnerProduct16bExternalBufferSize.
            Can be NULL (it causes usage of internal buffer).
        \param [out] C - a pointer to output matrix. Actual element type is defined by \a typeC in initialization.
    */
    SIMD_API void SimdSynetInnerProduct16bForward(void* context, const uint8_t* A, const uint8_t* B, uint8_t* buf, uint8_t* C);

    /*! @ingroup synet_inner_product

        \fn void SimdSynetInnerProduct8i(size_t M, size_t N, size_t K, const uint8_t * src, const int8_t * weight, int32_t * dst, SimdSynetCompatibilityType compatibility);

        \short Performs UINT8-by-INT8 inner product with INT32 output.

        Algorithm's details:
        \verbatim
        for (i = 0; i < M; ++i)
        {
            for (j = 0; j < N; ++j)
            {
                sum = 0;
                for (k = 0; k < K; ++k)
                    sum += int(src[i*K + k]) * int(weight[j*K + k]);
                dst[i*N + j] = sum;
            }
        }
        \endverbatim

        When compatibility flags allow overflow-compatible multiplication, adjacent products can be accumulated with
        16-bit saturation before being added to the INT32 sum. Use ::SimdSynetCompatibility8iPrecise to request the
        precise product accumulation path.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] M - a batch size, or a number of input rows.
        \param [in] N - an output size, or a number of weight rows.
        \param [in] K - an input size, or a row length.
        \param [in] src - a pointer to the UINT8 input matrix with M*K elements.
        \param [in] weight - a pointer to the INT8 weight matrix with N*K elements, stored by output row.
        \param [out] dst - a pointer to the INT32 output matrix with M*N elements.
        \param [in] compatibility - calculation compatibility flags (see ::SimdSynetCompatibilityType).
    */
    SIMD_API void SimdSynetInnerProduct8i(size_t M, size_t N, size_t K, const uint8_t * src, const int8_t * weight, int32_t * dst, SimdSynetCompatibilityType compatibility);

    /*! @ingroup synet_other

        \fn void SimdSynetLrnLayerCrossChannels(const float * src, size_t half, size_t channels, size_t spatial, const float * k, float * dst, SimdTensorFormatType format);

        \short Performs local response normalization across channels for a single FP32 tensor.

        For every tensor element the function accumulates squares of values from a channel window
        [c - half, c + half] clipped by tensor boundaries, and multiplies the source value by
        Pow(k[0] + k[1]*sum, k[2]). It supports ::SimdTensorFormatNchw and ::SimdTensorFormatNhwc.

        Algorithm's details (NCHW tensor format):
        \verbatim
        for(c = 0; c < channels; ++c)
            for(s = 0; s < spatial; ++s)
            {
                lo = Max(0, c - half);
                hi = Min(channels, c + half + 1);
                sum = 0;
                for(i = lo; i < hi; ++i)
                    sum += Square(src[i*spatial + s]);
                dst[c*spatial + s] = src[c*spatial + s]*Pow(k[0] + sum*k[1], k[2]);
            }
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the FP32 input tensor. The size of the array must be equal to channels*spatial.
        \param [in] half - a half size of the normalization channel window.
        \param [in] channels - a number of input and output tensor channels.
        \param [in] spatial - a spatial size (height*width) of input and output tensor.
        \param [in] k - a pointer to three FP32 coefficients: offset, scale and exponent.
        \param [out] dst - a pointer to the FP32 output tensor. The size of the array must be equal to channels*spatial.
        \param [in] format - a format of input and output tensor. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
    */
    SIMD_API void SimdSynetLrnLayerCrossChannels(const float * src, size_t half, size_t channels, size_t spatial, const float * k, float * dst, SimdTensorFormatType format);

    /*! @ingroup synet_merged_convolution_fp32

        \fn void * SimdSynetMergedConvolution32fInit(size_t batch, const SimdConvolutionParameters * convs, size_t count, SimdBool add);

        \short Initializes an FP32 merged convolution context.

        The context fuses a sequence of two or three NHWC convolutions into one forward call:
        convolution + depthwise convolution, depthwise convolution + convolution, or
        convolution + depthwise convolution + convolution. The first and last tensors must be FP32.
        Supported kernels are 1x1 or 3x3 for ordinary convolutions, 3x3, 5x5 or 7x7 for depthwise
        convolutions; dilation must be 1 and stride must be 1, 2 or 3. If add is ::SimdTrue for a
        three-convolution sequence, the source tensor is added to the final output and therefore must
        have the same shape as the final destination tensor.

        \param [in] batch - a batch size.
        \param [in] convs - an array with convolution parameters in execution order.
        \param [in] count - a number of merged convolutions. It must be 2 or 3.
        \param [in] add - a flag that enables adding the source tensor to the final output tensor.
        \return a pointer to FP32 merged convolution context. On error it returns NULL. It must be released with function ::SimdRelease.
            This pointer is used in functions ::SimdSynetMergedConvolution32fExternalBufferSize, ::SimdSynetMergedConvolution32fInternalBufferSize, 
            ::SimdSynetMergedConvolution32fInfo, ::SimdSynetMergedConvolution32fSetParams and ::SimdSynetMergedConvolution32fForward.
    */
    SIMD_API void * SimdSynetMergedConvolution32fInit(size_t batch, const SimdConvolutionParameters * convs, size_t count, SimdBool add);

    /*! @ingroup synet_merged_convolution_fp32

        \fn size_t SimdSynetMergedConvolution32fExternalBufferSize(const void * context);

        \short Gets the size of the optional external temporary buffer for FP32 merged convolution.

        \param [in] context - a pointer to FP32 merged convolution context. It must be created by function ::SimdSynetMergedConvolution32fInit and released by function ::SimdRelease.
        \return a number of FP32 elements required for the external temporary buffer passed to ::SimdSynetMergedConvolution32fForward.
    */
    SIMD_API size_t SimdSynetMergedConvolution32fExternalBufferSize(const void * context);

    /*! @ingroup synet_merged_convolution_fp32

        \fn size_t SimdSynetMergedConvolution32fInternalBufferSize(const void * context);

        \short Gets the size of internal storage used by an FP32 merged convolution context.

        \param [in] context - a pointer to FP32 merged convolution context. It must be created by function ::SimdSynetMergedConvolution32fInit and released by function ::SimdRelease.
        \return a number of FP32 elements stored inside the context (temporary buffer, reordered weights, biases and activation parameters).
    */
    SIMD_API size_t SimdSynetMergedConvolution32fInternalBufferSize(const void * context);

    /*! @ingroup synet_merged_convolution_fp32

        \fn const char* SimdSynetMergedConvolution32fInfo(const void* context);

        \short Gets a textual description of the selected FP32 merged convolution implementation.

        \param [in] context - a pointer to FP32 merged convolution context. It must be created by function ::SimdSynetMergedConvolution32fInit and released by function ::SimdRelease.
        \return a zero-terminated string with the selected implementation name.
    */
    SIMD_API const char* SimdSynetMergedConvolution32fInfo(const void* context);

    /*! @ingroup synet_merged_convolution_fp32

        \fn void SimdSynetMergedConvolution32fSetParams(void * context, const float * const * weight, SimdBool * internal, const float * const * bias, const float * const * params);

        \short Sets weights, biases and activation parameters for FP32 merged convolution.

        \param [in, out] context - a pointer to FP32 merged convolution context. It must be created by function ::SimdSynetMergedConvolution32fInit and released by function ::SimdRelease.
        \param [in] weight - an array of pointers to FP32 convolution weights. The array size must be equal to the number of merged convolutions.
        \param [out] internal - an array of flags set to ::SimdTrue when the corresponding weights were reordered and copied to the context, or ::SimdFalse when they are used directly. The array size must be equal to the number of merged convolutions. Can be NULL.
        \param [in] bias - an array of pointers to FP32 bias arrays, one per convolution. Each pointer can be NULL.
        \param [in] params - an array of pointers to activation parameters (see ::SimdConvolutionActivationType), one per convolution. Each pointer can be NULL for activations that do not use parameters.
    */
    SIMD_API void SimdSynetMergedConvolution32fSetParams(void * context, const float * const * weight, SimdBool * internal, const float * const * bias, const float * const * params);

    /*! @ingroup synet_merged_convolution_fp32

        \fn void SimdSynetMergedConvolution32fForward(void * context, const float * src, float * buf, float * dst);

        \short Performs forward propagation through the fused FP32 convolution sequence.

        \param [in] context - a pointer to FP32 merged convolution context. It must be created by function ::SimdSynetMergedConvolution32fInit and released by function ::SimdRelease.
        \param [in] src - a pointer to the FP32 input tensor with batch*convs[0].srcC*convs[0].srcH*convs[0].srcW elements.
        \param [out] buf - a pointer to an external temporary FP32 buffer. Its size is determined by function ::SimdSynetMergedConvolution32fExternalBufferSize. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to the FP32 output tensor with batch*convs[count - 1].dstC*convs[count - 1].dstH*convs[count - 1].dstW elements.
    */
    SIMD_API void SimdSynetMergedConvolution32fForward(void * context, const float * src, float * buf, float * dst);

    /*! @ingroup synet_merged_convolution_bf16

        \fn void * SimdSynetMergedConvolution16bInit(size_t batch, const SimdConvolutionParameters* convs, size_t count, SimdBool add);

        \short Initializes a merged convolution context that uses BF16 for internal convolution data.

        The context fuses a sequence of two or three NHWC convolutions into one forward call:
        convolution + depthwise convolution, depthwise convolution + convolution, or
        convolution + depthwise convolution + convolution. Source and destination tensors can be
        FP32 or BF16 according to the corresponding ::SimdConvolutionParameters fields. Ordinary
        convolutions use 1x1 or 3x3 kernels, depthwise convolutions use 3x3, 5x5 or 7x7 kernels;
        dilation must be 1 and stride must be 1, 2 or 3. If add is ::SimdTrue for a
        three-convolution sequence, the source tensor is added to the final output and therefore must
        have the same shape as the final destination tensor.

        \param [in] batch - a batch size.
        \param [in] convs - an array with convolution parameters in execution order.
        \param [in] count - a number of merged convolutions. It must be 2 or 3.
        \param [in] add - a flag that enables adding the source tensor to the final output tensor.
        \return a pointer to BF16 merged convolution context. On error it returns NULL. It must be released with function ::SimdRelease.
            This pointer is used in functions ::SimdSynetMergedConvolution16bExternalBufferSize, ::SimdSynetMergedConvolution16bInternalBufferSize, 
            ::SimdSynetMergedConvolution16bInfo, ::SimdSynetMergedConvolution16bSetParams and ::SimdSynetMergedConvolution16bForward.
    */
    SIMD_API void* SimdSynetMergedConvolution16bInit(size_t batch, const SimdConvolutionParameters* convs, size_t count, SimdBool add);

    /*! @ingroup synet_merged_convolution_bf16

        \fn size_t SimdSynetMergedConvolution16bExternalBufferSize(const void * context);

        \short Gets the size in bytes of the optional external temporary buffer for BF16 merged convolution.

        \param [in] context - a pointer to BF16 merged convolution context. It must be created by function ::SimdSynetMergedConvolution16bInit and released by function ::SimdRelease.
        \return size in bytes of the external temporary buffer passed to ::SimdSynetMergedConvolution16bForward.
    */
    SIMD_API size_t SimdSynetMergedConvolution16bExternalBufferSize(const void* context);

    /*! @ingroup synet_merged_convolution_bf16

        \fn size_t SimdSynetMergedConvolution16bInternalBufferSize(const void * context);

        \short Gets the size in bytes of internal storage used by a BF16 merged convolution context.

        \param [in] context - a pointer to BF16 merged convolution context. It must be created by function ::SimdSynetMergedConvolution16bInit and released by function ::SimdRelease.
        \return size in bytes of internal temporary storage, reordered weights, biases and activation parameters.
    */
    SIMD_API size_t SimdSynetMergedConvolution16bInternalBufferSize(const void* context);

    /*! @ingroup synet_merged_convolution_bf16

        \fn const char* SimdSynetMergedConvolution16bInfo(const void* context);

        \short Gets a textual description of the selected BF16 merged convolution implementation.

        \param [in] context - a pointer to BF16 merged convolution context. It must be created by function ::SimdSynetMergedConvolution16bInit and released by function ::SimdRelease.
        \return a zero-terminated string with the selected implementation name.
    */
    SIMD_API const char* SimdSynetMergedConvolution16bInfo(const void* context);

    /*! @ingroup synet_merged_convolution_bf16

        \fn void SimdSynetMergedConvolution16bSetParams(void* context, const float* const* weight, const float* const* bias, const float* const* params);

        \short Sets FP32 weights, biases and activation parameters for BF16 merged convolution.

        \param [in, out] context - a pointer to BF16 merged convolution context. It must be created by function ::SimdSynetMergedConvolution16bInit and released by function ::SimdRelease.
        \param [in] weight - an array of pointers to FP32 convolution weights. The array size must be equal to the number of merged convolutions.
        \param [in] bias - an array of pointers to FP32 bias arrays, one per convolution. Each pointer can be NULL.
        \param [in] params - an array of pointers to activation parameters (see ::SimdConvolutionActivationType), one per convolution. Each pointer can be NULL for activations that do not use parameters.
    */
    SIMD_API void SimdSynetMergedConvolution16bSetParams(void* context, const float* const* weight, const float* const* bias, const float* const* params);

    /*! @ingroup synet_merged_convolution_bf16

        \fn void SimdSynetMergedConvolution16bForward(void * context, const uint8_t* src, uint8_t* buf, uint8_t* dst);

        \short Performs forward propagation through the fused BF16 merged convolution sequence.

        \param [in] context - a pointer to BF16 merged convolution context. It must be created by function ::SimdSynetMergedConvolution16bInit and released by function ::SimdRelease.
        \param [in] src - a pointer to the input tensor bytes. The tensor type is determined by convs[0].srcT (FP32 or BF16).
        \param [out] buf - a pointer to an external temporary byte buffer. Its size in bytes is determined by function ::SimdSynetMergedConvolution16bExternalBufferSize. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to the output tensor bytes. The tensor type is determined by convs[count - 1].dstT (FP32 or BF16).
    */
    SIMD_API void SimdSynetMergedConvolution16bForward(void* context, const uint8_t* src, uint8_t* buf, uint8_t* dst);

    /*! @ingroup synet_merged_convolution_int8

        \fn void * SimdSynetMergedConvolution8iInit(size_t batch, const SimdConvolutionParameters* convs, size_t count, SimdSynetCompatibilityType compatibility);

        \short Initializes an INT8 merged convolution context.

        The context fuses a sequence of two or three NHWC convolutions into one forward call:
        convolution + depthwise convolution, depthwise convolution + convolution, or
        convolution + depthwise convolution + convolution. Source and destination tensors can be
        FP32 or UINT8 according to the corresponding ::SimdConvolutionParameters fields. Ordinary
        convolutions use 1x1 or 3x3 kernels, depthwise convolutions use 3x3, 5x5 or 7x7 kernels;
        kernels and strides must be square, dilation must be 1 and stride must be 1, 2 or 3.
        Ordinary convolution weights are quantized to INT8 by ::SimdSynetMergedConvolution8iSetParams.

        \param [in] batch - a batch size.
        \param [in] convs - an array with convolution parameters in execution order.
        \param [in] count - a number of merged convolutions. It must be 2 or 3.
        \param [in] compatibility - calculation compatibility flags (see ::SimdSynetCompatibilityType).
        \return a pointer to INT8 merged convolution context. On error it returns NULL. It must be released with function ::SimdRelease.
            This pointer is used in functions ::SimdSynetMergedConvolution8iExternalBufferSize, ::SimdSynetMergedConvolution8iInternalBufferSize, 
            ::SimdSynetMergedConvolution8iInfo, ::SimdSynetMergedConvolution8iSetParams and ::SimdSynetMergedConvolution8iForward.
    */
    SIMD_API void* SimdSynetMergedConvolution8iInit(size_t batch, const SimdConvolutionParameters* convs, size_t count, SimdSynetCompatibilityType compatibility);

    /*! @ingroup synet_merged_convolution_int8

        \fn size_t SimdSynetMergedConvolution8iExternalBufferSize(const void * context);

        \short Gets the size in bytes of the optional external temporary buffer for INT8 merged convolution.

        \param [in] context - a pointer to INT8 merged convolution context. It must be created by function ::SimdSynetMergedConvolution8iInit and released by function ::SimdRelease.
        \return size in bytes of the external temporary buffer passed to ::SimdSynetMergedConvolution8iForward.
    */
    SIMD_API size_t SimdSynetMergedConvolution8iExternalBufferSize(const void* context);

    /*! @ingroup synet_merged_convolution_int8

        \fn size_t SimdSynetMergedConvolution8iInternalBufferSize(const void * context);

        \short Gets the size in bytes of internal storage used by an INT8 merged convolution context.

        \param [in] context - a pointer to INT8 merged convolution context. It must be created by function ::SimdSynetMergedConvolution8iInit and released by function ::SimdRelease.
        \return size in bytes of internal temporary storage, quantized/reordered weights, conversion parameters, biases and activation parameters.
    */
    SIMD_API size_t SimdSynetMergedConvolution8iInternalBufferSize(const void* context);

    /*! @ingroup synet_merged_convolution_int8

        \fn const char* SimdSynetMergedConvolution8iInfo(const void* context);

        \short Gets a textual description of the selected INT8 merged convolution implementation.

        \param [in] context - a pointer to INT8 merged convolution context. It must be created by function ::SimdSynetMergedConvolution8iInit and released by function ::SimdRelease.
        \return a zero-terminated string with the selected implementation name.
    */
    SIMD_API const char* SimdSynetMergedConvolution8iInfo(const void* context);

    /*! @ingroup synet_merged_convolution_int8

        \fn void SimdSynetMergedConvolution8iSetParams(void* context, const float* const* weight, SimdBool* internal, const float* const* bias, const float* const* params, const float* const* stats);

        \short Sets FP32 weights, biases, activation parameters and quantization statistics for INT8 merged convolution.

        \param [in, out] context - a pointer to INT8 merged convolution context. It must be created by function ::SimdSynetMergedConvolution8iInit and released by function ::SimdRelease.
        \param [in] weight - an array of pointers to FP32 convolution weights. The array size must be equal to the number of merged convolutions.
        \param [out] internal - an array of flags set to ::SimdTrue when the corresponding weights are stored in the context after quantization/reordering. The array size must be equal to the number of merged convolutions. Can be NULL.
        \param [in] bias - an array of pointers to FP32 bias arrays, one per convolution. Each pointer can be NULL.
        \param [in] params - an array of pointers to activation parameters (see ::SimdConvolutionActivationType), one per convolution. Each pointer can be NULL for activations that do not use parameters.
        \param [in] stats - an array of six pointers to FP32 per-channel statistics: input min/max (stats[0], stats[1]), intermediate min/max before the last convolution (stats[2], stats[3]) and output min/max (stats[4], stats[5]).
    */
    SIMD_API void SimdSynetMergedConvolution8iSetParams(void* context, const float* const* weight, SimdBool* internal, const float* const* bias, const float* const* params, const float* const* stats);

    /*! @ingroup synet_merged_convolution_int8

        \fn void SimdSynetMergedConvolution8iForward(void * context, const uint8_t* src, uint8_t* buf, uint8_t* dst);

        \short Performs forward propagation through the fused INT8 merged convolution sequence.

        \param [in] context - a pointer to INT8 merged convolution context. It must be created by function ::SimdSynetMergedConvolution8iInit and released by function ::SimdRelease.
        \param [in] src - a pointer to the input tensor bytes. The tensor type is determined by convs[0].srcT (FP32 or UINT8).
        \param [out] buf - a pointer to an external temporary byte buffer. Its size in bytes is determined by function ::SimdSynetMergedConvolution8iExternalBufferSize. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to the output tensor bytes. The tensor type is determined by convs[count - 1].dstT (FP32 or UINT8).
    */
    SIMD_API void SimdSynetMergedConvolution8iForward(void* context, const uint8_t* src, uint8_t* buf, uint8_t* dst);

    /*! @ingroup synet_activation

        \fn void SimdSynetMish32f(const float* src, size_t size, const float* threshold, float* dst);

        \short Calculates Mish activation function (https://arxiv.org/abs/1908.08681) for an FP32 array.

        The function uses threshold[0] as an overflow guard: values greater than the threshold are
        copied to the destination because Mish(x) approaches x for large positive x.

        Algorithm's details:
        \verbatim
        for(i = 0; i < size; ++i)
            dst[i] = src[i] > threshold[0] ? src[i] : src[i] * Tanh(Log(Exp(src[i]) + 1));
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 array.
        \param [in] size - a size of input and output arrays.
        \param [in] threshold - a pointer to one FP32 threshold parameter.
        \param [out] dst - a pointer to the output FP32 array.
    */
    SIMD_API void SimdSynetMish32f(const float* src, size_t size, const float* threshold, float* dst);

    /*! @ingroup synet_normalize

        \fn void SimdSynetNormalizeLayerForward(const float* src, size_t batch, size_t channels, size_t spatial, const float* scale, const float* eps, SimdBool acrossSpatial, SimdTensorFormatType format, float* buf, float* dst);

        \short Performs FP32 L2 normalization with per-channel scale.

        If acrossSpatial is ::SimdTrue, each batch item is normalized by one norm computed across all
        channels and spatial positions. Otherwise each spatial position is normalized across channels.

        Algorithm's details (NHWC format, acrossSpatial is false):
        \verbatim
        for(b = 0; b < batch; ++b)
            for(s = 0; s < spatial; ++s)
            {
                sum = 0;
                for(c = 0; c < channels; ++c)
                    sum += Square(src[b, s, c]);
                for(c = 0; c < channels; ++c)
                    dst[b, s, c] = src[b, s, c] * scale[c] / Sqrt(sum + eps[0]);
            }
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 tensor.
        \param [in] batch - a batch size of input and output tensor.
        \param [in] channels - a number of channels in input and output tensor.
        \param [in] spatial - a spatial size (height*width) of input and output tensor.
        \param [in] scale - an array with per-channel scale parameters. The size of the array is equal to channels.
        \param [in] eps - a pointer to epsilon parameter. It is used to prevent division by zero.
        \param [in] acrossSpatial - a flag that selects normalization across channels*spatial for each batch item. Otherwise normalization is performed across channels for each spatial position.
        \param [in] format - a format of input and output tensor. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
        \param [out] buf - a pointer to external temporary FP32 buffer used for NCHW non-across-spatial mode. The size of the buffer must be equal to spatial. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to the output FP32 tensor.
    */
    SIMD_API void SimdSynetNormalizeLayerForward(const float* src, size_t batch, size_t channels, size_t spatial,
        const float* scale, const float* eps, SimdBool acrossSpatial, SimdTensorFormatType format, float* buf, float* dst);

    /*! @ingroup synet_normalize

        \fn void SimdSynetNormalizeLayerForwardV2(const float* src, size_t batch, size_t channels, size_t spatial, const float* scale, const float* shift, const float* eps, SimdTensorFormatType format, float* buf, float* dst);

        \short Performs FP32 layer normalization across channels with per-channel scale and shift.

        For every batch item and spatial position the function subtracts mean over channels, divides
        by standard deviation over channels and then applies per-channel scale and shift.

        Algorithm's details (NHWC tensor format):
        \verbatim
        for(b = 0; b < batch; ++b)
            for(s = 0; s < spatial; ++s)
            {
                sum = 0;
                for(c = 0; c < channels; ++c)
                    sum += src[b, s, c];
                mean = sum / channels;
                for(c = 0; c < channels; ++c)
                    dst[b, s, c] = src[b, s, c] - mean;

                sqsum = 0;
                for(c = 0; c < channels; ++c)
                    sqsum += Square(dst[b, s, c]);
                norm = 1 / Sqrt(sqsum / channels + eps[0]);
                for(c = 0; c < channels; ++c)
                    dst[b, s, c] = dst[b, s, c] * norm * scale[c] + shift[c];
            }
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 tensor.
        \param [in] batch - a batch size of input and output tensor.
        \param [in] channels - a number of channels in input and output tensor.
        \param [in] spatial - a spatial size (height*width) of input and output tensor.
        \param [in] scale - an array with per-channel scale parameters. The size of the array is equal to channels.
        \param [in] shift - an array with per-channel shift parameters. The size of the array is equal to channels.
        \param [in] eps - a pointer to epsilon parameter. It is used to prevent division by zero.
        \param [in] format - a format of input and output tensor. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
        \param [out] buf - a pointer to external temporary FP32 buffer used for NCHW layout. The size of the buffer must be equal to spatial. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to the output FP32 tensor.
    */
    SIMD_API void SimdSynetNormalizeLayerForwardV2(const float* src, size_t batch, size_t channels, size_t spatial, 
        const float* scale, const float* shift, const float* eps, SimdTensorFormatType format, float* buf, float* dst);

    /*! @ingroup synet_normalize

        \fn void SimdSynetNormalizeLayerForwardV3(const float* src, size_t batch, size_t channels, size_t spatial, const float* scale, const float* shift, const float* eps, SimdTensorFormatType format, float* buf, float* dst);

        \short Performs FP32 layer normalization across spatial positions with per-channel scale and shift.

        For every batch item and channel the function subtracts mean over spatial positions, divides
        by standard deviation over spatial positions and then applies the channel scale and shift.

        Algorithm's details (NCHW tensor format):
        \verbatim
        for(b = 0; b < batch; ++b)
            for(c = 0; c < channels; ++c)
            {
                sum = 0;
                for(s = 0; s < spatial; ++s)
                    sum += src[b, c, s];
                mean = sum / spatial;
                for(s = 0; s < spatial; ++s)
                    dst[b, c, s] = src[b, c, s] - mean;

                sqsum = 0;
                for(s = 0; s < spatial; ++s)
                    sqsum += Square(dst[b, c, s]);
                norm = 1 / Sqrt(sqsum / spatial + eps[0]);
                for(s = 0; s < spatial; ++s)
                    dst[b, c, s] = dst[b, c, s] * norm * scale[c] + shift[c];
            }
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 tensor.
        \param [in] batch - a batch size of input and output tensor.
        \param [in] channels - a number of channels in input and output tensor.
        \param [in] spatial - a spatial size (height*width) of input and output tensor.
        \param [in] scale - an array with per-channel scale parameters. The size of the array is equal to channels.
        \param [in] shift - an array with per-channel shift parameters. The size of the array is equal to channels.
        \param [in] eps - a pointer to epsilon parameter. It is used to prevent division by zero.
        \param [in] format - a format of input and output tensor. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
        \param [out] buf - a pointer to external temporary FP32 buffer used for NHWC layout. The size of the buffer must be equal to channels. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to the output FP32 tensor.
    */
    SIMD_API void SimdSynetNormalizeLayerForwardV3(const float* src, size_t batch, size_t channels, size_t spatial,
        const float* scale, const float* shift, const float* eps, SimdTensorFormatType format, float* buf, float* dst);

    /*! @ingroup synet_normalize

        \fn void SimdSynetNormalizeLayerForwardV4(const float* src, size_t batch, size_t channels, size_t spatial, const float* scale, const float* shift, const float* eps, SimdTensorFormatType format, float* buf, float* dst);

        \short Performs FP32 channel-norm based normalization with per-channel scale and shift.

        For every batch item the function computes an L2 norm for each channel, normalizes these
        norms by their channel average and uses the result as a per-channel multiplier.

        Algorithm's details (NCHW tensor format):
        \verbatim
        for(b = 0; b < batch; ++b)
        {
            sum = 0;
            for(c = 0; c < channels; ++c)
            {
                sqsum = 0;
                for(s = 0; s < spatial; ++s)
                    sqsum += Square(src[b, c, s]);
                buf[c] = sqrt(sqsum);
                sum += buf[c];
            }
            norm = 1 / (sum / channels + eps[0]);
            for(c = 0; c < channels; ++c)
            {
                buf[c] = 1 + scale[c] * buf[c] * norm;
                for(s = 0; s < spatial; ++s)
                    dst[b, c, s] = src[b, c, s] * buf[c] + shift[c];
            }
        }
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 tensor.
        \param [in] batch - a batch size of input and output tensor.
        \param [in] channels - a number of channels in input and output tensor.
        \param [in] spatial - a spatial size (height*width) of input and output tensor.
        \param [in] scale - an array with per-channel scale parameters. The size of the array is equal to channels.
        \param [in] shift - an array with per-channel shift parameters. The size of the array is equal to channels.
        \param [in] eps - a pointer to epsilon parameter. It is used to prevent division by zero.
        \param [in] format - a format of input and output tensor. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
        \param [out] buf - a pointer to external temporary FP32 buffer. The size of the buffer must be equal to channels. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to the output FP32 tensor.
    */
    SIMD_API void SimdSynetNormalizeLayerForwardV4(const float* src, size_t batch, size_t channels, size_t spatial,
        const float* scale, const float* shift, const float* eps, SimdTensorFormatType format, float* buf, float* dst);

    /*! @ingroup synet_normalize

        \fn void SimdSynetNormalizeLayerForward16bV2(const uint16_t* src, size_t batch, size_t channels, size_t spatial, const float* scale, const float* shift, const float* eps, SimdTensorFormatType format, float* buf, uint16_t* dst);

        \short Performs BF16 layer normalization across channels with per-channel scale and shift.

        This BF16 variant supports only ::SimdTensorFormatNhwc. Source values are converted to FP32
        for mean, variance, scale and shift calculation, and the result is converted back to BF16.

        Algorithm's details (NHWC tensor format):
        \verbatim
        for(b = 0; b < batch; ++b)
            for(s = 0; s < spatial; ++s)
            {
                for(c = 0; c < channels; ++c)
                    buf[c] = Bf16ToFp32(src[b, s, c]);

                sum = 0;
                for(c = 0; c < channels; ++c)
                    sum += buf[c];
                mean = sum / channels;
                for(c = 0; c < channels; ++c)
                    buf[c] = buf[c] - mean;

                sqsum = 0;
                for(c = 0; c < channels; ++c)
                    sqsum += Square(buf[c]);
                norm = 1 / Sqrt(sqsum / channels + eps[0]);
                for(c = 0; c < channels; ++c)
                    dst[b, s, c] = Fp32ToBf16(buf[c] * norm * scale[c] + shift[c]);
            }
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input BF16 tensor.
        \param [in] batch - a batch size of input and output tensor.
        \param [in] channels - a number of channels in input and output tensor.
        \param [in] spatial - a spatial size (height*width) of input and output tensor.
        \param [in] scale - an array with per-channel scale parameters. The size of the array is equal to channels.
        \param [in] shift - an array with per-channel shift parameters. The size of the array is equal to channels.
        \param [in] eps - a pointer to epsilon parameter. It is used to prevent division by zero.
        \param [in] format - a format of input and output tensor. It must be ::SimdTensorFormatNhwc.
        \param [out] buf - a pointer to external temporary FP32 buffer. The size of the buffer must be equal to channels. Can be NULL (it causes usage of internal buffer).
        \param [out] dst - a pointer to the output BF16 tensor.
    */
    SIMD_API void SimdSynetNormalizeLayerForward16bV2(const uint16_t* src, size_t batch, size_t channels, size_t spatial,
        const float* scale, const float* shift, const float* eps, SimdTensorFormatType format, float* buf, uint16_t* dst);

    /*! @ingroup synet_permute

        \fn void* SimdSynetPermuteInit(const size_t * shape, const size_t* order, size_t count, SimdTensorDataType type);

        \short Initializes a tensor permutation context.

        The context reorders tensor dimensions. If input shape is shape[0..count-1], then output
        dimension i has size shape[order[i]]. Supported dimension count is from 2 to 5. Dimensions
        with size 1 can be skipped by the implementation, but the requested permutation must change
        at least two non-unit dimensions. Supported tensor types are FP32, INT32, INT8, UINT8, BF16
        and FP16.

        \param [in] shape - a pointer to input tensor shape. The array size must be equal to count.
        \param [in] order - a pointer to output dimension order. The array size must be equal to count and contain a permutation of dimension indices.
        \param [in] count - a count of dimensions of input and output tensor.
        \param [in] type - an input and output tensor data type.
        \return a pointer to permute context. On error it returns NULL. It must be released with function ::SimdRelease.
            This pointer is used in functions ::SimdSynetPermuteInternalBufferSize, and ::SimdSynetPermuteForward.
    */
    SIMD_API void* SimdSynetPermuteInit(const size_t * shape, const size_t* order, size_t count, SimdTensorDataType type);

    /*! @ingroup synet_permute

        \fn size_t SimdSynetPermuteInternalBufferSize(const void* context);

        \short Gets the size in bytes of internal storage used by a permute context.

        \param [in] context - a pointer to permute context. It must be created by function ::SimdSynetPermuteInit and released by function ::SimdRelease.
        \return size in bytes of internal storage used by the permutation implementation.
    */
    SIMD_API size_t SimdSynetPermuteInternalBufferSize(const void* context);

    /*! @ingroup synet_permute

        \fn void SimdSynetPermuteForward(void* context, const uint8_t* src, uint8_t* dst);

        \short Performs tensor dimension permutation.

        \param [in] context - a pointer to permute context. It must be created by function ::SimdSynetPermuteInit and released by function ::SimdRelease.
        \param [in] src - a pointer to the input tensor bytes.
        \param [out] dst - a pointer to the output tensor bytes. Its shape is determined by the order parameter passed to ::SimdSynetPermuteInit.
    */
    SIMD_API void SimdSynetPermuteForward(void* context, const uint8_t* src, uint8_t* dst);

    /*! @ingroup synet_pooling

        \fn void SimdSynetPoolingAverage(const float * src, size_t srcC, size_t srcH, size_t srcW, size_t kernelY, size_t kernelX, size_t strideY, size_t strideX, size_t padY, size_t padX, float * dst, size_t dstH, size_t dstW, SimdBool excludePad, SimdTensorFormatType format);

        \short Performs 2D average pooling for an FP32 tensor.

        For every output position the pooling window starts at (dstY*strideY - padY,
        dstX*strideX - padX), is clipped by input boundaries and is averaged independently for every
        channel. If excludePad is ::SimdTrue, the divisor is the clipped window area; otherwise it is
        kernelY*kernelX. It supports ::SimdTensorFormatNchw and ::SimdTensorFormatNhwc.

        Algorithm's details:
        \verbatim
        for(c = 0; c < srcC; ++c)
            for(dy = 0; dy < dstH; ++dy)
                for(dx = 0; dx < dstW; ++dx)
                {
                    yBeg = Max(0, dy*strideY - padY);
                    yEnd = Min(srcH, dy*strideY - padY + kernelY);
                    xBeg = Max(0, dx*strideX - padX);
                    xEnd = Min(srcW, dx*strideX - padX + kernelX);
                    sum = 0;
                    for(sy = yBeg; sy < yEnd; ++sy)
                        for(sx = xBeg; sx < xEnd; ++sx)
                            sum += src[c, sy, sx];
                    dst[c, dy, dx] = sum / (excludePad ? (yEnd - yBeg)*(xEnd - xBeg) : kernelY*kernelX);
                }
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 tensor. The size of the array must be equal to srcC*srcH*srcW.
        \param [in] srcC - a number of input and output channels.
        \param [in] srcH - an input height.
        \param [in] srcW - an input width.
        \param [in] kernelY - a height of the pooling kernel.
        \param [in] kernelX - a width of the pooling kernel.
        \param [in] strideY - a y-stride of the pooling.
        \param [in] strideX - a x-stride of the pooling.
        \param [in] padY - a pad to the top of the input image.
        \param [in] padX - a pad to the left of the input image.
        \param [out] dst - a pointer to the output FP32 tensor. The size of the array must be equal to srcC*dstH*dstW.
        \param [in] dstH - an output height.
        \param [in] dstW - an output width.
        \param [in] excludePad - a flag that excludes padded positions from average value calculation.
        \param [in] format - a format of input and output tensor. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
    */
    SIMD_API void SimdSynetPoolingAverage(const float * src, size_t srcC, size_t srcH, size_t srcW, size_t kernelY, size_t kernelX,
        size_t strideY, size_t strideX, size_t padY, size_t padX, float * dst, size_t dstH, size_t dstW, SimdBool excludePad, SimdTensorFormatType format);

    /*! @ingroup synet_pooling

        \fn void SimdSynetPoolingMax32f(const float * src, size_t srcC, size_t srcH, size_t srcW, size_t kernelC, size_t kernelY, size_t kernelX, size_t strideC, size_t strideY, size_t strideX, size_t padC, size_t padY, size_t padX, float * dst, size_t dstC, size_t dstH, size_t dstW, SimdTensorFormatType format);

        \short Performs 2D or 3D max pooling for an FP32 tensor.

        If kernelC == 1, strideC == 1, padC == 0 and srcC == dstC, the function performs ordinary
        2D max pooling independently for every channel. Otherwise it also pools across channels,
        using kernelC, strideC, padC and dstC. Pooling windows are clipped by input boundaries. It
        supports ::SimdTensorFormatNchw and ::SimdTensorFormatNhwc.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 tensor. The size of the array must be equal to srcC*srcH*srcW.
        \param [in] srcC - a number of input channels.
        \param [in] srcH - an input height.
        \param [in] srcW - an input width.
        \param [in] kernelC - a channel size of the pooling kernel in 3D case. In 2D case it must be equal to 1.
        \param [in] kernelY - a height of the pooling kernel.
        \param [in] kernelX - a width of the pooling kernel.
        \param [in] strideC - a c-stride of the pooling in 3D case. In 2D case it must be equal to 1.
        \param [in] strideY - a y-stride of the pooling.
        \param [in] strideX - a x-stride of the pooling.
        \param [in] padC - a channel pad before the first input channel.
        \param [in] padY - a pad to the top of the input image.
        \param [in] padX - a pad to the left of the input image.
        \param [out] dst - a pointer to the output FP32 tensor. The size of the array must be equal to dstC*dstH*dstW.
        \param [in] dstC - a number of output channels.
        \param [in] dstH - an output height.
        \param [in] dstW - an output width.
        \param [in] format - a format of input and output tensor. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
    */
    SIMD_API void SimdSynetPoolingMax32f(const float * src, size_t srcC, size_t srcH, size_t srcW, 
        size_t kernelC, size_t kernelY, size_t kernelX, size_t strideC, size_t strideY, size_t strideX, 
        size_t padC, size_t padY, size_t padX, float * dst, size_t dstC, size_t dstH, size_t dstW, SimdTensorFormatType format);

    /*! @ingroup synet_pooling

        \fn void SimdSynetPoolingMax16b(const uint16_t* src, size_t srcC, size_t srcH, size_t srcW, size_t kernelY, size_t kernelX, size_t strideY, size_t strideX, size_t padY, size_t padX, uint16_t* dst, size_t dstH, size_t dstW, SimdTensorFormatType format);

        \short Performs 2D max pooling for a BF16 tensor.

        For every output position the pooling window is clipped by input boundaries and the maximum
        value is calculated independently for every channel. BF16 values are compared in FP32 domain
        and stored back as BF16. It supports ::SimdTensorFormatNchw and ::SimdTensorFormatNhwc.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input BF16 tensor. The size of the array must be equal to srcC*srcH*srcW.
        \param [in] srcC - a number of input and output channels.
        \param [in] srcH - an input height.
        \param [in] srcW - an input width.
        \param [in] kernelY - a height of the pooling kernel.
        \param [in] kernelX - a width of the pooling kernel.
        \param [in] strideY - a y-stride of the pooling.
        \param [in] strideX - a x-stride of the pooling.
        \param [in] padY - a pad to the top of the input image.
        \param [in] padX - a pad to the left of the input image.
        \param [out] dst - a pointer to the output BF16 tensor. The size of the array must be equal to srcC*dstH*dstW.
        \param [in] dstH - an output height.
        \param [in] dstW - an output width.
        \param [in] format - a format of input and output tensor. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
    */
    SIMD_API void SimdSynetPoolingMax16b(const uint16_t* src, size_t srcC, size_t srcH, size_t srcW, size_t kernelY, size_t kernelX, 
        size_t strideY, size_t strideX, size_t padY, size_t padX, uint16_t* dst, size_t dstH, size_t dstW, SimdTensorFormatType format);

    /*! @ingroup synet_pooling

        \fn void SimdSynetPoolingMax8u(const uint8_t * src, size_t srcC, size_t srcH, size_t srcW, size_t kernelY, size_t kernelX, size_t strideY, size_t strideX, size_t padY, size_t padX, uint8_t * dst, size_t dstH, size_t dstW, SimdTensorFormatType format);

        \short Performs 2D max pooling for a UINT8 tensor.

        For every output position the pooling window is clipped by input boundaries and the maximum
        value is calculated independently for every channel. It supports ::SimdTensorFormatNchw and
        ::SimdTensorFormatNhwc.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input UINT8 tensor. The size of the array must be equal to srcC*srcH*srcW.
        \param [in] srcC - a number of input and output channels.
        \param [in] srcH - an input height.
        \param [in] srcW - an input width.
        \param [in] kernelY - a height of the pooling kernel.
        \param [in] kernelX - a width of the pooling kernel.
        \param [in] strideY - a y-stride of the pooling.
        \param [in] strideX - a x-stride of the pooling.
        \param [in] padY - a pad to the top of the input image.
        \param [in] padX - a pad to the left of the input image.
        \param [out] dst - a pointer to the output UINT8 tensor. The size of the array must be equal to srcC*dstH*dstW.
        \param [in] dstH - an output height.
        \param [in] dstW - an output width.
        \param [in] format - a format of input and output tensor. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
    */
    SIMD_API void SimdSynetPoolingMax8u(const uint8_t* src, size_t srcC, size_t srcH, size_t srcW, size_t kernelY, size_t kernelX,
        size_t strideY, size_t strideX, size_t padY, size_t padX, uint8_t* dst, size_t dstH, size_t dstW, SimdTensorFormatType format);

    /*! @ingroup synet_activation

        \fn void SimdSynetPreluLayerForward(const float * src, const float * slope, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format);

        \short Performs PReLU activation with one slope per channel for an FP32 tensor.

        The function supports ::SimdTensorFormatNchw and ::SimdTensorFormatNhwc. For each element it
        keeps positive values unchanged and multiplies negative values by the slope of the
        corresponding channel.

        Algorithm's details (example for NCHW tensor format):
        \verbatim
        for(c = 0; c < channels; ++c)
            for(s = 0; s < spatial; ++s)
                dst[c*spatial + s] = src[c*spatial + s] > 0 ? src[c*spatial + s] : slope[c]*src[c*spatial + s];
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 tensor. The size of the array must be equal to channels*spatial.
        \param [in] slope - a pointer to per-channel FP32 slope coefficients. The size of the array must be equal to channels.
        \param [in] channels - a number of input and output tensor channels.
        \param [in] spatial - a spatial size (height*width) of input and output tensor.
        \param [out] dst - a pointer to the output FP32 tensor. The size of the array must be equal to channels*spatial.
        \param [in] format - a format of input and output tensor. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
    */
    SIMD_API void SimdSynetPreluLayerForward(const float * src, const float * slope, size_t channels, size_t spatial, float * dst, SimdTensorFormatType format);

    /*! @ingroup synet_quantized_add

        \fn void* SimdSynetQuantizedAddInit(const size_t* aShape, size_t aCount, SimdTensorDataType aType, const float* aScale, int32_t aZero, const size_t* bShape, size_t bCount, SimdTensorDataType bType, const float* bScale, int32_t bZero, SimdConvolutionActivationType actType, const float* actParams, SimdTensorDataType dstType, const float* dstScale, int32_t dstZero);

        \short Initializes element-wise quantized addition of two tensors with optional activation.

        The current implementation supports equal input shapes. For each element it dequantizes UINT8 inputs
        as (value - zero)*scale, adds the two values, applies activation if it is specified and converts the
        result to FP32 or UINT8 output. FP32 inputs and outputs ignore the corresponding quantization zero.

        \param [in] aShape - a pointer to shape of input A tensor.
        \param [in] aCount - a count of dimensions of input A tensor.
        \param [in] aType - a type of input A tensor. It can be ::SimdTensorData32f or ::SimdTensorData8u.
        \param [in] aScale - a pointer to quantization scale of input A tensor. Can be NULL (scale is 1.0).
        \param [in] aZero - a quantization zero of input A tensor.
        \param [in] bShape - a pointer to shape of input B tensor.
        \param [in] bCount - a count of dimensions of input B tensor.
        \param [in] bType - a type of input B tensor. It can be ::SimdTensorData32f or ::SimdTensorData8u.
        \param [in] bScale - a pointer to quantization scale of input B tensor. Can be NULL (scale is 1.0).
        \param [in] bZero - a quantization zero of input B tensor.
        \param [in] actType - an activation function type applied after addition. Supported optimized path uses ::SimdConvolutionActivationIdentity or ::SimdConvolutionActivationRelu.
        \param [in] actParams - a pointer to activation function parameters. Can be NULL.
        \param [in] dstType - a type of output tensor. It can be ::SimdTensorData32f or ::SimdTensorData8u.
        \param [in] dstScale - a pointer to output quantization scale. Can be NULL (scale is 1.0).
        \param [in] dstZero - an output quantization zero.
        \return a pointer to quantized addition context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in function ::SimdSynetQuantizedAddForward.
    */
    SIMD_API void* SimdSynetQuantizedAddInit(
        const size_t* aShape, size_t aCount, SimdTensorDataType aType, const float* aScale, int32_t aZero,
        const size_t* bShape, size_t bCount, SimdTensorDataType bType, const float* bScale, int32_t bZero, 
        SimdConvolutionActivationType actType, const float* actParams, SimdTensorDataType dstType, const float* dstScale, int32_t dstZero);

    /*! @ingroup synet_quantized_add

        \fn void SimdSynetQuantizedAddForward(void* context, const uint8_t* a, const uint8_t* b, uint8_t* dst);

        \short Performs element-wise quantized addition.

        Algorithm's details for UINT8 output:
        \verbatim
        for(i = 0; i < size; ++i)
        {
            value = Activate((a[i] - aZero)*aScale + (b[i] - bZero)*bScale, actType, actParams);
            dst[i] = RestrictRange(Round(value/dstScale) + dstZero, 0, 255);
        }
        \endverbatim

        \param [in] context - a pointer to quantized addition context. It must be created by function ::SimdSynetQuantizedAddInit and released by function ::SimdRelease.
        \param [in] a - a pointer to input A tensor data. Its type is defined by parameter aType of ::SimdSynetQuantizedAddInit.
        \param [in] b - a pointer to input B tensor data. Its type is defined by parameter bType of ::SimdSynetQuantizedAddInit.
        \param [out] dst - a pointer to output tensor data. Its type is defined by parameter dstType of ::SimdSynetQuantizedAddInit.
    */
    SIMD_API void SimdSynetQuantizedAddForward(void* context, const uint8_t* a, const uint8_t* b, uint8_t* dst);

    /*! @ingroup synet_quantized_other

        \fn void SimdSynetQuantizedConcatLayerForward(size_t count, const uint8_t** src, size_t num, const size_t* size, const int32_t* bias, const float* norm, const float* scale, int32_t zero, uint8_t* dst);

        \short Concatenates UINT8 tensors with requantization.

        Algorithm's details:
        \verbatim
        for(n = 0; n < num; ++n)
            for(s = 0, offset = 0; s < count; offset += size[s], ++s)
                for(i = 0; i < size[s]; ++i)
                    dst[offset + i] = RestrictRange(Round((src[s][n*size[s] + i] + bias[s])*norm[s]*scale[0]) + zero, 0, 255);
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] count - a number of input tensors.        
        \param [in] src - an array with pointers to UINT8 input tensors.
        \param [in] num - a number of concatenated blocks.
        \param [in] size - an array with sizes of concatenated parts for each input tensor.
        \param [in] bias - an array with dequantization biases of input tensors (usually -zero).
        \param [in] norm - an array with dequantization scales of input tensors.
        \param [in] scale - a pointer to output quantization norm (usually 1/scale).
        \param [in] zero - an output quantization zero.
        \param [out] dst - a pointer to the UINT8 output tensor.
    */
    SIMD_API void SimdSynetQuantizedConcatLayerForward(size_t count, const uint8_t** src, size_t num, const size_t* size, const int32_t* bias, const float* norm, const float* scale, int32_t zero, uint8_t* dst);


    /*! @ingroup synet_quantized_convolution

        \fn void * SimdSynetQuantizedConvolutionInit(size_t batch, const SimdConvolutionParameters* conv);

        \short Initializes UINT8-to-UINT8 quantized convolution algorithm.

        The convolution parameters have to describe a valid 2D convolution with equal source and destination
        tensor formats (::SimdTensorFormatNchw or ::SimdTensorFormatNhwc) and UINT8 source and destination
        tensors. The implementation uses signed 8-bit weights, per-output-channel weight scales, optional
        bias and optional activation from ::SimdConvolutionActivationType.

        \param [in] batch - a batch size.
        \param [in] conv - a pointer to convolution parameters (shape, kernel, stride, dilation, padding, group, tensor format, activation and data types).
        \return a pointer to Quantized convolution context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in functions ::SimdSynetQuantizedConvolutionExternalBufferSize, ::SimdSynetQuantizedConvolutionInternalBufferSize,
            ::SimdSynetQuantizedConvolutionInfo, ::SimdSynetQuantizedConvolutionSetParams and ::SimdSynetQuantizedConvolutionForward.
    */
    SIMD_API void* SimdSynetQuantizedConvolutionInit(size_t batch, const SimdConvolutionParameters* conv);

    /*! @ingroup synet_quantized_convolution

        \fn size_t SimdSynetQuantizedConvolutionExternalBufferSize(const void * context);

        \short Gets size in bytes of external temporary buffer required for quantized convolution.

        \param [in] context - a pointer to Quantized convolution context. It must be created by function ::SimdSynetQuantizedConvolutionInit and released by function ::SimdRelease.
        \return size in bytes of external temporary buffer required for quantized convolution. This value can be 0 or greater depending on selected implementation.
    */
    SIMD_API size_t SimdSynetQuantizedConvolutionExternalBufferSize(const void* context);

    /*! @ingroup synet_quantized_convolution

        \fn size_t SimdSynetQuantizedConvolutionInternalBufferSize(const void * context);

        \short Gets size in bytes of internal buffers allocated by quantized convolution context.

        \param [in] context - a pointer to Quantized convolution context. It must be created by function ::SimdSynetQuantizedConvolutionInit and released by function ::SimdRelease.
        \return size in bytes of internal buffers used to store reordered weights, biases, quantization parameters and an optional fallback temporary buffer.
    */
    SIMD_API size_t SimdSynetQuantizedConvolutionInternalBufferSize(const void* context);

    /*! @ingroup synet_quantized_convolution

        \fn const char* SimdSynetQuantizedConvolutionInfo(const void* context);

        \short Gets description of selected quantized convolution implementation.

        \param [in] context - a pointer to Quantized convolution context. It must be created by function ::SimdSynetQuantizedConvolutionInit and released by function ::SimdRelease.
        \return string with description of selected implementation (extension and algorithm name).
    */
    SIMD_API const char* SimdSynetQuantizedConvolutionInfo(const void* context);

    /*! @ingroup synet_quantized_convolution

        \fn void SimdSynetQuantizedConvolutionSetParams(void* context, const float * ioScale, const uint8_t* ioZero, const int8_t* weight, const float* weightScale, const int32_t* bias, const float* params);

        \short Sets quantization parameters, weights, bias and activation parameters for quantized convolution.

        Parameter ioScale contains source, intermediate and destination scales in this order. Parameter ioZero
        contains source, intermediate and destination zero points in the same order. The implementation folds
        source zero into bias and computes per-output-channel normalization as srcScale*weightScale[c]/dstScale
        for identity activation or srcScale*weightScale[c]/intScale for other activations.

        \param [in, out] context - a pointer to Quantized convolution context. It must be created by function ::SimdSynetQuantizedConvolutionInit and released by function ::SimdRelease.
        \param [in] ioScale - a pointer to 3 FP32 scales: input, intermediate and output.
        \param [in] ioZero - a pointer to 3 UINT8 zero points: input, intermediate and output.
        \param [in] weight - a pointer to INT8 convolution weights. Its layout is defined by convolution tensor format.
        \param [in] weightScale - a pointer to per-output-channel FP32 weight scales. The size of the array must be equal to conv->dstC.
        \param [in] bias - a pointer to per-output-channel INT32 bias. Can be NULL.
        \param [in] params - a pointer to FP32 activation parameters (see ::SimdConvolutionActivationType). Can be NULL.
    */
    SIMD_API void SimdSynetQuantizedConvolutionSetParams(void* context, const float * ioScale, const uint8_t* ioZero, const int8_t* weight, const float* weightScale, const int32_t* bias, const float* params);

    /*! @ingroup synet_quantized_convolution

        \fn void SimdSynetQuantizedConvolutionForward(void * context, const uint8_t * src, uint8_t * buf, uint8_t * dst);

        \short Performs forward propagation of quantized convolution.

        \param [in] context - a pointer to Quantized convolution context. It must be created by function ::SimdSynetQuantizedConvolutionInit and released by function ::SimdRelease.
        \param [in] src - a pointer to UINT8 input tensor with size batch*srcC*srcH*srcW.
        \param [out] buf - a pointer to external temporary buffer. Its size is determined by function ::SimdSynetQuantizedConvolutionExternalBufferSize. Can be NULL (then context uses an internal buffer).
        \param [out] dst - a pointer to UINT8 output tensor with size batch*dstC*dstH*dstW.
    */
    SIMD_API void SimdSynetQuantizedConvolutionForward(void* context, const uint8_t* src, uint8_t* buf, uint8_t* dst);

    /*! @ingroup synet_quantized_activation

        \fn void SimdSynetQuantizedHardSigmoid(const uint8_t* src, const float* srcScale, int srcZero, size_t size, const float* scale, const float* shift, uint8_t* dst, const float* dstScale, int dstZero);

        \short Performs forward propagation of UINT8 quantized HardSigmoid layer.

        Algorithm's details:
        \verbatim
        value = (src - srcZero)*srcScale[0];
        value = Max(Min(value, shift[0]) + shift[0], 0) * scale[0] * value;
        dst = RestrictRange(Round(value/dstScale[0]) + dstZero, 0, 255);
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to UINT8 input tensor.
        \param [in] srcScale - a pointer to quantization scale of input tensor.
        \param [in] srcZero - a quantization zero parameter of input tensor.
        \param [in] size - a size of (input/output) tensors.
        \param [in] scale - a pointer to scale parameter. Only scale[0] is used. It is equal to 1/6 in the original paper.
        \param [in] shift - a pointer to shift parameter. Only shift[0] is used. It is equal to 1/2 in the original paper.
        \param [out] dst - a pointer to UINT8 output tensor.
        \param [in] dstScale - a pointer to output quantization scale.
        \param [in] dstZero - an output quantization zero.
    */
    SIMD_API void SimdSynetQuantizedHardSigmoid(const uint8_t* src, const float* srcScale, int srcZero, size_t size, const float* scale, const float* shift, uint8_t* dst, const float* dstScale, int dstZero);

    /*! @ingroup synet_quantized_activation

        \fn void SimdSynetQuantizedHswish(const uint8_t* src, const float* srcScale, int srcZero, size_t size, const float* shift, const float* scale, uint8_t* dst, const float* dstScale, int dstZero);

        \short Performs forward propagation of UINT8 quantized Swish layer.

        Algorithm's details:
        \verbatim
        value = (src - srcZero)*srcScale[0];
        value = Max(0, Min(value * scale[0] + shift[0], 1));
        dst = RestrictRange(Round(value/dstScale[0]) + dstZero, 0, 255);
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to UINT8 input tensor.
        \param [in] srcScale - a pointer to quantization scale of input tensor.
        \param [in] srcZero - a quantization zero parameter of input tensor.
        \param [in] size - a size of (input/output) tensors.
        \param [in] shift - a pointer to shift parameter. Only shift[0] is used. It is equal to 3 in the original paper.
        \param [in] scale - a pointer to scale parameter. Only scale[0] is used. It is equal to 1/6 in the original paper.
        \param [out] dst - a pointer to UINT8 output tensor.
        \param [in] dstScale - a pointer to output quantization scale.
        \param [in] dstZero - an output quantization zero.
    */
    SIMD_API void SimdSynetQuantizedHswish(const uint8_t* src, const float* srcScale, int srcZero, size_t size, const float* shift, const float* scale, uint8_t* dst, const float* dstScale, int dstZero);

    /*! @ingroup synet_quantized_inner_product

        \fn void* SimdSynetQuantizedInnerProductInit(size_t M, size_t N, size_t K, SimdTensorDataType typeA, SimdTensorDataType typeB, SimdTensorDataType typeC, SimdBool transB, SimdBool constB, SimdBool bias);

        \short Initializes quantized inner product (matrix multiplication) algorithm for UINT8 input, INT8 weight and UINT8 output.

        The current implementation requires constB to be ::SimdTrue. Matrix B is supplied to
        ::SimdSynetQuantizedInnerProductSetParams and may be stored transposed according to transB.

        Algorithm's details before requantization (transB = false, bias = true):
        \verbatim
        for(i = 0; i < M; ++i)
            for(j = 0; j < N; ++j)
            {
                sum = bias[j] - aZero*Sum(B[:,j]);
                for(k = 0; k < K; ++k)
                    sum += A[i,k] * B[k,j];
                C[i,j] = RestrictRange(Round(sum*aScale*bScale[j]/cScale) + cZero, 0, 255);
            }
        \endverbatim

        \param [in] M - a height of A and height of C matrices.
        \param [in] N - a width of B and width of C matrices.
        \param [in] K - a width of A and height of B matrices.
        \param [in] typeA - a type of A matrix. Currently it must be ::SimdTensorData8u.
        \param [in] typeB - a type of B matrix. Currently it must be ::SimdTensorData8i.
        \param [in] typeC - a type of C matrix. Currently it must be ::SimdTensorData8u.
        \param [in] transB - a flag that matrix B is stored transposed (N*K instead of K*N).
        \param [in] constB - a flag that matrix B is constant. Currently it must be ::SimdTrue.
        \param [in] bias - a flag to add bias to output matrix C.
        \return a pointer to quantized inner product context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in functions ::SimdSynetQuantizedInnerProductInternalBufferSize, ::SimdSynetQuantizedInnerProductExternalBufferSize,
            ::SimdSynetQuantizedInnerProductInfo, ::SimdSynetQuantizedInnerProductSetParams and ::SimdSynetQuantizedInnerProductForward.
    */
    SIMD_API void* SimdSynetQuantizedInnerProductInit(size_t M, size_t N, size_t K, SimdTensorDataType typeA, SimdTensorDataType typeB, SimdTensorDataType typeC, SimdBool transB, SimdBool constB, SimdBool bias);

    /*! @ingroup synet_quantized_inner_product

        \fn size_t SimdSynetQuantizedInnerProductInternalBufferSize(const void * context);

        \short Gets size in bytes of internal buffers allocated by quantized inner product context.

        \param [in] context - a pointer to quantized inner product context. It must be created by function ::SimdSynetQuantizedInnerProductInit and released by function ::SimdRelease.
        \return size in bytes of internal buffers used to store constant B, bias, zero points, scales and an optional fallback temporary buffer.
    */
    SIMD_API size_t SimdSynetQuantizedInnerProductInternalBufferSize(const void* context);

    /*! @ingroup synet_quantized_inner_product

        \fn size_t SimdSynetQuantizedInnerProductExternalBufferSize(const void * context);

        \short Gets size in bytes of external temporary buffer required for quantized inner product.

        \param [in] context - a pointer to quantized inner product context. It must be created by function ::SimdSynetQuantizedInnerProductInit and released by function ::SimdRelease.
        \return size in bytes of external temporary buffer required by ::SimdSynetQuantizedInnerProductForward.
    */
    SIMD_API size_t SimdSynetQuantizedInnerProductExternalBufferSize(const void* context);

    /*! @ingroup synet_quantized_inner_product

        \fn const char* SimdSynetQuantizedInnerProductInfo(const void * context);

        \short Gets description of selected quantized inner product implementation.

        \param [in] context - a pointer to quantized inner product context. It must be created by function ::SimdSynetQuantizedInnerProductInit and released by function ::SimdRelease.
        \return string with description of selected implementation (extension and algorithm name).
    */
    SIMD_API const char* SimdSynetQuantizedInnerProductInfo(const void* context);

    /*! @ingroup synet_quantized_inner_product

        \fn void SimdSynetQuantizedInnerProductSetParams(void* context, const float* aScale, const uint8_t* aZero, const int8_t* b, const float* bScale, const int32_t* bias, const float* cScale, const uint8_t* cZero);

        \short Sets constant matrix B, bias and quantization parameters for quantized inner product.

        \param [in, out] context - a pointer to quantized inner product context. It must be created by function ::SimdSynetQuantizedInnerProductInit and released by function ::SimdRelease.
        \param [in] aScale - a pointer to FP32 quantization scale of A matrix.
        \param [in] aZero - a pointer to UINT8 quantization zero of A matrix.
        \param [in] b - a pointer to constant INT8 B matrix. It must be valid when constB is ::SimdTrue.
        \param [in] bScale - a pointer to per-output-channel FP32 scales of B matrix. The size of the array must be equal to N.
        \param [in] bias - a pointer to INT32 bias values. The size of the array must be equal to N. Can be NULL.
        \param [in] cScale - a pointer to FP32 quantization scale of C matrix.
        \param [in] cZero - a pointer to UINT8 quantization zero of C matrix.
    */
    SIMD_API void SimdSynetQuantizedInnerProductSetParams(void* context, const float* aScale, const uint8_t* aZero, const int8_t* b, const float* bScale, const int32_t* bias, const float* cScale, const uint8_t* cZero);

    /*! @ingroup synet_quantized_inner_product

        \fn void SimdSynetQuantizedInnerProductForward(void* context, const uint8_t* A, const uint8_t* B, uint8_t* buf, uint8_t* C);

        \short Performs forward propagation of quantized inner product.

        \param [in] context - a pointer to quantized inner product context. It must be created by function ::SimdSynetQuantizedInnerProductInit and released by function ::SimdRelease.
        \param [in] A - a pointer to UINT8 A matrix with size M*K.
        \param [in] B - a pointer to INT8 B matrix. Can be NULL when B was set by ::SimdSynetQuantizedInnerProductSetParams.
        \param [out] buf - a pointer to external buffer. The size of the external temporary buffer is determined by function ::SimdSynetQuantizedInnerProductExternalBufferSize.
            Can be NULL (it causes usage of internal buffer).
        \param [out] C - a pointer to UINT8 C matrix with size M*N.
    */
    SIMD_API void SimdSynetQuantizedInnerProductForward(void* context, const uint8_t* A, const uint8_t* B, uint8_t* buf, uint8_t* C);

    /*! @ingroup synet_quantized_merged_convolution

        \fn void * SimdSynetQuantizedMergedConvolutionInit(size_t batch, const SimdConvolutionParameters * convs, size_t count, int add);

        \short Initializes a chain of merged UINT8-to-UINT8 quantized convolutions.

        The merged chain contains 2 or 3 NHWC convolutions with UINT8 source and destination tensors, INT8
        weights and per-layer quantization parameters. Supported patterns are pointwise-depthwise,
        depthwise-pointwise and pointwise-depthwise-pointwise. If add is non-zero for a 3-convolution chain,
        the final output is requantized residual sum of the convolution output and the original input.

        \param [in] batch - a batch size.
        \param [in] convs - an array with convolution parameters. The array size must be equal to count.
        \param [in] count - a number of merged convolutions. It must be 2 or 3.
        \param [in] add - a residual addition mode: 0 disables addition, 1 adds output to source, 2 adds source to output.
        \return a pointer to Quantized merged convolution context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in functions ::SimdSynetQuantizedMergedConvolutionExternalBufferSize, ::SimdSynetQuantizedMergedConvolutionInternalBufferSize,
            ::SimdSynetQuantizedMergedConvolutionInfo, ::SimdSynetQuantizedMergedConvolutionSetParams and ::SimdSynetQuantizedMergedConvolutionForward.
    */
    SIMD_API void* SimdSynetQuantizedMergedConvolutionInit(size_t batch, const SimdConvolutionParameters* convs, size_t count, int add);

    /*! @ingroup synet_quantized_merged_convolution

        \fn size_t SimdSynetQuantizedMergedConvolutionExternalBufferSize(const void * context);

        \short Gets size in bytes of external temporary buffer required for quantized merged convolution.

        \param [in] context - a pointer to Quantized merged convolution context. It must be created by function ::SimdSynetQuantizedMergedConvolutionInit and released by function ::SimdRelease.
        \return size in bytes of external temporary buffer required by ::SimdSynetQuantizedMergedConvolutionForward.
    */
    SIMD_API size_t SimdSynetQuantizedMergedConvolutionExternalBufferSize(const void* context);

    /*! @ingroup synet_quantized_merged_convolution

        \fn size_t SimdSynetQuantizedMergedConvolutionInternalBufferSize(const void * context);

        \short Gets size in bytes of internal buffers allocated by quantized merged convolution context.

        \param [in] context - a pointer to Quantized merged convolution context. It must be created by function ::SimdSynetQuantizedMergedConvolutionInit and released by function ::SimdRelease.
        \return size in bytes of internal buffers used to store reordered weights, biases, norms, zero points and an optional fallback temporary buffer.
    */
    SIMD_API size_t SimdSynetQuantizedMergedConvolutionInternalBufferSize(const void* context);

    /*! @ingroup synet_quantized_merged_convolution

        \fn const char* SimdSynetQuantizedMergedConvolutionInfo(const void* context);

        \short Gets description of selected quantized merged convolution implementation.

        \param [in] context - a pointer to Quantized merged convolution context. It must be created by function ::SimdSynetQuantizedMergedConvolutionInit and released by function ::SimdRelease.
        \return string with description of selected implementation (extension and algorithm name).
    */
    SIMD_API const char* SimdSynetQuantizedMergedConvolutionInfo(const void* context);

    /*! @ingroup synet_quantized_merged_convolution

        \fn void SimdSynetQuantizedMergedConvolutionSetParams(void* context, const float* ioScale, const uint8_t* ioZero, const int8_t* const* weight, const float* const* weightScale, const int32_t* const* bias);

        \short Sets weights, biases and quantization parameters for quantized merged convolution.

        Arrays weight, weightScale and bias contain one pointer per merged convolution. The ioScale and ioZero
        arrays contain quantization parameters for every edge between convolutions: input, intermediate outputs
        and final output. When residual addition is enabled, one additional scale and zero point are used for
        the residual-sum output.

        \param [in, out] context - a pointer to Quantized merged convolution context. It must be created by function ::SimdSynetQuantizedMergedConvolutionInit and released by function ::SimdRelease.
        \param [in] ioScale - a pointer to FP32 input/intermediate/output tensor scales.
        \param [in] ioZero - a pointer to UINT8 input/intermediate/output tensor zero points.
        \param [in] weight - an array of pointers to INT8 convolution weights.
        \param [in] weightScale - an array of pointers to per-output-channel FP32 weight scales.
        \param [in] bias - an array of pointers to per-output-channel INT32 biases. Individual pointers can be NULL.
    */
    SIMD_API void SimdSynetQuantizedMergedConvolutionSetParams(void* context, const float* ioScale, const uint8_t* ioZero, const int8_t* const* weight, const float* const* weightScale, const int32_t* const* bias);

    /*! @ingroup synet_quantized_merged_convolution

        \fn void SimdSynetQuantizedMergedConvolutionForward(void* context, const uint8_t* src, uint8_t* buf, uint8_t* dst);

        \short Performs forward propagation of quantized merged convolution.

        \param [in] context - a pointer to Quantized merged convolution context. It must be created by function ::SimdSynetQuantizedMergedConvolutionInit and released by function ::SimdRelease.
        \param [in] src - a pointer to UINT8 input tensor of the first convolution.
        \param [out] buf - a pointer to external temporary buffer. Its size is determined by function ::SimdSynetQuantizedMergedConvolutionExternalBufferSize. Can be NULL (then context uses an internal buffer).
        \param [out] dst - a pointer to UINT8 output tensor of the last convolution or residual sum.
    */
    SIMD_API void SimdSynetQuantizedMergedConvolutionForward(void* context, const uint8_t* src, uint8_t* buf, uint8_t* dst);

    /*! @ingroup synet_quantized_mul

        \fn void* SimdSynetQuantizedMulInit(const size_t* aShape, size_t aCount, SimdTensorDataType aType, const float* aScale, int32_t aZero, const size_t* bShape, size_t bCount, SimdTensorDataType bType, const float* bScale, int32_t bZero, SimdConvolutionActivationType actType, const float* actParams, SimdTensorDataType dstType, const float* dstScale, int32_t dstZero);

        \short Initializes element-wise quantized multiplication of two tensors with optional activation.

        The current implementation supports equal input shapes. For each element it dequantizes UINT8 inputs
        as (value - zero)*scale, multiplicates the two values and converts the result to FP32 or UINT8 output. 
        FP32 inputs and outputs ignore the corresponding quantization zero.

        \param [in] aShape - a pointer to shape of input A tensor.
        \param [in] aCount - a count of dimensions of input A tensor.
        \param [in] aType - a type of input A tensor. It can be ::SimdTensorData32f or ::SimdTensorData8u.
        \param [in] aScale - a pointer to quantization scale of input A tensor. Can be NULL (scale is 1.0).
        \param [in] aZero - a quantization zero of input A tensor.
        \param [in] bShape - a pointer to shape of input B tensor.
        \param [in] bCount - a count of dimensions of input B tensor.
        \param [in] bType - a type of input B tensor. It can be ::SimdTensorData32f or ::SimdTensorData8u.
        \param [in] bScale - a pointer to quantization scale of input B tensor. Can be NULL (scale is 1.0).
        \param [in] bZero - a quantization zero of input B tensor.
        \param [in] dstType - a type of output tensor. It can be ::SimdTensorData32f or ::SimdTensorData8u.
        \param [in] dstScale - a pointer to output quantization scale. Can be NULL (scale is 1.0).
        \param [in] dstZero - an output quantization zero.
        \return a pointer to quantized multiplication context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in function ::SimdSynetQuantizedMulForward.
    */
    SIMD_API void* SimdSynetQuantizedMulInit(
        const size_t* aShape, size_t aCount, SimdTensorDataType aType, const float* aScale, int32_t aZero,
        const size_t* bShape, size_t bCount, SimdTensorDataType bType, const float* bScale, int32_t bZero,
        SimdTensorDataType dstType, const float* dstScale, int32_t dstZero);

    /*! @ingroup synet_quantized_mul

        \fn void SimdSynetQuantizedMulForward(void* context, const uint8_t* a, const uint8_t* b, uint8_t* dst);

        \short Performs element-wise quantized multiplication.

        Algorithm's details for UINT8 output:
        \verbatim
        for(i = 0; i < size; ++i)
        {
            _a = (a[i] - aZero)*aScale;
            _b = (b[i] - bZero)*bScale;
            dst[i] = RestrictRange(Round((_a * _b)/dstScale) + dstZero, 0, 255);
        }
        \endverbatim

        \param [in] context - a pointer to quantized multiplication context. It must be created by function ::SimdSynetQuantizedMulInit and released by function ::SimdRelease.
        \param [in] a - a pointer to input A tensor data. Its type is defined by parameter aType of ::SimdSynetQuantizedMulInit.
        \param [in] b - a pointer to input B tensor data. Its type is defined by parameter bType of ::SimdSynetQuantizedMulInit.
        \param [out] dst - a pointer to output tensor data. Its type is defined by parameter dstType of ::SimdSynetQuantizedMulInit.
    */
    SIMD_API void SimdSynetQuantizedMulForward(void* context, const uint8_t* a, const uint8_t* b, uint8_t* dst);

    /*! @ingroup synet_quantized_other

        \fn void SimdSynetQuantizedPoolingAverage(const uint8_t* src, const float* srcScale, int srcZero, size_t batch, size_t srcC, size_t srcH, size_t srcW,
                    size_t kernelY, size_t kernelX, size_t strideY, size_t strideX, size_t padY, size_t padX, SimdBool excludePad,
                    uint8_t* dst, const float* dstScale, int dstZero, size_t dstH, size_t dstW, SimdTensorFormatType format);

        \short Performs quantized 2D average pooling for an UINT8 tensor.

        For every output position the pooling window starts at (dstY*strideY - padY,
        dstX*strideX - padX), is clipped by input boundaries and is averaged independently for every
        channel. If excludePad is ::SimdTrue, the divisor is the clipped window area; otherwise it is
        kernelY*kernelX. It supports ::SimdTensorFormatNchw and ::SimdTensorFormatNhwc.

        Algorithm's details:
        \verbatim
        for(b = 0; b < batch; ++b)
            for(c = 0; c < srcC; ++c)
                for(dy = 0; dy < dstH; ++dy)
                    for(dx = 0; dx < dstW; ++dx)
                    {
                        yBeg = Max(0, dy*strideY - padY);
                        yEnd = Min(srcH, dy*strideY - padY + kernelY);
                        xBeg = Max(0, dx*strideX - padX);
                        xEnd = Min(srcW, dx*strideX - padX + kernelX);
                        sum = 0;
                        for(sy = yBeg; sy < yEnd; ++sy)
                            for(sx = xBeg; sx < xEnd; ++sx)
                                sum += (src[b, c, sy, sx] - srcZero)*srcScale[0];
                        val = sum / (excludePad ? (yEnd - yBeg)*(xEnd - xBeg) : kernelY*kernelX);
                        dst[b, c, dy, dx] = RestrictRange(Round(val/dstScale[0]) + dstZero, 0, 255);
                }
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input UINT8 tensor. The size of the array must be equal to srcC*srcH*srcW.
        \param [in] srcScale - a pointer to quantization scale of input tensor.
        \param [in] srcZero - a quantization zero parameter of input tensor.
        \param [in] batch - a batch size.
        \param [in] srcC - a number of input and output channels.
        \param [in] srcH - an input height.
        \param [in] srcW - an input width.
        \param [in] kernelY - a height of the pooling kernel.
        \param [in] kernelX - a width of the pooling kernel.
        \param [in] strideY - a y-stride of the pooling.
        \param [in] strideX - a x-stride of the pooling.
        \param [in] padY - a pad to the top of the input image.
        \param [in] padX - a pad to the left of the input image.
        \param [in] excludePad - a flag that excludes padded positions from average value calculation.
        \param [out] dst - a pointer to the output UINT8 tensor. The size of the array must be equal to srcC*dstH*dstW.
        \param [in] dstScale - a pointer to output quantization scale.
        \param [in] dstZero - an output quantization zero.
        \param [in] dstH - an output height.
        \param [in] dstW - an output width.
        \param [in] format - a format of input and output tensor. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
    */
    SIMD_API void SimdSynetQuantizedPoolingAverage(const uint8_t* src, const float* srcScale, int srcZero, size_t batch, size_t srcC, size_t srcH, size_t srcW,
        size_t kernelY, size_t kernelX, size_t strideY, size_t strideX, size_t padY, size_t padX, SimdBool excludePad,
        uint8_t* dst, const float* dstScale, int dstZero, size_t dstH, size_t dstW, SimdTensorFormatType format);

    /*! @ingroup synet_quantized_activation

        \fn void SimdSynetQuantizedPreluLayerForward(const uint8_t* src, const float* srcScale, int srcZero, size_t channels, size_t spatial, const float* slope, uint8_t* dst, const float* dstScale, int dstZero, SimdTensorFormatType format);

        \short Performs forward propagation of UINT8 quantized PReLU layer.

        Algorithm's details:
        \verbatim
        value = (src - srcZero)*srcScale[0];
        value = value > 0 ? value : slope[c]*value;
        dst = RestrictRange(Round(value/dstScale[0]) + dstZero, 0, 255);
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to UINT8 input tensor.
        \param [in] srcScale - a pointer to quantization scale of input tensor.
        \param [in] srcZero - a quantization zero parameter of input tensor.
        \param [in] channels - a number of channels in (input/output) tensors.
        \param [in] spatial - a spatial size of (input/output) tensors.
        \param [in] slope - a pointer to the 32-bit float array with slope coefficients. The size of the array is equal to channels.
        \param [out] dst - a pointer to UINT8 output tensor.
        \param [in] dstScale - a pointer to output quantization scale.
        \param [in] dstZero - an output quantization zero.
        \param [in] format - a format of input and output tensors. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
    */
    SIMD_API void SimdSynetQuantizedPreluLayerForward(const uint8_t* src, const float* srcScale, int srcZero, size_t channels, size_t spatial, const float* slope, uint8_t* dst, const float* dstScale, int dstZero, SimdTensorFormatType format);

    /*! @ingroup synet_quantized_other

        \fn void SimdSynetQuantizedScaleLayerForward(const uint8_t* src, const float* srcScale, int srcZero, size_t channels, size_t spatial, const float* scale, const float* bias, uint8_t* dst, const float* dstScale, int dstZero, SimdTensorFormatType format);

        \short Performs forward propagation of UINT8 quantized scale layer.

        Algorithm's details:
        \verbatim
        value = (src - srcZero)*srcScale[0];
        value = value*scale[c] + (bias ? bias[c] : 0);
        dst = RestrictRange(Round(value/dstScale[0]) + dstZero, 0, 255);
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to UINT8 input tensor.
        \param [in] srcScale - a pointer to quantization scale of input tensor.
        \param [in] srcZero - a quantization zero parameter of input tensor.
        \param [in] channels - a number of channels in (input/output) tensors.
        \param [in] spatial - a spatial size of (input/output) tensors.
        \param [in] scale - a pointer to the 32-bit float array with scale coefficients. The size of the array is equal to channels.
        \param [in] bias - a pointer to the 32-bit float array with bias coefficients. The size of the array is equal to channels. Can be NULL.
        \param [out] dst - a pointer to UINT8 output tensor.
        \param [in] dstScale - a pointer to output quantization scale.
        \param [in] dstZero - an output quantization zero.
        \param [in] format - a format of input and output tensors. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
    */
    SIMD_API void SimdSynetQuantizedScaleLayerForward(const uint8_t* src, const float* srcScale, int srcZero, size_t channels, size_t spatial, const float* scale, const float* bias, uint8_t* dst, const float* dstScale, int dstZero, SimdTensorFormatType format);

    /*! @ingroup synet_quantized_other

        \fn void SimdSynetQuantizedShuffleLayerForward(const uint8_t* src0, int bias0, const float* norm0, size_t srcC0, const uint8_t* src1, int bias1, const float* norm1, size_t srcC1, size_t spatial, uint8_t* dst0, uint8_t* dst1, const float* scale, int zero, SimdTensorFormatType format, int type);

        \short Performs forward propagation of UINT8 quantized shuffle layer.

        The function dequantizes channels from two input tensors, performs channel shuffle and requantizes
        results to two output tensors. For type 0 pairs of channels from src0 and src1 are split between
        dst0 and dst1. For type 1 channels from src0 and src1 are interleaved back into dst0 and dst1.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src0 - a pointer to UINT8 data of the first input tensor.
        \param [in] bias0 - a dequantization bias parameter of the first input tensor (-zero).
        \param [in] norm0 - a dequantization norm parameter of the first input tensor (scale).
        \param [in] srcC0 - a number of channels in the first input tensor.
        \param [in] src1 - a pointer to UINT8 data of the second input tensor.
        \param [in] bias1 - a dequantization bias parameter of the second input tensor (-zero).
        \param [in] norm1 - a dequantization norm parameter of the second input tensor (scale).
        \param [in] srcC1 - a number of channels in the second input tensor.
        \param [in] spatial - a spatial size of (input/output) tensors.
        \param [out] dst0 - a pointer to UINT8 data of the first output tensor.
        \param [out] dst1 - a pointer to UINT8 data of the second output tensor.
        \param [in] scale - an output quantization norm (1/scale).
        \param [in] zero - an output quantization zero.
        \param [in] format - a format of input and output tensors. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
        \param [in] type - a shuffle type: 0 for split operation, 1 for interleave operation.
        */
    SIMD_API void SimdSynetQuantizedShuffleLayerForward(const uint8_t* src0, int bias0, const float* norm0, size_t srcC0, const uint8_t* src1, int bias1, const float* norm1, size_t srcC1, size_t spatial, uint8_t* dst0, uint8_t* dst1, const float* scale, int zero, SimdTensorFormatType format, int type);

    /*! @ingroup synet_quantized_other

        \fn void SimdSynetQuantizeLinear(const float* src, size_t size, const float* norm, int32_t zero, uint8_t* dst);

        \short Performs FP32 to UINT8 linear quantization.

        Algorithm's details for ::SimdSynetQuantizeLinear:
        \verbatim
        for(i = 0; i < size; ++i)
            dst[i] = RestrictRange(Round(src[i]*norm[0]) + zero, 0, 255);
        \endverbatim

        \param [in] src - a pointer to FP32 input tensor.
        \param [in] size - a size of the input and output tensors.
        \param [in] norm - a pointer to quantization norm (usually 1/scale).
        \param [in] zero - a quantization zero.
        \param [out] dst - a pointer to UINT8 output tensor.
    */
    SIMD_API void SimdSynetQuantizeLinear(const float* src, size_t size, const float* norm, int32_t zero, uint8_t* dst);

    /*! @ingroup synet_activation

        \fn void SimdSynetRelu32f(const float* src, size_t size, const float* slope, float* dst);

        \short Calculates leaky ReLU function for FP32 array.

        Algorithm's details:
        \verbatim
        for(i = 0; i < size; ++i)
            dst[i] = Max(0, src[i]) + slope[0]*Min(0, src[i]);
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array.
        \param [in] size - a size of input and output arrays.
        \param [in] slope - a pointer to slope parameter for negative values.
        \param [out] dst - a pointer to output 32-bit float array.
    */
    SIMD_API void SimdSynetRelu32f(const float* src, size_t size, const float* slope, float* dst);

    /*! @ingroup synet_activation

        \fn void SimdSynetRelu16b(const uint16_t* src, size_t size, const float* slope, uint16_t* dst);

        \short Calculates leaky ReLU function for BF16 array.

        Algorithm's details:
        \verbatim
        for(i = 0; i < size; ++i)
        {
            value = BFloat16ToFloat32(src[i]);
            dst[i] = Float32ToBFloat16(Max(0, value) + slope[0]*Min(0, value));
        }
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 16-bit brain-float array.
        \param [in] size - a size of input and output arrays.
        \param [in] slope - a pointer to slope parameter for negative values.
        \param [out] dst - a pointer to output 16-bit brain-float array.
    */
    SIMD_API void SimdSynetRelu16b(const uint16_t* src, size_t size, const float* slope, uint16_t* dst);

    /*! @ingroup synet_activation

        \fn void SimdSynetRestrictRange32f(const float * src, size_t size, const float * lower, const float * upper, float * dst);

        \short Clamps FP32 array values to a given range.

        Algorithm's details:
        \verbatim
        for(i = 0; i < size; ++i)
            dst[i] = Min(Max(src[i], lower[0]), upper[0]);
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array.
        \param [in] size - a size of input and output arrays.
        \param [in] lower - a pointer to lower bound.
        \param [in] upper - a pointer to upper bound.
        \param [out] dst - a pointer to the output 32-bit float array.
    */
    SIMD_API void SimdSynetRestrictRange32f(const float * src, size_t size, const float * lower, const float * upper, float * dst);

    /*! @ingroup synet_scale

        \fn void* SimdSynetScale16bInit(size_t channels, size_t spatial, SimdTensorDataType srcType, SimdTensorDataType dstType, SimdTensorFormatType format, SimdBool norm, SimdBool bias);

        \short Initializes FP32/BF16 scale and bias algorithm.

        The context applies per-channel operation dst = src*norm + bias, dst = src*norm or dst = src + bias
        depending on norm and bias flags. Source and destination tensors can be FP32 or BF16.

        \param [in] channels - a number of channels in the (input/output) image tensor.
        \param [in] spatial - a spatial size (height*width) of (input/output) image tensor.
        \param [in] srcType - a type of input tensor. It can be ::SimdTensorData32f or ::SimdTensorData16b.
        \param [in] dstType - a type of output tensor. It can be ::SimdTensorData32f or ::SimdTensorData16b.
        \param [in] format - a format of input/output tensors. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
        \param [in] norm - a flag of presence of per-channel multiplication by norm.
        \param [in] bias - a flag of presence of per-channel addition of bias.
        \return a pointer to scale context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in function ::SimdSynetScale16bForward.
    */
    SIMD_API void* SimdSynetScale16bInit(size_t channels, size_t spatial, SimdTensorDataType srcType, SimdTensorDataType dstType, SimdTensorFormatType format, SimdBool norm, SimdBool bias);

    /*! @ingroup synet_scale

        \fn void SimdSynetScale16bForward(void* context, const uint8_t* src, const float *norm, const float * bias, uint8_t* dst);

        \short Performs forward propagation of FP32/BF16 scale and bias algorithm.

        Algorithm's details:
        \verbatim
        value = ConvertToFloat(src);
        if(norm) value *= norm[c];
        if(bias) value += bias[c];
        dst = ConvertFromFloat(value);
        \endverbatim

        \param [in] context - a pointer to scale context. It must be created by function ::SimdSynetScale16bInit and released by function ::SimdRelease.
        \param [in] src - a pointer to input tensor data. Its type is defined by parameter srcType of ::SimdSynetScale16bInit.
        \param [in] norm - a pointer to FP32 array with per-channel scale coefficients. Can be NULL if norm flag is ::SimdFalse.
        \param [in] bias - a pointer to FP32 array with per-channel bias coefficients. Can be NULL if bias flag is ::SimdFalse.
        \param [out] dst - a pointer to output tensor data. Its type is defined by parameter dstType of ::SimdSynetScale16bInit.
    */
    SIMD_API void SimdSynetScale16bForward(void* context, const uint8_t* src, const float *norm, const float * bias, uint8_t* dst);

    /*! @ingroup synet_scale

        \fn void SimdSynetScaleLayerForward(const float * src, const float * scale, const float * bias, size_t channels, size_t height, size_t width, float * dst, SimdTensorFormatType format, SimdSynetCompatibilityType compatibility);

        \short Performs forward propagation of FP32 ScaleLayer.

        Algorithm's details (example for NCHW tensor format):
        \verbatim
        for(c = 0; c < channels; ++c)
            for(h = 0; h < height; ++h)
                for(w = 0; w < width; ++w)
                    dst[(c*height + h)*width + w] = src[(c*height + h)*width + w]*scale[c] + (bias ? bias[c] : 0);
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the 32-bit float array with input image tensor. The size of the array is equal to channels*height*width.
        \param [in] scale - a pointer to the 32-bit float array with scale coefficients. The size of the array is equal to channels.
        \param [in] bias - a pointer to the 32-bit float array with bias coefficients. The size of the array is equal to channels. Can be NULL.
        \param [in] channels - a number of channels in the (input/output) image tensor.
        \param [in] height - a height of (input/output) image tensor.
        \param [in] width - a width of (input/output) image tensor.
        \param [out] dst - a pointer to the 32-bit float array with output image tensor. The size of the array is equal to channels*height*width.
        \param [in] format - a format of input and output image tensors. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
        \param [in] compatibility - reserved compatibility flags. Current implementation does not use this parameter.
    */
    SIMD_API void SimdSynetScaleLayerForward(const float * src, const float * scale, const float * bias, size_t channels, size_t height, size_t width, float * dst, SimdTensorFormatType format, SimdSynetCompatibilityType compatibility);

    /*! @ingroup synet_scale

        \fn void * SimdSynetScale8iInit(size_t batch, size_t channels, size_t spatial, SimdTensorDataType srcType, SimdTensorDataType dstType, SimdTensorFormatType format, SimdSynetCompatibilityType compatibility);

        \short Initializes FP32/UINT8 scale and bias algorithm.

        The context performs per-channel affine transformation between FP32 and UINT8 tensors. When UINT8 is
        used, conversion parameters are derived from statistics passed to ::SimdSynetScale8iSetParams.

        \param [in] batch - a batch size.
        \param [in] channels - a number of channels in input and output tensors.
        \param [in] spatial - a spatial size (height*width) of input and output tensors.
        \param [in] srcType - an input data type. It can be ::SimdTensorData32f or ::SimdTensorData8u.
        \param [in] dstType - an output data type. It can be ::SimdTensorData32f or ::SimdTensorData8u.
        \param [in] format - a format of input and output tensors. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
        \param [in] compatibility - a flags of calculation compatibility.
        \return a pointer to INT8 scale context. On error it returns NULL. It must be released with using of function ::SimdRelease.
            This pointer is used in functions ::SimdSynetScale8iInternalBufferSize, ::SimdSynetScale8iSetParams and ::SimdSynetScale8iForward.
    */
    SIMD_API void* SimdSynetScale8iInit(size_t batch, size_t channels, size_t spatial, SimdTensorDataType srcType, SimdTensorDataType dstType, SimdTensorFormatType format, SimdSynetCompatibilityType compatibility);

    /*! @ingroup synet_scale

        \fn size_t SimdSynetScale8iInternalBufferSize(const void * context);

        \short Gets size in bytes of internal buffers allocated by FP32/UINT8 scale context.

        \param [in] context - a pointer to INT8 scale context. It must be created by function ::SimdSynetScale8iInit and released by function ::SimdRelease.
        \return size in bytes of internal buffers used to store conversion parameters, scale and shift arrays.
    */
    SIMD_API size_t SimdSynetScale8iInternalBufferSize(const void* context);

    /*! @ingroup synet_scale

        \fn void SimdSynetScale8iSetParams(void * context, const float * scale, const float * bias, const float * const * stats);

        \short Sets per-channel scale, bias and tensor statistics for FP32/UINT8 scale algorithm.

        \param [in, out] context - a pointer to INT8 scale context. It must be created by function ::SimdSynetScale8iInit and released by function ::SimdRelease.
        \param [in] scale - a pointer to original FP32 per-channel scale coefficients.
        \param [in] bias - a pointer to original FP32 per-channel bias coefficients. Can be NULL.
        \param [in] stats - a pointer to pointers with input and output statistics: input min (stats[0]), input max (stats[1]), output min (stats[2]) and output max (stats[3]). Can be NULL for subsequent calls after statistics were initialized.
    */
    SIMD_API void SimdSynetScale8iSetParams(void* context, const float* scale, const float* bias, const float* const* stats);

    /*! @ingroup synet_scale

        \fn void SimdSynetScale8iForward(void * context, const uint8_t * src, uint8_t * dst);

        \short Performs forward propagation of FP32/UINT8 scale algorithm.

        Algorithm's details after ::SimdSynetScale8iSetParams prepares internal coefficients:
        \verbatim
        dst = Convert(src*internalScale[c] + internalShift[c]);
        \endverbatim

        \param [in] context - a pointer to INT8 scale context. It must be created by function ::SimdSynetScale8iInit and released by function ::SimdRelease.
        \param [in] src - a pointer to input tensor data. Its type is defined by parameter srcType of ::SimdSynetScale8iInit.
        \param [out] dst - a pointer to output tensor data. Its type is defined by parameter dstType of ::SimdSynetScale8iInit.
    */
    SIMD_API void SimdSynetScale8iForward(void* context, const uint8_t* src, uint8_t* dst);

    /*! @ingroup synet_conversion

        \fn void SimdSynetSetInput(const uint8_t * src, size_t width, size_t height, size_t stride, SimdPixelFormatType srcFormat, const float * lower, const float * upper, float * dst, size_t channels, SimdTensorFormatType dstFormat);

        \short Converts an 8-bit image to normalized FP32 neural-network input tensor.

        Algorithm's details (example for BGRA pixel format and NCHW tensor format):
        \verbatim
        for(c = 0; c < channels; ++c)
            for(y = 0; y < height; ++y)
                for(x = 0; x < width; ++x)
                    dst[(c*height + y)*width + x] = src[stride*y + x*4 + c]*(upper[c] - lower[c])/255 + lower[c];
        \endverbatim

        Each output value is mapped from [0, 255] to [lower[c], upper[c]]. Note that there are following relationships:
        \verbatim
        upper[c] = (1 - mean[c]) / std[c];
        lower[c] = - mean[c] / std[c];
        \endverbatim
        Also this algorithm assumes that channel order of output tensor is BGR. 
        In case of RGB channel order you need to change parameter srcFormat: ::SimdPixelFormatBgr24 <-> ::SimdPixelFormatRgb24, ::SimdPixelFormatBgra32 <-> ::SimdPixelFormatRgba32. 
        The actual pixel data of the input image does not need to be changed.
        
        \note This function has a C++ wrappers: Simd::SynetSetInput(const View<A> & src, const float * lower, const float * upper, float * dst, size_t channels, SimdTensorFormatType format, bool isRgb = false).

        \param [in] src - a pointer to pixels data of input image.
        \param [in] width - a width of input image and output image tensor.
        \param [in] height - a height of input image and output image tensor.
        \param [in] stride - a row size of input image.
        \param [in] srcFormat - a pixel format of input image. There are supported following pixel formats: ::SimdPixelFormatGray8, ::SimdPixelFormatBgr24, ::SimdPixelFormatBgra32, ::SimdPixelFormatRgb24, ::SimdPixelFormatRgba32.
        \param [in] lower - a pointer to lower bounds of output tensor values. The size of the array must be equal to channels.
        \param [in] upper - a pointer to upper bounds of output tensor values. The size of the array must be equal to channels.
        \param [out] dst - a pointer to the output 32-bit float image tensor.
        \param [in] channels - a number of channels in the output image tensor. It can be 1, 3 or 4.
        \param [in] dstFormat - a format of output image tensor. There are supported following tensor formats: ::SimdTensorFormatNchw, ::SimdTensorFormatNhwc.
    */
    SIMD_API void SimdSynetSetInput(const uint8_t * src, size_t width, size_t height, size_t stride, SimdPixelFormatType srcFormat, 
        const float * lower, const float * upper, float * dst, size_t channels, SimdTensorFormatType dstFormat);

    /*! @ingroup synet_other

        \fn void SimdSynetShuffleLayerForward(const float * src0, const float * src1, size_t channels0, size_t channels1, size_t spatial, float * dst0, float * dst1, SimdTensorFormatType format, int type);

        \short Performs forward propagation of FP32 ShuffleLayer.

        For type 0 the function splits even and odd channels from two input tensors into two output tensors.
        For type 1 it performs the inverse operation and interleaves channels from two input tensors into two
        output tensors. The number of channels in each input (type 0) or output (type 1) tensor must be even.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src0 - a pointer to the 32-bit float array with the first input image tensor.
        \param [in] src1 - a pointer to the 32-bit float array with the second input image tensor.
        \param [in] channels0 - a number of channels in the first input (type == 0) or output (type == 1) image tensor. It must be even number.
        \param [in] channels1 - a number of channels in the second input (type == 0) or output (type == 1) image tensor. It must be even number.
        \param [in] spatial - a spatial size of (input/output) image tensors.
        \param [out] dst0 - a pointer to the 32-bit float array with the first output image tensor.
        \param [out] dst1 - a pointer to the 32-bit float array with the second output image tensor.
        \param [in] format - a format of input and output image tensors. It can be ::SimdTensorFormatNchw or ::SimdTensorFormatNhwc.
        \param [in] type - a shuffle type: 0 for split operation, 1 for interleave operation.
    */
    SIMD_API void SimdSynetShuffleLayerForward(const float * src0, const float * src1, size_t channels0, size_t channels1, size_t spatial, float * dst0, float * dst1, SimdTensorFormatType format, int type);

    /*! @ingroup synet_activation

        \fn void SimdSynetSigmoid32f(const float * src, size_t size, const float * slope, float * dst);

        \short Calculates sigmoid function for FP32 array.

        Algorithm's details:
        \verbatim
        for(i = 0; i < size; ++i)
            dst[i] = 1/(1 + exp(-slope[0]*src[i]));
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array.
        \param [in] size - a size of input and output arrays.
        \param [in] slope - a pointer to slope parameter.
        \param [out] dst - a pointer to output 32-bit float array.
    */
    SIMD_API void SimdSynetSigmoid32f(const float* src, size_t size, const float* slope, float* dst);

    /*! @ingroup synet_other

        \fn void SimdSynetSoftmax32f(const float * src, size_t outer, size_t count, size_t inner, float * dst);

        \short Calculates FP32 softmax along count dimension.

        Algorithm's details:
        \verbatim
        for(o = 0; o < outer; ++o)
            for(i = 0; i < inner; ++i)
            {
                max = Max(src[(o*count + c)*inner + i]) over c in [0, count);
                sum = Sum(exp(src[(o*count + c)*inner + i] - max)) over c in [0, count);
                for(c = 0; c < count; ++c)
                    dst[(o*count + c)*inner + i] = exp(src[(o*count + c)*inner + i] - max)/sum;
            }
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input FP32 array. The size of the array must be equal to outer*count*inner.
        \param [in] outer - a product of dimensions before softmax axis.
        \param [in] count - a size of softmax axis.
        \param [in] inner - a product of dimensions after softmax axis.
        \param [out] dst - a pointer to the output FP32 array. The size of the array must be equal to outer*count*inner.
    */
    SIMD_API void SimdSynetSoftmax32f(const float * src, size_t outer, size_t count, size_t inner, float * dst);

    /*! @ingroup synet_other

        \fn void SimdSynetSoftmax16b(const uint16_t * src, size_t outer, size_t count, size_t inner, uint16_t * dst);

        \short Calculates BF16 softmax along count dimension.

        Input BF16 values are converted to FP32 for exponent and sum computations. The final probabilities are
        converted back to BF16.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input BF16 array. The size of the array must be equal to outer*count*inner.
        \param [in] outer - a product of dimensions before softmax axis.
        \param [in] count - a size of softmax axis.
        \param [in] inner - a product of dimensions after softmax axis.
        \param [out] dst - a pointer to the output BF16 array. The size of the array must be equal to outer*count*inner.
    */
    SIMD_API void SimdSynetSoftmax16b(const uint16_t* src, size_t outer, size_t count, size_t inner, uint16_t* dst);

    /*! @ingroup synet_activation

        \fn void SimdSynetSoftplus32f(const float* src, size_t size, const float * beta, const float * threshold, float * dst);

        \short Applies Softplus activation to a 32-bit floating point array.

        Algorithm's details:
        \verbatim
        _beta = beta[0];
        _threshold = threshold[0];
        for(i = 0; i < size; ++i)
        {
            value = src[i];
            dst[i] = value > _threshold ? value : log(1 + exp(value*_beta))/_beta;
        }
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array.
        \param [in] size - a size of input and output arrays.
        \param [in] beta - a pointer to the Softplus beta scalar parameter.
        \param [in] threshold - a pointer to the Softplus threshold scalar parameter. If input value is greater than threshold then output is equal to input.
        \param [out] dst - a pointer to the output 32-bit float array.
    */
    SIMD_API void SimdSynetSoftplus32f(const float* src, size_t size, const float * beta, const float * threshold, float * dst);

    /*! @ingroup synet_activation

        \fn void SimdSynetSwish32f(const float * src, size_t size, const float * slope, float * dst);

        \short Applies Swish activation to a 32-bit floating point array.

        Algorithm's details:
        \verbatim
        _slope = slope[0];
        for(i = 0; i < size; ++i)
            dst[i] = src[i]/(1 + exp(-_slope*src[i]));
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array.
        \param [in] size - a size of input and output arrays.
        \param [in] slope - a pointer to the Swish slope scalar parameter.
        \param [out] dst - a pointer to the output 32-bit float array.
    */
    SIMD_API void SimdSynetSwish32f(const float* src, size_t size, const float* slope, float* dst);

    /*! @ingroup synet_activation

        \fn void SimdSynetTanh32f(const float * src, size_t size, const float * slope, float * dst);

        \short Applies hyperbolic tangent activation to a 32-bit floating point array.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        Algorithm's details:
        \verbatim
        _slope = slope[0];
        for(i = 0; i < size; ++i)
            dst[i] = tanh(src[i]*_slope);
        \endverbatim

        \param [in] src - a pointer to the input 32-bit float array.
        \param [in] size - a size of input and output arrays.
        \param [in] slope - a pointer to the Tanh slope scalar parameter.
        \param [out] dst - a pointer to the output 32-bit float array.
    */
    SIMD_API void SimdSynetTanh32f(const float* src, size_t size, const float* slope, float* dst);

    /*! @ingroup synet_other

        \fn void SimdSynetTiledScale2D32f(const float* src, size_t channels, size_t height, size_t width, SimdTensorFormatType format, const float* ver, const float* hor, float* dst);

        \short Multiplies every element of a 3D FP32 tensor by two tiled 2D scale tensors.

        Algorithm's details for NCHW tensor format:
        \verbatim
        for(c = 0; c < channels; ++c)
            for(y = 0; y < height; ++y)
                for(x = 0; x < width; ++x)
                    dst[(c*height + y)*width + x] = src[(c*height + y)*width + x] * ver[c*width + x] * hor[c*height + y];
        \endverbatim

        Algorithm's details for NHWC tensor format:
        \verbatim
        for(y = 0; y < height; ++y)
            for(x = 0; x < width; ++x)
                for(c = 0; c < channels; ++c)
                    dst[(y*width + x)*channels + c] = src[(y*width + x)*channels + c] * ver[x*channels + c] * hor[y*channels + c];
        \endverbatim

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the 32-bit float array with input image tensor. The size of the array is equal to channels * height * width.
        \param [in] channels - a number of channels in the (input/output) image tensor.
        \param [in] height - a height of (input/output) image tensor.
        \param [in] width - a width of (input/output) image tensor.
        \param [in] format - a format of (input/output) image tensor. Only ::SimdTensorFormatNchw and ::SimdTensorFormatNhwc are supported.
        \param [in] ver - a pointer to the 32-bit float array with scale coefficients indexed by channel and column. The size of the array is equal to channels * width.
        \param [in] hor - a pointer to the 32-bit float array with scale coefficients indexed by channel and row. The size of the array is equal to channels * height.
        \param [out] dst - a pointer to the 32-bit float array with output image tensor. The size of the array is equal to channels * height * width. Input and output image tensors can be the same.
    */
    SIMD_API void SimdSynetTiledScale2D32f(const float* src, size_t channels, size_t height, size_t width, SimdTensorFormatType format, const float* ver, const float* hor, float* dst);

    /*! @ingroup synet_other

        \fn void SimdSynetUnaryOperation32f(const float * src, size_t size, SimdSynetUnaryOperation32fType type, float* dst);

        \short Applies selected unary operation to every element of a 32-bit floating point array.

        For every input element this function performs one of operations described by ::SimdSynetUnaryOperation32fType:
        absolute value, ceil, cosine, error function, exponent, floor, logarithm, negation, bitwise NOT of floating point
        representation, reciprocal, round, reciprocal square root, sign, sine, square root, hyperbolic tangent or zeroing.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array.
        \param [in] size - a size of the input and output arrays.
        \param [in] type - a unary operation type (see ::SimdSynetUnaryOperation32fType).
        \param [out] dst - a pointer to the output 32-bit float array.
    */
    SIMD_API void SimdSynetUnaryOperation32f(const float * src, size_t size, SimdSynetUnaryOperation32fType type, float * dst);

    /*! @ingroup texture_estimation

        \fn void SimdTextureBoostedSaturatedGradient(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t saturation, uint8_t boost, uint8_t * dx, size_t dxStride, uint8_t * dy, size_t dyStride);

        \short Calculates boosted saturated horizontal and vertical gradients for an 8-bit gray image.

        All images must have the same width, height and format (8-bit gray).

        For border pixels (x == 0, x == width - 1, y == 0 or y == height - 1):
        \verbatim
        dx[x, y] = 0;
        dy[x, y] = 0;
        \endverbatim
        For other pixels:
        \verbatim
        dx[x, y] = (saturation + max(-saturation, min(saturation, src[x + 1, y] - src[x - 1, y])))*boost;
        dy[x, y] = (saturation + max(-saturation, min(saturation, src[x, y + 1] - src[x, y - 1])))*boost;
        \endverbatim
        The values must satisfy: 2*saturation*boost <= 255.

        \note This function has a C++ wrappers: Simd::TextureBoostedSaturatedGradient(const View<A>& src, uint8_t saturation, uint8_t boost, View<A>& dx, View<A>& dy).

        \param [in] src - a pointer to pixels data of source 8-bit gray image.
        \param [in] srcStride - a row size of source image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] saturation - a saturation threshold of gradient.
        \param [in] boost - a boost coefficient.
        \param [out] dx - a pointer to pixels data of image with boosted saturated gradient along x axis.
        \param [in] dxStride - a row size of dx image.
        \param [out] dy - a pointer to pixels data of image with boosted saturated gradient along y axis.
        \param [in] dyStride - a row size of dy image.
    */
    SIMD_API void SimdTextureBoostedSaturatedGradient(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        uint8_t saturation, uint8_t boost, uint8_t * dx, size_t dxStride, uint8_t * dy, size_t dyStride);

    /*! @ingroup texture_estimation

        \fn void SimdTextureBoostedUv(const uint8_t * src, size_t srcStride, size_t width, size_t height, uint8_t boost, uint8_t * dst, size_t dstStride);

        \short Calculates boosted colorized texture feature of an 8-bit gray image (actual for U and V components of YUV format).

        All images must have the same width, height and format (8-bit gray).

        For every pixel:
        \verbatim
        lo = 128 - (128/boost);
        hi = 255 - lo;
        dst[x, y] = (max(lo, min(hi, src[x, y])) - lo)*boost;
        \endverbatim
        The boost value must be in range [1, 127].

        \note This function has a C++ wrappers: Simd::TextureBoostedUv(const View<A>& src, uint8_t boost, View<A>& dst).

        \param [in] src - a pointer to pixels data of source 8-bit gray image.
        \param [in] srcStride - a row size of source image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] boost - a boost coefficient.
        \param [out] dst - a pointer to pixels data of result image.
        \param [in] dstStride - a row size of destination image.
    */
    SIMD_API void SimdTextureBoostedUv(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        uint8_t boost, uint8_t * dst, size_t dstStride);

    /*! @ingroup texture_estimation

        \fn void SimdTextureGetDifferenceSum(const uint8_t * src, size_t srcStride, size_t width, size_t height, const uint8_t * lo, size_t loStride, const uint8_t * hi, size_t hiStride, int64_t * sum);

        \short Calculates sum of differences between current image and background range center.

        All images must have the same width, height and format (8-bit gray).

        For every pixel:
        \verbatim
        *sum = 0;
        for(y = 0; y < height; ++y)
            for(x = 0; x < width; ++x)
                *sum += src[x, y] - ((lo[x, y] + hi[x, y] + 1) >> 1);
        \endverbatim

        \note This function has a C++ wrappers: Simd::TextureGetDifferenceSum(const View<A>& src, const View<A>& lo, const View<A>& hi, int64_t & sum).

        \param [in] src - a pointer to pixels data of current image.
        \param [in] srcStride - a row size of current image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] lo - a pointer to pixels data of image with lower bound of background feature.
        \param [in] loStride - a row size of lo image.
        \param [in] hi - a pointer to pixels data of image with upper bound of background feature.
        \param [in] hiStride - a row size of hi image.
        \param [out] sum - a pointer to 64-bit integer with result sum.
    */
    SIMD_API void SimdTextureGetDifferenceSum(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        const uint8_t * lo, size_t loStride, const uint8_t * hi, size_t hiStride, int64_t * sum);

    /*! @ingroup texture_estimation

        \fn void SimdTexturePerformCompensation(const uint8_t * src, size_t srcStride, size_t width, size_t height, int32_t shift, uint8_t * dst, size_t dstStride);

        \short Adds signed brightness shift to an 8-bit gray image with saturation.

        All images must have the same width, height and format (8-bit gray).

        For every pixel:
        \verbatim
        dst[x, y] = max(0, min(255, src[x, y] + shift));
        \endverbatim
        The shift value must be in range (-255, 255). Input and output images can be the same.

        \note This function has a C++ wrappers: Simd::TexturePerformCompensation(const View<A>& src, int shift, View<A>& dst).

        \param [in] src - a pointer to pixels data of input image.
        \param [in] srcStride - a row size of input image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [in] shift - a compensation shift.
        \param [out] dst - a pointer to pixels data of output image.
        \param [in] dstStride - a row size of output image.
    */
    SIMD_API void SimdTexturePerformCompensation(const uint8_t * src, size_t srcStride, size_t width, size_t height,
        int32_t shift, uint8_t * dst, size_t dstStride);

    /*! @ingroup transform

        \fn void SimdTransformImage(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t pixelSize, SimdTransformType transform, uint8_t * dst, size_t dstStride);

        \short Copies, rotates, mirrors or transposes an image according to ::SimdTransformType.

        The function supports pixels with size 1, 2, 3 or 4 bytes. For ::SimdTransformRotate0, ::SimdTransformRotate180,
        ::SimdTransformTransposeRotate90 and ::SimdTransformTransposeRotate270 the output image has the same width and
        height as the input image. For ::SimdTransformRotate90, ::SimdTransformRotate270,
        ::SimdTransformTransposeRotate0 and ::SimdTransformTransposeRotate180 the output image width and height are
        equal to input height and width.

        \note This function has a C++ wrappers: Simd::TransformImage(const View<A> & src, ::SimdTransformType transform, View<A> & dst).

        \param [in] src - a pointer to pixels data of input image.
        \param [in] srcStride - a row size of input image.
        \param [in] width - an input image width. 
        \param [in] height - an input image height.
        \param [in] pixelSize - a pixel size in input and output images. It can be 1, 2, 3, 4.
        \param [in] transform - a type of image transformation.
        \param [out] dst - a pointer to pixels data of output image.
        \param [in] dstStride - a row size of output image.
    */
    SIMD_API void SimdTransformImage(const uint8_t * src, size_t srcStride, size_t width, size_t height, size_t pixelSize, SimdTransformType transform, uint8_t * dst, size_t dstStride);

    /*! @ingroup uyvy_conversion

        \fn void SimdUyvy422ToBgr(const uint8_t * uyvy, size_t uyvyStride, size_t width, size_t height, uint8_t * bgr, size_t bgrStride, SimdYuvType yuvType);

        \short Converts a 16-bit UYVY422 image to a 24-bit BGR image.

        The input and output images must have the same width and height. Width must be even number.
        Every 4 bytes of UYVY422 input contain two pixels in order: U0, Y0, V0, Y1.

        \note This function has a C++ wrappers: Simd::Uyvy422ToBgr(const View<A>& uyvy, View<A>& bgr, SimdYuvType yuvType = SimdYuvBt601);

        \param [in] uyvy - a pointer to pixels data of input 16-bit UYVY422 image.
        \param [in] uyvyStride - a row size of the UYVY422 image.
        \param [in] width - an image width. Width must be even number.
        \param [in] height - an image height.
        \param [out] bgr - a pointer to pixels data of output 24-bit BGR image.
        \param [in] bgrStride - a row size of the bgr image.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdUyvy422ToBgr(const uint8_t* uyvy, size_t uyvyStride, size_t width, size_t height, uint8_t* bgr, size_t bgrStride, SimdYuvType yuvType);

    /*! @ingroup uyvy_conversion

        \fn void SimdUyvy422ToYuv420p(const uint8_t* uyvy, size_t uyvyStride, size_t width, size_t height, uint8_t* y, size_t yStride, uint8_t* u, size_t uStride, uint8_t* v, size_t vStride);

        \short Converts a 16-bit UYVY422 image to planar YUV420P.

        The input UYVY422 and output Y images must have the same width and height.
        The output U and V images must have width and height equal to half of Y component width and height.
        Width and height must be even numbers. Every 4 bytes of UYVY422 input contain two pixels in order: U0, Y0, V0, Y1.
        The output Y plane receives all Y samples, and every output U or V sample is the rounded average of corresponding
        chroma samples from two adjacent UYVY rows:
        \verbatim
        y[2*x + 0, 2*y + 0] = uyvy0[4*x + 1];
        y[2*x + 1, 2*y + 0] = uyvy0[4*x + 3];
        y[2*x + 0, 2*y + 1] = uyvy1[4*x + 1];
        y[2*x + 1, 2*y + 1] = uyvy1[4*x + 3];
        u[x, y] = (uyvy0[4*x + 0] + uyvy1[4*x + 0] + 1)/2;
        v[x, y] = (uyvy0[4*x + 2] + uyvy1[4*x + 2] + 1)/2;
        \endverbatim

        \note This function has a C++ wrapper Simd::Uyvy422ToYuv420p(const View<A>& uyvy, View<A>& y, View<A>& u, View<A>& v).

        \param [in] uyvy - a pointer to pixels data of input 16-bit UYVY422 image.
        \param [in] uyvyStride - a row size of the UYVY422 image.
        \param [in] width - an image width. Width must be even number.
        \param [in] height - an image height. Height must be even number.
        \param [out] y - a pointer to pixels data of output 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [out] u - a pointer to pixels data of output 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [out] v - a pointer to pixels data of output 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
    */
    SIMD_API void SimdUyvy422ToYuv420p(const uint8_t* uyvy, size_t uyvyStride, size_t width, size_t height, 
        uint8_t* y, size_t yStride, uint8_t* u, size_t uStride, uint8_t* v, size_t vStride);

    /*! @ingroup warp_affine

        \fn void * SimdWarpAffineInit(size_t srcW, size_t srcH, size_t srcS, size_t dstW, size_t dstH, size_t dstS, size_t channels, const float* mat, SimdWarpAffineFlags flags, const uint8_t * border);

        \short Creates warp affine context.

        The input matrix describes forward affine transformation from source coordinates to destination coordinates:
        \verbatim
        dstX = srcX*mat[0] + srcY*mat[1] + mat[2];
        dstY = srcX*mat[3] + srcY*mat[4] + mat[5];
        \endverbatim
        The context stores inverse matrix and, during ::SimdWarpAffineRun, samples source image for every destination pixel.
        Supported data type is 8-bit unsigned channels, supported channel count is 1..4, and supported interpolation methods
        are nearest and bilinear. With ::SimdWarpAffineBorderConstant, pixels outside the source image are filled by border
        color (zero color when border is NULL). With ::SimdWarpAffineBorderTransparent, destination pixels outside transformed
        source area are left unchanged.

        An using example (for BGR image):
        \verbatim
        float mat[2][3] = { { 1.0f, -1.0f, 0.0f }, { 1.0f, 1.0f, 0.0f } };
        SimdWarpAffineFlags flags = SimdWarpAffineChannelByte | SimdWarpAffineInterpBilinear | SimdWarpAffineBorderConstant;
        void* context = SimdWarpAffineInit(srcW, srcH, srcS, dstW, dstH, dstS, 3, mat, flags, NULL);
        if (context)
        {
             SimdWarpAffineRun(context, src, dst);
             SimdRelease(context);
        }
        \endverbatim

        \note This function has a C++ wrapper Simd::WarpAffine(const View<A>& src, const float * mat, View<A>& dst, SimdWarpAffineFlags flags = SimdWarpAffineInterpBilinear | SimdWarpAffineBorderConstant, const uint8_t* border = NULL).

        \param [in] srcW - a width of input image.
        \param [in] srcH - a height of input image.
        \param [in] srcS - a row size (in bytes) of the input image.
        \param [in] dstW - a width of output image.
        \param [in] dstH - a height of output image.
        \param [in] dstS - a row size (in bytes) of the output image.
        \param [in] channels - a channel number of input and output image. Its value must be in range [1..4].
        \param [in] mat - a pointer to 2x3 matrix with coefficients of forward affine transformation.
        \param [in] flags - flags of algorithm parameters (channel type, interpolation method and border method).
        \param [in] border - a pointer to the array with color of border. The size of the array must be equal to channels.
                             This parameter is actual for ::SimdWarpAffineBorderConstant flag. It can be NULL.
        \return a pointer to warp affine context. On error it returns NULL.
                This pointer is used in functions ::SimdWarpAffineRun.
                It must be released with using of function ::SimdRelease.
    */
    SIMD_API void* SimdWarpAffineInit(size_t srcW, size_t srcH, size_t srcS, size_t dstW, size_t dstH, size_t dstS,
        size_t channels, const float* mat, SimdWarpAffineFlags flags, const uint8_t * border);

    /*! @ingroup warp_affine

        \fn void SimdWarpAffineRun(const void* context, const uint8_t* src, uint8_t* dst);

        \short Performs warp affine transformation for current image.

        The function uses parameters and precomputed data from the context created by ::SimdWarpAffineInit. If transparent
        border mode is used, destination image has to contain the background values before calling this function.

        \note This function has a C++ wrapper Simd::WarpAffine(const View<A>& src, const float * mat, View<A>& dst, SimdWarpAffineFlags flags = SimdWarpAffineInterpBilinear | SimdWarpAffineBorderConstant, const uint8_t* border = NULL).

        \param [in] context - a warp affine context. It must be created by function ::SimdWarpAffineInit and released by function ::SimdRelease.
        \param [in] src - a pointer to pixels data of the original input image.
        \param [out] dst - a pointer to pixels data of the transformed output image.
    */
    SIMD_API void SimdWarpAffineRun(const void* context, const uint8_t* src, uint8_t* dst);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel1x3Block1x4SetFilter(const float * src, size_t size, float * dst, SimdBool trans);

        \short Converts 1x3 convolution filters to Winograd F(1x4,1x3) domain (or 3x1 filters to F(4x1,3x1) domain).

        For every input-output channel pair the function transforms 3 source filter values to 6 Winograd coefficients.
        If trans is ::SimdFalse, every source filter is stored contiguously and output coefficients are separated by size.
        If trans is ::SimdTrue, every coefficient plane is stored with stride equal to size.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array with filter weights.
        \param [in] size - (number of input channels)*(number of output channels).
        \param [out] dst - a pointer to the output 32-bit float array with 6 transformed coefficients per channel pair.
        \param [in] trans - a flag of transposed filter layout.
    */
    SIMD_API void SimdWinogradKernel1x3Block1x4SetFilter(const float* src, size_t size, float* dst, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel1x3Block1x4SetInput(const float * src, size_t srcChannels, size_t srcHeight, size_t srcWidth, size_t padY, size_t padX, size_t padH, size_t padW, float * dst, size_t dstStride, SimdBool trans);

        \short Converts input tensor rows to Winograd F(1x4,1x3) domain.

        The function transforms source rows by horizontal tiles. Every full tile consumes 6 input values and prepares data
        for 4 output values. Only horizontal zero padding is supported: padY and padH must be 0, padX must be equal to padW,
        and padX must be 0 or 1. The transformed output width is srcWidth - 2 without padding, or srcWidth with padding.
        If trans is ::SimdFalse, source tensor layout is CHW. If trans is ::SimdTrue, source tensor layout is HWC and
        transformed channel values are packed contiguously for every tile.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input image.
        \param [in] srcChannels - a number of input channels.
        \param [in] srcHeight - a height of input image.
        \param [in] srcWidth - a width of input image.
        \param [in] padY - an additional zero padding of input image at the beginning of Y-axis.
        \param [in] padX - an additional zero padding of input image at the beginning of X-axis.
        \param [in] padH - an additional zero padding of input image at the end of Y-axis.
        \param [in] padW - an additional zero padding of input image at the end of X-axis.
        \param [out] dst - a pointer to the output array with converted image tiles.
        \param [in] dstStride - a distance between adjacent Winograd coefficients in output array.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel1x3Block1x4SetInput(const float* src, size_t srcChannels, size_t srcHeight, size_t srcWidth,
        size_t padY, size_t padX, size_t padH, size_t padW, float* dst, size_t dstStride, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel1x3Block1x4SetOutput(const float * src, size_t srcStride, float * dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

        \short Converts output tensor rows from Winograd F(1x4,1x3) domain.

        Every tile contains 6 Winograd coefficients and produces up to 4 output values. Tail tiles are clipped to dstWidth.
        If trans is ::SimdFalse, destination tensor layout is CHW. If trans is ::SimdTrue, destination tensor layout is HWC.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input array with converted image tiles.
        \param [in] srcStride - a distance between adjacent Winograd coefficients in input array.
        \param [out] dst - a pointer to the output image.
        \param [in] dstChannels - a number of output channels.
        \param [in] dstHeight - a height of output image.
        \param [in] dstWidth - a width of output image.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel1x3Block1x4SetOutput(const float* src, size_t srcStride, float* dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel1x5Block1x4SetFilter(const float * src, size_t size, float * dst, SimdBool trans);

        \short Converts 1x5 convolution filters to Winograd F(1x4,1x5) domain (or 5x1 filters to F(4x1,5x1) domain).

        For every input-output channel pair the function transforms 5 source filter values to 8 Winograd coefficients.
        If trans is ::SimdFalse, every source filter is stored contiguously and output coefficients are separated by size.
        If trans is ::SimdTrue, every coefficient plane is stored with stride equal to size.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array with filter weights.
        \param [in] size - (number of input channels)*(number of output channels).
        \param [out] dst - a pointer to the output 32-bit float array with 8 transformed coefficients per channel pair.
        \param [in] trans - a flag of transposed filter layout.
    */
    SIMD_API void SimdWinogradKernel1x5Block1x4SetFilter(const float* src, size_t size, float* dst, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel1x5Block1x4SetInput(const float * src, size_t srcChannels, size_t srcHeight, size_t srcWidth, size_t padY, size_t padX, size_t padH, size_t padW, float * dst, size_t dstStride, SimdBool trans);

        \short Converts input tensor rows to Winograd F(1x4,1x5) domain.

        The function transforms source rows by horizontal tiles. Every full tile consumes 8 input values and prepares data
        for 4 output values. Only horizontal zero padding is supported: padY and padH must be 0, padX must be equal to padW,
        and padX must be 0 or 2. The transformed output width is srcWidth - 4 without padding, or srcWidth with padding.
        If trans is ::SimdFalse, source tensor layout is CHW. If trans is ::SimdTrue, source tensor layout is HWC and
        transformed channel values are packed contiguously for every tile.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input image.
        \param [in] srcChannels - a number of input channels.
        \param [in] srcHeight - a height of input image.
        \param [in] srcWidth - a width of input image.
        \param [in] padY - an additional zero padding of input image at the beginning of Y-axis.
        \param [in] padX - an additional zero padding of input image at the beginning of X-axis.
        \param [in] padH - an additional zero padding of input image at the end of Y-axis.
        \param [in] padW - an additional zero padding of input image at the end of X-axis.
        \param [out] dst - a pointer to the output array with converted image tiles.
        \param [in] dstStride - a distance between adjacent Winograd coefficients in output array.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel1x5Block1x4SetInput(const float* src, size_t srcChannels, size_t srcHeight, size_t srcWidth,
        size_t padY, size_t padX, size_t padH, size_t padW, float* dst, size_t dstStride, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel1x5Block1x4SetOutput(const float * src, size_t srcStride, float * dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

        \short Converts output tensor rows from Winograd F(1x4,1x5) domain.

        Every tile contains 8 Winograd coefficients and produces up to 4 output values. Tail tiles are clipped to dstWidth.
        If trans is ::SimdFalse, destination tensor layout is CHW. If trans is ::SimdTrue, destination tensor layout is HWC.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input array with converted image tiles.
        \param [in] srcStride - a distance between adjacent Winograd coefficients in input array.
        \param [out] dst - a pointer to the output image.
        \param [in] dstChannels - a number of output channels.
        \param [in] dstHeight - a height of output image.
        \param [in] dstWidth - a width of output image.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel1x5Block1x4SetOutput(const float* src, size_t srcStride, float* dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel2x2Block2x2SetFilter(const float * src, size_t size, float * dst, SimdBool trans);

        \short Converts 2x2 convolution filters to Winograd F(2x2,2x2) domain.

        For every input-output channel pair the function transforms 4 source filter values to 9 Winograd coefficients
        arranged as a 3x3 transformed filter. If trans is ::SimdFalse, every source filter is stored contiguously and
        output coefficients are separated by size. If trans is ::SimdTrue, every coefficient plane is stored with stride
        equal to size.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array with filter weights.
        \param [in] size - (number of input channels)*(number of output channels).
        \param [out] dst - a pointer to the output 32-bit float array with 9 transformed coefficients per channel pair.
        \param [in] trans - a flag of transposed filter layout.
    */
    SIMD_API void SimdWinogradKernel2x2Block2x2SetFilter(const float* src, size_t size, float* dst, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel2x2Block2x2SetInput(const float * src, size_t srcChannels, size_t srcHeight, size_t srcWidth, size_t padY, size_t padX, size_t padH, size_t padW, float * dst, size_t dstStride, SimdBool trans);

        \short Converts input tensor tiles to Winograd F(2x2,2x2) domain.

        Every full tile consumes a 3x3 input patch and prepares data for a 2x2 output block. Padding must satisfy
        padY == padX, padW == padH, and padY + padH is 0 or 1. The transformed output size is
        (srcHeight - 1 + padY + padH) x (srcWidth - 1 + padX + padW). If trans is ::SimdFalse, source tensor layout is
        CHW. If trans is ::SimdTrue, source tensor layout is HWC and transformed channel values are packed contiguously
        for every tile.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input image.
        \param [in] srcChannels - a number of input channels.
        \param [in] srcHeight - a height of input image.
        \param [in] srcWidth - a width of input image.
        \param [in] padY - an additional zero padding of input image at the beginning of Y-axis.
        \param [in] padX - an additional zero padding of input image at the beginning of X-axis.
        \param [in] padH - an additional zero padding of input image at the end of Y-axis.
        \param [in] padW - an additional zero padding of input image at the end of X-axis.
        \param [out] dst - a pointer to the output array with converted image tiles.
        \param [in] dstStride - a distance between adjacent Winograd coefficients in output array.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel2x2Block2x2SetInput(const float* src, size_t srcChannels, size_t srcHeight, size_t srcWidth,
        size_t padY, size_t padX, size_t padH, size_t padW, float* dst, size_t dstStride, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel2x2Block2x2SetOutput(const float * src, size_t srcStride, float * dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

        \short Converts output tensor tiles from Winograd F(2x2,2x2) domain.

        Every tile contains a 3x3 set of Winograd coefficients and produces up to a 2x2 output block. Tail blocks are
        clipped to dstHeight and dstWidth. If trans is ::SimdFalse, destination tensor layout is CHW. If trans is
        ::SimdTrue, destination tensor layout is HWC.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input array with converted image tiles.
        \param [in] srcStride - a distance between adjacent Winograd coefficients in input array.
        \param [out] dst - a pointer to the output image.
        \param [in] dstChannels - a number of output channels.
        \param [in] dstHeight - a height of output image.
        \param [in] dstWidth - a width of output image.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel2x2Block2x2SetOutput(const float* src, size_t srcStride, float* dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel2x2Block4x4SetFilter(const float * src, size_t size, float * dst, SimdBool trans);

        \short Converts 2x2 convolution filters to Winograd F(4x4,2x2) domain.

        For every input-output channel pair the function transforms 4 source filter values to 25 Winograd coefficients
        arranged as a 5x5 transformed filter. If trans is ::SimdFalse, every source filter is stored contiguously and
        output coefficients are separated by size. If trans is ::SimdTrue, every coefficient plane is stored with stride
        equal to size.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array with filter weights.
        \param [in] size - (number of input channels)*(number of output channels).
        \param [out] dst - a pointer to the output 32-bit float array with 25 transformed coefficients per channel pair.
        \param [in] trans - a flag of transposed filter layout.
    */
    SIMD_API void SimdWinogradKernel2x2Block4x4SetFilter(const float* src, size_t size, float* dst, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel2x2Block4x4SetInput(const float * src, size_t srcChannels, size_t srcHeight, size_t srcWidth, size_t padY, size_t padX, size_t padH, size_t padW, float * dst, size_t dstStride, SimdBool trans);

        \short Converts input tensor tiles to Winograd F(4x4,2x2) domain.

        Every full tile consumes a 5x5 input patch and prepares data for a 4x4 output block. Padding must satisfy
        padY == padX, padW == padH, and padY + padH is 0 or 1. The transformed output size is
        (srcHeight - 1 + padY + padH) x (srcWidth - 1 + padX + padW). If trans is ::SimdFalse, source tensor layout is
        CHW. If trans is ::SimdTrue, source tensor layout is HWC and transformed channel values are packed contiguously
        for every tile.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input image.
        \param [in] srcChannels - a number of input channels.
        \param [in] srcHeight - a height of input image.
        \param [in] srcWidth - a width of input image.
        \param [in] padY - an additional zero padding of input image at the beginning of Y-axis.
        \param [in] padX - an additional zero padding of input image at the beginning of X-axis.
        \param [in] padH - an additional zero padding of input image at the end of Y-axis.
        \param [in] padW - an additional zero padding of input image at the end of X-axis.
        \param [out] dst - a pointer to the output array with converted image tiles.
        \param [in] dstStride - a distance between adjacent Winograd coefficients in output array.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel2x2Block4x4SetInput(const float* src, size_t srcChannels, size_t srcHeight, size_t srcWidth,
        size_t padY, size_t padX, size_t padH, size_t padW, float* dst, size_t dstStride, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel2x2Block4x4SetOutput(const float * src, size_t srcStride, float * dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

        \short Converts output tensor tiles from Winograd F(4x4,2x2) domain.

        Every tile contains a 5x5 set of Winograd coefficients and produces up to a 4x4 output block. Tail blocks are
        clipped to dstHeight and dstWidth. If trans is ::SimdFalse, destination tensor layout is CHW. If trans is
        ::SimdTrue, destination tensor layout is HWC.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input array with converted image tiles.
        \param [in] srcStride - a distance between adjacent Winograd coefficients in input array.
        \param [out] dst - a pointer to the output image.
        \param [in] dstChannels - a number of output channels.
        \param [in] dstHeight - a height of output image.
        \param [in] dstWidth - a width of output image.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel2x2Block4x4SetOutput(const float* src, size_t srcStride, float* dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel3x3Block2x2SetFilter(const float * src, size_t size, float * dst, SimdBool trans);

        \short Converts 3x3 convolution filters to Winograd F(2x2,3x3) domain.

        For every input-output channel pair the function transforms 9 source filter values to 16 Winograd coefficients
        arranged as a 4x4 transformed filter. If trans is ::SimdFalse, every source filter is stored contiguously and
        output coefficients are separated by size. If trans is ::SimdTrue, every coefficient plane is stored with stride
        equal to size.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array with filter weights.
        \param [in] size - (number of input channels)*(number of output channels).
        \param [out] dst - a pointer to the output 32-bit float array with 16 transformed coefficients per channel pair.
        \param [in] trans - a flag of transposed filter layout.
    */
    SIMD_API void SimdWinogradKernel3x3Block2x2SetFilter(const float * src, size_t size, float * dst, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel3x3Block2x2SetInput(const float * src, size_t srcChannels, size_t srcHeight, size_t srcWidth, size_t padY, size_t padX, size_t padH, size_t padW, float * dst, size_t dstStride, SimdBool trans);

        \short Converts input tensor tiles to Winograd F(2x2,3x3) domain.

        Every full tile consumes a 4x4 input patch and prepares data for a 2x2 output block. Padding must be equal on all
        sides and must be 0 or 1. The transformed output size is srcHeight x srcWidth with padding, or
        (srcHeight - 2) x (srcWidth - 2) without padding. If trans is ::SimdFalse, source tensor layout is CHW. If trans
        is ::SimdTrue, source tensor layout is HWC and transformed channel values are packed contiguously for every tile.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input image.
        \param [in] srcChannels - a number of input channels.
        \param [in] srcHeight - a height of input image.
        \param [in] srcWidth - a width of input image.
        \param [in] padY - an additional zero padding of input image at the beginning of Y-axis.
        \param [in] padX - an additional zero padding of input image at the beginning of X-axis.
        \param [in] padH - an additional zero padding of input image at the end of Y-axis.
        \param [in] padW - an additional zero padding of input image at the end of X-axis.
        \param [out] dst - a pointer to the output array with converted image tiles.
        \param [in] dstStride - a distance between adjacent Winograd coefficients in output array.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel3x3Block2x2SetInput(const float * src, size_t srcChannels, size_t srcHeight, size_t srcWidth,
        size_t padY, size_t padX, size_t padH, size_t padW, float * dst, size_t dstStride, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel3x3Block2x2SetOutput(const float * src, size_t srcStride, float * dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

        \short Converts output tensor tiles from Winograd F(2x2,3x3) domain.

        Every tile contains a 4x4 set of Winograd coefficients and produces up to a 2x2 output block. Tail blocks are
        clipped to dstHeight and dstWidth. If trans is ::SimdFalse, destination tensor layout is CHW. If trans is
        ::SimdTrue, destination tensor layout is HWC.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input array with converted image tiles.
        \param [in] srcStride - a distance between adjacent Winograd coefficients in input array.
        \param [out] dst - a pointer to the output image.
        \param [in] dstChannels - a number of output channels.
        \param [in] dstHeight - a height of output image.
        \param [in] dstWidth - a width of output image.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel3x3Block2x2SetOutput(const float * src, size_t srcStride, float * dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel3x3Block3x3SetFilter(const float * src, size_t size, float * dst, SimdBool trans);

        \short Converts 3x3 convolution filters to Winograd F(3x3,3x3) domain.

        For every input-output channel pair the function transforms 9 source filter values to 25 Winograd coefficients
        arranged as a 5x5 transformed filter. If trans is ::SimdFalse, every source filter is stored contiguously and
        output coefficients are separated by size. If trans is ::SimdTrue, every coefficient plane is stored with stride
        equal to size.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array with filter weights.
        \param [in] size - (number of input channels)*(number of output channels).
        \param [out] dst - a pointer to the output 32-bit float array with 25 transformed coefficients per channel pair.
        \param [in] trans - a flag of transposed filter layout.
    */
    SIMD_API void SimdWinogradKernel3x3Block3x3SetFilter(const float * src, size_t size, float * dst, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel3x3Block3x3SetInput(const float * src, size_t srcChannels, size_t srcHeight, size_t srcWidth, size_t padY, size_t padX, size_t padH, size_t padW, float * dst, size_t dstStride, SimdBool trans);

        \short Converts input tensor tiles to Winograd F(3x3,3x3) domain.

        Every full tile consumes a 5x5 input patch and prepares data for a 3x3 output block. Padding must be equal on all
        sides and must be 0 or 1. The transformed output size is srcHeight x srcWidth with padding, or
        (srcHeight - 2) x (srcWidth - 2) without padding. If trans is ::SimdFalse, source tensor layout is CHW. If trans
        is ::SimdTrue, source tensor layout is HWC and transformed channel values are packed contiguously for every tile.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input image.
        \param [in] srcChannels - a number of input channels.
        \param [in] srcHeight - a height of input image.
        \param [in] srcWidth - a width of input image.
        \param [in] padY - an additional zero padding of input image at the beginning of Y-axis.
        \param [in] padX - an additional zero padding of input image at the beginning of X-axis.
        \param [in] padH - an additional zero padding of input image at the end of Y-axis.
        \param [in] padW - an additional zero padding of input image at the end of X-axis.
        \param [out] dst - a pointer to the output array with converted image tiles.
        \param [in] dstStride - a distance between adjacent Winograd coefficients in output array.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel3x3Block3x3SetInput(const float* src, size_t srcChannels, size_t srcHeight, size_t srcWidth,
        size_t padY, size_t padX, size_t padH, size_t padW, float* dst, size_t dstStride, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel3x3Block3x3SetOutput(const float * src, size_t srcStride, float * dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

        \short Converts output tensor tiles from Winograd F(3x3,3x3) domain.

        Every tile contains a 5x5 set of Winograd coefficients and produces up to a 3x3 output block. Tail blocks are
        clipped to dstHeight and dstWidth. If trans is ::SimdFalse, destination tensor layout is CHW. If trans is
        ::SimdTrue, destination tensor layout is HWC.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input array with converted image tiles.
        \param [in] srcStride - a distance between adjacent Winograd coefficients in input array.
        \param [out] dst - a pointer to the output image.
        \param [in] dstChannels - a number of output channels.
        \param [in] dstHeight - a height of output image.
        \param [in] dstWidth - a width of output image.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel3x3Block3x3SetOutput(const float * src, size_t srcStride, float * dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel3x3Block4x4SetFilter(const float * src, size_t size, float * dst, SimdBool trans);

        \short Converts 3x3 convolution filters to Winograd F(4x4,3x3) domain.

        For every input-output channel pair the function transforms 9 source filter values to 36 Winograd coefficients
        arranged as a 6x6 transformed filter. If trans is ::SimdFalse, every source filter is stored contiguously and
        output coefficients are separated by size. If trans is ::SimdTrue, every coefficient plane is stored with stride
        equal to size.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input 32-bit float array with filter weights.
        \param [in] size - (number of input channels)*(number of output channels).
        \param [out] dst - a pointer to the output 32-bit float array with 36 transformed coefficients per channel pair.
        \param [in] trans - a flag of transposed filter layout.
    */
    SIMD_API void SimdWinogradKernel3x3Block4x4SetFilter(const float * src, size_t size, float * dst, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel3x3Block4x4SetInput(const float * src, size_t srcChannels, size_t srcHeight, size_t srcWidth, size_t padY, size_t padX, size_t padH, size_t padW, float * dst, size_t dstStride, SimdBool trans);

        \short Converts input tensor tiles to Winograd F(4x4,3x3) domain.

        Every full tile consumes a 6x6 input patch and prepares data for a 4x4 output block. Padding must satisfy
        padY + padH <= 2 and padX + padW <= 2. The transformed output size is
        (srcHeight - 2 + padY + padH) x (srcWidth - 2 + padX + padW). If trans is ::SimdFalse, source tensor layout is
        CHW. If trans is ::SimdTrue, source tensor layout is HWC and transformed channel values are packed contiguously
        for every tile.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input image.
        \param [in] srcChannels - a number of input channels.
        \param [in] srcHeight - a height of input image.
        \param [in] srcWidth - a width of input image.
        \param [in] padY - an additional zero padding of input image at the beginning of Y-axis.
        \param [in] padX - an additional zero padding of input image at the beginning of X-axis.
        \param [in] padH - an additional zero padding of input image at the end of Y-axis.
        \param [in] padW - an additional zero padding of input image at the end of X-axis.
        \param [out] dst - a pointer to the output array with converted image tiles.
        \param [in] dstStride - a distance between adjacent Winograd coefficients in output array.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel3x3Block4x4SetInput(const float* src, size_t srcChannels, size_t srcHeight, size_t srcWidth,
        size_t padY, size_t padX, size_t padH, size_t padW, float* dst, size_t dstStride, SimdBool trans);

    /*! @ingroup synet_winograd

        \fn void SimdWinogradKernel3x3Block4x4SetOutput(const float * src, size_t srcStride, float * dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

        \short Converts output tensor tiles from Winograd F(4x4,3x3) domain.

        Every tile contains a 6x6 set of Winograd coefficients and produces up to a 4x4 output block. Tail blocks are
        clipped to dstHeight and dstWidth. If trans is ::SimdFalse, destination tensor layout is CHW. If trans is
        ::SimdTrue, destination tensor layout is HWC.

        \note This function is used in <a href="http://github.com/ermig1979/Synet">Synet Framework</a>.

        \param [in] src - a pointer to the input array with converted image tiles.
        \param [in] srcStride - a distance between adjacent Winograd coefficients in input array.
        \param [out] dst - a pointer to the output image.
        \param [in] dstChannels - a number of output channels.
        \param [in] dstHeight - a height of output image.
        \param [in] dstWidth - a width of output image.
        \param [in] trans - a flag of transposed tensor layout.
    */
    SIMD_API void SimdWinogradKernel3x3Block4x4SetOutput(const float * src, size_t srcStride, float * dst, size_t dstChannels, size_t dstHeight, size_t dstWidth, SimdBool trans);

    /*! @ingroup yuv_conversion

        \fn void SimdYToGray(const uint8_t* y, size_t yStride, size_t width, size_t height, uint8_t* gray, size_t grayStride);

        \short Converts 8-bit Y-plane of YUV image to 8-bit gray image.

        The input Y and output gray images must have the same width and height.
        The Y values are treated as studio-range luma [16..235] and converted to full-range gray [0..255].

        \note This function has C++ wrappers: Simd::YToGray(const View& y, View& gray).

        \param [in] y - a pointer to pixels data of input 8-bit Y plane of YUV image.
        \param [in] yStride - a row size of the y image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] gray - a pointer to pixels data of output 8-bit gray image.
        \param [in] grayStride - a row size of the gray image.
    */
    SIMD_API void SimdYToGray(const uint8_t* y, size_t yStride, size_t width, size_t height, uint8_t* gray, size_t grayStride);

    /*! @ingroup yuv_conversion

        \fn void SimdYuva420pToBgraV2(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, const uint8_t * a, size_t aStride, size_t width, size_t height, uint8_t * bgra, size_t bgraStride, SimdYuvType yuvType);

        \short Converts 8-bit planar YUVA 4:2:0 image to 32-bit BGRA image.

        The input Y, A and output BGRA images must have the same width and height.
        The input U and V images must have the same width and height (their width and height are equal to half of the Y width and half of the Y height).
        The image width and height must be even and not less than 2. One U and one V value are used for each 2x2 block of Y and A values.

        \note This function has a C++ wrapper: Simd::Yuva420pToBgra(const View<A>& y, const View<A>& u, const View<A>& v, const View<A>& a, View<A>& bgra, SimdYuvType yuvType = SimdYuvBt601).

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] a - a pointer to pixels data of input 8-bit image with alpha channel.
        \param [in] aStride - a row size of the a image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] bgra - a pointer to pixels data of output 32-bit BGRA image.
        \param [in] bgraStride - a row size of the bgra image.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuva420pToBgraV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        const uint8_t* a, size_t aStride, size_t width, size_t height, uint8_t* bgra, size_t bgraStride, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuva422pToBgraV2(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, const uint8_t * a, size_t aStride, size_t width, size_t height, uint8_t * bgra, size_t bgraStride, SimdYuvType yuvType);

        \short Converts 8-bit planar YUVA 4:2:2 image to 32-bit BGRA image.

        The input Y, A and output BGRA images must have the same width and height.
        The input U and V images must have the same width and height (their width is equal to half of the Y width, their height is equal to the Y height).
        The image width must be even and not less than 2. One U and one V value are used for each horizontal pair of Y and A values.

        \note This function has a C++ wrapper: Simd::Yuva422pToBgra(const View<A>& y, const View<A>& u, const View<A>& v, const View<A>& a, View<A>& bgra, SimdYuvType yuvType = SimdYuvBt601).

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] a - a pointer to pixels data of input 8-bit image with alpha channel.
        \param [in] aStride - a row size of the a image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] bgra - a pointer to pixels data of output 32-bit BGRA image.
        \param [in] bgraStride - a row size of the bgra image.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuva422pToBgraV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        const uint8_t* a, size_t aStride, size_t width, size_t height, uint8_t* bgra, size_t bgraStride, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuva444pToBgraV2(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, const uint8_t * a, size_t aStride, size_t width, size_t height, uint8_t * bgra, size_t bgraStride, SimdYuvType yuvType);

        \short Converts 8-bit planar YUVA 4:4:4 image to 32-bit BGRA image.

        The input Y, U, V, A and output BGRA images must have the same width and height.
        Each output alpha value is copied from the corresponding input A value.

        \note This function has a C++ wrapper: Simd::Yuva444pToBgra(const View<A>& y, const View<A>& u, const View<A>& v, const View<A>& a, View<A>& bgra, SimdYuvType yuvType = SimdYuvBt601).

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] a - a pointer to pixels data of input 8-bit image with alpha channel.
        \param [in] aStride - a row size of the a image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] bgra - a pointer to pixels data of output 32-bit BGRA image.
        \param [in] bgraStride - a row size of the bgra image.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuva444pToBgraV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        const uint8_t* a, size_t aStride, size_t width, size_t height, uint8_t* bgra, size_t bgraStride, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv420pToBgrV2(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * bgr, size_t bgrStride, SimdYuvType yuvType);

        \short Converts 8-bit planar YUV 4:2:0 image to 24-bit BGR image.

        The input Y and output BGR images must have the same width and height.
        The input U and V images must have the same width and height (their width and height are equal to half of the Y width and half of the Y height).
        The image width and height must be even and not less than 2. One U and one V value are used for each 2x2 block of Y values.

        \note This function has a C++ wrapper: Simd::Yuv420pToBgr(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& bgr, SimdYuvType yuvType = SimdYuvBt601);

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] bgr - a pointer to pixels data of output 24-bit BGR image.
        \param [in] bgrStride - a row size of the bgr image.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuv420pToBgrV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        size_t width, size_t height, uint8_t* bgr, size_t bgrStride, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv422pToBgrV2(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * bgr, size_t bgrStride, SimdYuvType yuvType);

        \short Converts 8-bit planar YUV 4:2:2 image to 24-bit BGR image.

        The input Y and output BGR images must have the same width and height.
        The input U and V images must have the same width and height (their width is equal to half of the Y width, their height is equal to the Y height).
        The image width must be even and not less than 2. One U and one V value are used for each horizontal pair of Y values.

        \note This function has a C++ wrapper: Simd::Yuv422pToBgr(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& bgr, SimdYuvType yuvType = SimdYuvBt601);

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] bgr - a pointer to pixels data of output 24-bit BGR image.
        \param [in] bgrStride - a row size of the bgr image.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuv422pToBgrV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        size_t width, size_t height, uint8_t* bgr, size_t bgrStride, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv444pToBgrV2(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * bgr, size_t bgrStride, SimdYuvType yuvType);

        \short Converts 8-bit planar YUV 4:4:4 image to 24-bit BGR image.

        The input Y, U, V and output BGR images must have the same width and height.

        \note This function has a C++ wrapper: Simd::Yuv444pToBgr(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& bgr, SimdYuvType yuvType = SimdYuvBt601);

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] bgr - a pointer to pixels data of output 24-bit BGR image.
        \param [in] bgrStride - a row size of the bgr image.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuv444pToBgrV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        size_t width, size_t height, uint8_t* bgr, size_t bgrStride, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv420pToBgraV2(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * bgra, size_t bgraStride, uint8_t alpha, SimdYuvType yuvType);

        \short Converts 8-bit planar YUV 4:2:0 image to 32-bit BGRA image.

        The input Y and output BGRA images must have the same width and height.
        The input U and V images must have the same width and height (their width and height are equal to half of the Y width and half of the Y height).
        The image width and height must be even and not less than 2. One U and one V value are used for each 2x2 block of Y values.
        Every output alpha channel is filled with the specified alpha value.

        \note This function has a C++ wrapper: Simd::Yuv420pToBgra(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& bgra, uint8_t alpha, SimdYuvType yuvType).

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] bgra - a pointer to pixels data of output 32-bit BGRA image.
        \param [in] bgraStride - a row size of the bgra image.
        \param [in] alpha - a value of alpha channel.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuv420pToBgraV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        size_t width, size_t height, uint8_t* bgra, size_t bgraStride, uint8_t alpha, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv422pToBgraV2(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * bgra, size_t bgraStride, uint8_t alpha, SimdYuvType yuvType);

        \short Converts 8-bit planar YUV 4:2:2 image to 32-bit BGRA image.

        The input Y and output BGRA images must have the same width and height.
        The input U and V images must have the same width and height (their width is equal to half of the Y width, their height is equal to the Y height).
        The image width must be even and not less than 2. One U and one V value are used for each horizontal pair of Y values.
        Every output alpha channel is filled with the specified alpha value.

        \note This function has a C++ wrapper: Simd::Yuv422pToBgra(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& bgra, uint8_t alpha, SimdYuvType yuvType).

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] bgra - a pointer to pixels data of output 32-bit BGRA image.
        \param [in] bgraStride - a row size of the bgra image.
        \param [in] alpha - a value of alpha channel.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuv422pToBgraV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        size_t width, size_t height, uint8_t* bgra, size_t bgraStride, uint8_t alpha, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv444pToBgraV2(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * bgra, size_t bgraStride, uint8_t alpha, SimdYuvType yuvType);

        \short Converts 8-bit planar YUV 4:4:4 image to 32-bit BGRA image.

        The input Y, U, V and output BGRA images must have the same width and height.
        Every output alpha channel is filled with the specified alpha value.

        \note This function has a C++ wrapper: Simd::Yuv444pToBgra(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& bgra, uint8_t alpha, SimdYuvType yuvType).

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] bgra - a pointer to pixels data of output 32-bit BGRA image.
        \param [in] bgraStride - a row size of the bgra image.
        \param [in] alpha - a value of alpha channel.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuv444pToBgraV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        size_t width, size_t height, uint8_t* bgra, size_t bgraStride, uint8_t alpha, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv444pToHsl(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * hsl, size_t hslStride);

        \short Converts 8-bit planar YUV 4:4:4 image to 24-bit HSL (Hue, Saturation, Lightness) image.

        The input Y, U, V and output HSL images must have the same width and height.
        The function first converts each YUV pixel to RGB with BT.601 studio-range coefficients and then computes HSL components in range [0..255].

        \note This function has a C++ wrapper: Simd::Yuv444pToHsl(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& hsl).

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] hsl - a pointer to pixels data of output 24-bit HSL image.
        \param [in] hslStride - a row size of the hsl image.
    */
    SIMD_API void SimdYuv444pToHsl(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride,
        size_t width, size_t height, uint8_t * hsl, size_t hslStride);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv444pToHsv(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * hsv, size_t hsvStride);

        \short Converts 8-bit planar YUV 4:4:4 image to 24-bit HSV (Hue, Saturation, Value) image.

        The input Y, U, V and output HSV images must have the same width and height.
        The function first converts each YUV pixel to RGB with BT.601 studio-range coefficients and then computes HSV components in range [0..255].

        \note This function has a C++ wrapper: Simd::Yuv444pToHsv(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& hsv).

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] hsv - a pointer to pixels data of output 24-bit HSV image.
        \param [in] hsvStride - a row size of the hsv image.
    */
    SIMD_API void SimdYuv444pToHsv(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride,
        size_t width, size_t height, uint8_t * hsv, size_t hsvStride);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv420pToHue(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * hue, size_t hueStride);

        \short Converts 8-bit planar YUV 4:2:0 image to 8-bit image with Hue component of HSV or HSL color space.

        The input Y and output Hue images must have the same width and height.
        The input U and V images must have the same width and height (their width and height are equal to half of the Y width and half of the Y height).
        The image width and height must be even and not less than 2. One U and one V value are used for each 2x2 block of Y values.
        The function converts YUV to RGB with BT.601 studio-range coefficients and stores hue in range [0..255].

        \note This function has a C++ wrapper: Simd::Yuv420pToHue(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& hue).

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] hue - a pointer to pixels data of output 8-bit Hue image.
        \param [in] hueStride - a row size of the hue image.
    */
    SIMD_API void SimdYuv420pToHue(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride,
        size_t width, size_t height, uint8_t * hue, size_t hueStride);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv444pToHue(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * hue, size_t hueStride);

        \short Converts 8-bit planar YUV 4:4:4 image to 8-bit image with Hue component of HSV or HSL color space.

        The input Y, U, V and output Hue images must have the same width and height.
        The function converts YUV to RGB with BT.601 studio-range coefficients and stores hue in range [0..255].

        \note This function has a C++ wrapper: Simd::Yuv444pToHue(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& hue).

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] hue - a pointer to pixels data of output 8-bit Hue image.
        \param [in] hueStride - a row size of the hue image.
    */
    SIMD_API void SimdYuv444pToHue(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride,
        size_t width, size_t height, uint8_t * hue, size_t hueStride);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv420pToRgbV2(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * rgb, size_t rgbStride, SimdYuvType yuvType);

        \short Converts 8-bit planar YUV 4:2:0 image to 24-bit RGB image.

        The input Y and output RGB images must have the same width and height.
        The input U and V images must have the same width and height (their width and height are equal to half of the Y width and half of the Y height).
        The image width and height must be even and not less than 2. One U and one V value are used for each 2x2 block of Y values.

        \note This function has a C++ wrapper: Simd::Yuv420pToRgb(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& rgb, SimdYuvType yuvType = SimdYuvBt601);

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] rgb - a pointer to pixels data of output 24-bit RGB image.
        \param [in] rgbStride - a row size of the rgb image.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuv420pToRgbV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        size_t width, size_t height, uint8_t* rgb, size_t rgbStride, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv422pToRgbV2(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * rgb, size_t rgbStride, SimdYuvType yuvType);

        \short Converts 8-bit planar YUV 4:2:2 image to 24-bit RGB image.

        The input Y and output RGB images must have the same width and height.
        The input U and V images must have the same width and height (their width is equal to half of the Y width, their height is equal to the Y height).
        The image width must be even and not less than 2. One U and one V value are used for each horizontal pair of Y values.

        \note This function has a C++ wrapper: Simd::Yuv422pToRgb(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& rgb, SimdYuvType yuvType = SimdYuvBt601);

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] rgb - a pointer to pixels data of output 24-bit RGB image.
        \param [in] rgbStride - a row size of the rgb image.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuv422pToRgbV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        size_t width, size_t height, uint8_t* rgb, size_t rgbStride, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv444pToRgbV2(const uint8_t * y, size_t yStride, const uint8_t * u, size_t uStride, const uint8_t * v, size_t vStride, size_t width, size_t height, uint8_t * rgb, size_t rgbStride, SimdYuvType yuvType);

        \short Converts 8-bit planar YUV 4:4:4 image to 24-bit RGB image.

        The input Y, U, V and output RGB images must have the same width and height.

        \note This function has a C++ wrapper: Simd::Yuv444pToRgb(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& rgb, SimdYuvType yuvType = SimdYuvBt601);

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] rgb - a pointer to pixels data of output 24-bit RGB image.
        \param [in] rgbStride - a row size of the rgb image.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuv444pToRgbV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        size_t width, size_t height, uint8_t* rgb, size_t rgbStride, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv444pToRgbaV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride, size_t width, size_t height, uint8_t* rgba, size_t rgbaStride, uint8_t alpha, SimdYuvType yuvType);

        \short Converts 8-bit planar YUV 4:4:4 image to 32-bit RGBA image.

        The input Y, U, V and output RGBA images must have the same width and height.
        Every output alpha channel is filled with the specified alpha value.

        \note This function has a C++ wrapper: Simd::Yuv444pToRgba(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& rgba, uint8_t alpha, SimdYuvType yuvType = SimdYuvBt601);

        \param [in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param [in] yStride - a row size of the y image.
        \param [in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param [in] uStride - a row size of the u image.
        \param [in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param [in] vStride - a row size of the v image.
        \param [in] width - an image width.
        \param [in] height - an image height.
        \param [out] rgba - a pointer to pixels data of output 32-bit RGBA image.
        \param [in] rgbaStride - a row size of the rgba image.
        \param [in] alpha - a value of alpha channel.
        \param [in] yuvType - a type of input YUV image (see description of ::SimdYuvType).
    */
    SIMD_API void SimdYuv444pToRgbaV2(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        size_t width, size_t height, uint8_t* rgba, size_t rgbaStride, uint8_t alpha, SimdYuvType yuvType);

    /*! @ingroup yuv_conversion

        \fn void SimdYuv420pToUyvy422(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride, size_t width, size_t height, uint8_t* uyvy, size_t uyvyStride);

        \short Converts 8-bit planar YUV 4:2:0 image to 16-bit UYVY422 image.

        The input Y and output UYVY422 images must have the same width and height.
        The input U and V images must have the same width and height (their width and height are equal to half of the Y width and half of the Y height).
        The image width and height must be even. One U and one V value are written for each 2x2 block of Y values.
        Output bytes are stored as U0, Y0, V0, Y1 for every horizontal pair of pixels in each row.

        \note This function has a C++ wrapper: Simd::Yuv420pToUyvy422(const View<A>& y, const View<A>& u, const View<A>& v, View<A>& uyvy).

        \param[in] y - a pointer to pixels data of input 8-bit image with Y color plane.
        \param[in] yStride - a row size of the y image.
        \param[in] u - a pointer to pixels data of input 8-bit image with U color plane.
        \param[in] uStride - a row size of the u image.
        \param[in] v - a pointer to pixels data of input 8-bit image with V color plane.
        \param[in] vStride - a row size of the v image.
        \param [in] width - an image width. Width must be even number.
        \param [in] height - an image height. Height must be even number.
        \param [out] uyvy - a pointer to pixels data of output 16-bit UYVY422 image.
        \param [in] uyvyStride - a row size of the UYVY422 image.
    */
    SIMD_API void SimdYuv420pToUyvy422(const uint8_t* y, size_t yStride, const uint8_t* u, size_t uStride, const uint8_t* v, size_t vStride,
        size_t width, size_t height, uint8_t* uyvy, size_t uyvyStride);
#ifdef __cplusplus
}
#endif

#endif
