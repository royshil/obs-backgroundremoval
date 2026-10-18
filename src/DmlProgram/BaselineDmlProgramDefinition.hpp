// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstdint>
#include <variant>

namespace BaselineDml::ProgramDefinition {

// MARK: - Data Types

const std::uint32_t dtUnknown = 0;
const std::uint32_t dtFloat32 = 1;
const std::uint32_t dtFloat16 = 2;
const std::uint32_t dtUInt32 = 3;
const std::uint32_t dtUInt16 = 4;
const std::uint32_t dtUInt8 = 5;
const std::uint32_t dtInt32 = 6;
const std::uint32_t dtInt16 = 7;
const std::uint32_t dtInt8 = 8;
const std::uint32_t dtFloat64 = 9;
const std::uint32_t dtUInt64 = 10;
const std::uint32_t dtInt64 = 11;

// MARK: - Inputs

const std::int64_t kRuntimeInput = -1;

struct InputSpecItem final {
	std::int64_t byteOffset;
	std::uint32_t dataType;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
};

struct InputItem final {
	std::int32_t dataType;
	std::int32_t inputSpecIndex;
};

// MARK: - Basic Operations of ONNX

struct Add final {
	std::int32_t dataType;
	std::int32_t leftIndex;
	std::int32_t rightIndex;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
};

struct AveragePool2d final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::array<std::uint32_t, 2> strides;
	std::array<std::uint32_t, 2> window;
	std::array<std::uint32_t, 2> startPadding;
	std::array<std::uint32_t, 2> endPadding;
	std::array<std::int32_t, 4> shape;
};

struct ClipFloat32 final {
	static constexpr std::int32_t dataType = dtFloat32;
	std::int32_t inputIndex;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
	float minimum;
	float maximum;
};

struct BiasedConv2d final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t filterIndex;
	std::int32_t biasIndex;
	std::array<std::uint32_t, 2> strides;
	std::array<std::uint32_t, 2> dilations;
	std::array<std::uint32_t, 2> startPadding;
	std::array<std::uint32_t, 2> endPadding;
	std::uint32_t groupCount;
	std::array<std::int32_t, 4> shape;
};

struct BiasedConvTranspose2d final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t filterIndex;
	std::int32_t biasIndex;
	std::array<std::uint32_t, 2> strides;
	std::array<std::uint32_t, 2> dilations;
	std::array<std::uint32_t, 2> startPadding;
	std::array<std::uint32_t, 2> endPadding;
	std::uint32_t groupCount;
	std::array<std::int32_t, 4> shape;
};

struct Mul final {
	std::int32_t dataType;
	std::int32_t leftIndex;
	std::int32_t rightIndex;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
};

struct DepthToSpace final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::uint32_t blockSize;
	std::array<std::int32_t, 4> shape;
};

struct Relu final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
};

struct Resize final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t shapeRank;
	std::uint32_t height;
	std::uint32_t width;
	std::array<std::int32_t, 4> shape;
};

struct Sigmoid final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
};

// Native DirectML 1.0 operations used by the generated mask postprocessing graph.
struct Floor final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
};

struct Ceil final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
};

struct Cast final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
};

struct Sub final {
	std::int32_t dataType;
	std::int32_t leftIndex;
	std::int32_t rightIndex;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
};

struct Pad final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::array<std::uint32_t, 4> startPadding;
	std::array<std::uint32_t, 4> endPadding;
	bool edge;
	float value;
	std::array<std::int32_t, 4> shape;
};

struct Gather final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t indicesIndex;
	std::uint32_t axis;
	std::array<std::int32_t, 4> shape;
};

// MARK: - Fused operations

struct BiasedConv2dRelu final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t filterIndex;
	std::int32_t biasIndex;
	std::array<std::uint32_t, 2> strides;
	std::array<std::uint32_t, 2> dilations;
	std::array<std::uint32_t, 2> startPadding;
	std::array<std::uint32_t, 2> endPadding;
	std::uint32_t groupCount;
	std::array<std::int32_t, 4> shape;
};

// MediaPipe Landscape Node 230: FP32 depthwise 3x3, stride/dilation 1,
// padding 1, 16 channels at 72x128, with bias and ReLU.
struct MPLD10DepthwiseConv2dRelu final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t filterIndex;
	std::int32_t biasIndex;
	static constexpr std::array<std::int32_t, 4> shape{1, 16, 72, 128};
};

// The Add -> Clip -> scalar Mul gate of ONNX HardSwish.
struct HardSwishGate final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
};

// FP32 probabilities -> FP32 0/1. No intermediate quantization.
struct MPLD10BinarizeFloat32 final {
	static constexpr std::int32_t dataType = dtFloat32;
	std::int32_t inputIndex;
	std::int32_t thresholdIndex;
	static constexpr std::array<std::int32_t, 4> shape{1, 1, 144, 256};
};

using Onnx11Node = std::variant<AveragePool2d, BiasedConv2d, BiasedConvTranspose2d, DepthToSpace, Pad>;
using Onnx13Node = std::variant<ClipFloat32, Resize, Sigmoid, Floor, Ceil, Cast, Gather>;
using Onnx14Node = std::variant<Add, Mul, Relu, Sub>;
using FusedNode = std::variant<BiasedConv2dRelu, HardSwishGate, MPLD10DepthwiseConv2dRelu, MPLD10BinarizeFloat32>;
using Node = std::variant<InputItem, FusedNode, Onnx11Node, Onnx13Node, Onnx14Node>;

} // namespace BaselineDml::ProgramDefinition
