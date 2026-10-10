// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <variant>

namespace BaselineDml::ProgramDefinition {

using UINT = std::uint32_t;
using Size = std::int32_t;

// Numeric identities match DML_TENSOR_DATA_TYPE; availability depends on feature level.
enum class DataType : UINT {
	Unknown = 0,
	Float32 = 1,
	Float16 = 2,
	UInt32 = 3,
	UInt16 = 4,
	UInt8 = 5,
	Int32 = 6,
	Int16 = 7,
	Int8 = 8,
	Float64 = 9,
	UInt64 = 10,
	Int64 = 11,
};

enum class ItemType : std::uint8_t {
	Invalid = 0,
	RuntimeInput = 1,
	Weight = 2,
};

struct InputSpecItem final {
	DataType dataType;
	ItemType itemType;
	Size shapeRank;
	std::array<UINT, 4> shape;

	// Weight
	std::int64_t byteOffset;
};

struct InputItem final {
	Size inputSpecIndex;
};

struct Add final {
	DataType dataType;
	Size leftIndex;
	Size rightIndex;
	Size shapeRank;
	std::array<UINT, 4> shape;
};

struct AveragePool2d final {
	DataType dataType;
	Size inputIndex;
	std::array<UINT, 2> strides;
	std::array<UINT, 2> window;
	std::array<UINT, 2> startPadding;
	std::array<UINT, 2> endPadding;
	std::array<UINT, 4> shape;
};

struct Clip final {
	DataType dataType;
	Size inputIndex;
	Size shapeRank;
	std::array<UINT, 4> shape;
	float minimum;
	float maximum;
};

struct Concat final {
	DataType dataType;
	Size leftIndex;
	Size rightIndex;
	Size shapeRank;
	UINT axis;
	std::array<UINT, 4> shape;
};

struct Constant final {
	DataType dataType;
	std::int64_t byteOffset;
	Size shapeRank;
	std::array<UINT, 4> shape;
};

struct Conv2d final {
	DataType dataType;
	Size inputIndex;
	Size filterIndex;
	std::array<UINT, 2> strides;
	std::array<UINT, 2> dilations;
	std::array<UINT, 2> startPadding;
	std::array<UINT, 2> endPadding;
	UINT groupCount;
	std::array<UINT, 4> shape;
};

struct BiasedConv2d final {
	DataType dataType;
	Size inputIndex;
	Size filterIndex;
	Size biasIndex;
	std::array<UINT, 2> strides;
	std::array<UINT, 2> dilations;
	std::array<UINT, 2> startPadding;
	std::array<UINT, 2> endPadding;
	UINT groupCount;
	std::array<UINT, 4> shape;
};

struct ConvTranspose2d final {
	DataType dataType;
	Size inputIndex;
	Size filterIndex;
	std::array<UINT, 2> strides;
	std::array<UINT, 2> dilations;
	std::array<UINT, 2> startPadding;
	std::array<UINT, 2> endPadding;
	UINT groupCount;
	std::array<UINT, 4> shape;
};

struct BiasedConvTranspose2d final {
	DataType dataType;
	Size inputIndex;
	Size filterIndex;
	Size biasIndex;
	std::array<UINT, 2> strides;
	std::array<UINT, 2> dilations;
	std::array<UINT, 2> startPadding;
	std::array<UINT, 2> endPadding;
	UINT groupCount;
	std::array<UINT, 4> shape;
};

struct MatMul final {
	DataType dataType;
	Size leftIndex;
	Size rightIndex;
	Size shapeRank;
	std::array<UINT, 4> shape;
};

struct Mul final {
	DataType dataType;
	Size leftIndex;
	Size rightIndex;
	Size shapeRank;
	std::array<UINT, 4> shape;
};

struct Relu final {
	DataType dataType;
	Size inputIndex;
	Size shapeRank;
	std::array<UINT, 4> shape;
};

struct Reshape final {
	DataType dataType;
	Size inputIndex;
	Size shapeRank;
	std::array<UINT, 4> shape;
};

struct Resize final {
	DataType dataType;
	Size inputIndex;
	Size shapeRank;
	UINT height;
	UINT width;
	std::array<UINT, 4> shape;
};

struct Sigmoid final {
	DataType dataType;
	Size inputIndex;
	Size shapeRank;
	std::array<UINT, 4> shape;
};

struct Transpose final {
	DataType dataType;
	Size inputIndex;
	Size shapeRank;
	std::array<UINT, 4> permutation;
};

using Onnx11Node = std::variant<AveragePool2d, BiasedConv2d, BiasedConvTranspose2d>;
using Onnx13Node = std::variant<Clip, Resize, Sigmoid>;
using Onnx14Node = std::variant<Add, Mul, Relu>;
using Node = std::variant<InputItem, Onnx11Node, Onnx13Node, Onnx14Node>;

} // namespace BaselineDml::ProgramDefinition
