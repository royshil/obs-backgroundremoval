// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <exception>
#include <initializer_list>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>
#include <utility>
#include <vector>
#include <cstdint>
#include <variant>

namespace BaselineDml::Definition {

// MARK: - Data Types

constexpr std::uint32_t dtUnknown = 0;
constexpr std::uint32_t dtFloat32 = 1;
constexpr std::uint32_t dtFloat16 = 2;
constexpr std::uint32_t dtUInt32 = 3;
constexpr std::uint32_t dtUInt16 = 4;
constexpr std::uint32_t dtUInt8 = 5;
constexpr std::uint32_t dtInt32 = 6;
constexpr std::uint32_t dtInt16 = 7;
constexpr std::uint32_t dtInt8 = 8;
constexpr std::uint32_t dtFloat64 = 9;
constexpr std::uint32_t dtUInt64 = 10;
constexpr std::uint32_t dtInt64 = 11;

// MARK: - Inputs

struct InputTensorSpec final {
	std::uint32_t dataType = dtUnknown;
	std::uint32_t rank = 0;
	std::array<std::uint32_t, 4> sizes{};
	std::array<std::uint32_t, 4> strides{};
	std::uint64_t size = 0;
	std::uint64_t dataBytes = 0;
};

struct WeightTensorSpec final {
	std::uint32_t dataType = dtUnknown;
	std::uint32_t rank = 0;
	std::array<std::uint32_t, 4> sizes{};
	std::array<std::uint32_t, 4> strides{};
	std::uint64_t size = 0;
	std::uint64_t dataBytes = 0;
	std::int64_t byteOffset = 0;
};

using TensorSpec = std::variant<InputTensorSpec, WeightTensorSpec>;

constexpr auto tensorDataType(const TensorSpec &spec) noexcept -> std::uint32_t
{
	if (const auto *input = std::get_if<InputTensorSpec>(&spec))
		return input->dataType;
	return std::get<WeightTensorSpec>(spec).dataType;
}

constexpr auto tensorRank(const TensorSpec &spec) noexcept -> std::uint32_t
{
	if (const auto *input = std::get_if<InputTensorSpec>(&spec))
		return input->rank;
	return std::get<WeightTensorSpec>(spec).rank;
}

constexpr auto tensorSize(const TensorSpec &spec) noexcept -> std::uint64_t
{
	if (const auto *input = std::get_if<InputTensorSpec>(&spec))
		return input->size;
	return std::get<WeightTensorSpec>(spec).size;
}

constexpr auto tensorDataBytes(const TensorSpec &spec) noexcept -> std::uint64_t
{
	if (const auto *input = std::get_if<InputTensorSpec>(&spec))
		return input->dataBytes;
	return std::get<WeightTensorSpec>(spec).dataBytes;
}

constexpr auto tensorSizes(const TensorSpec &spec) noexcept -> std::span<const std::uint32_t>
{
	if (const auto *input = std::get_if<InputTensorSpec>(&spec))
		return std::span(input->sizes).first(input->rank);
	const auto &weight = std::get<WeightTensorSpec>(spec);
	return std::span(weight.sizes).first(weight.rank);
}

constexpr auto tensorStrides(const TensorSpec &spec) noexcept -> std::span<const std::uint32_t>
{
	if (const auto *input = std::get_if<InputTensorSpec>(&spec))
		return std::span(input->strides).first(input->rank);
	const auto &weight = std::get<WeightTensorSpec>(spec);
	return std::span(weight.strides).first(weight.rank);
}

constexpr auto isWeightTensor(const TensorSpec &spec) noexcept -> bool
{
	return std::holds_alternative<WeightTensorSpec>(spec);
}

constexpr auto tensorByteOffset(const TensorSpec &spec) noexcept -> std::int64_t
{
	if (const auto *weight = std::get_if<WeightTensorSpec>(&spec))
		return weight->byteOffset;
	return 0;
}

struct InputItem final {
	std::int32_t dataType;
	std::int32_t tensorSpecIndex;
};

// MARK: - Basic Operations of ONNX

/**
 * Fixed-shape metadata nodes. They share the input storage and record no GPU dispatch.
 * Reshape preserves element order/count and requires a contiguous input.
 * Expand preserves rank, expands only unit axes, and expresses replication through zero strides.
 * Both reference a preceding node; shape and all resulting tensor metadata are resolved before execution.
 */
struct Reshape final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
};
struct Expand final {
	std::int32_t dataType;
	std::int32_t inputIndex;
	std::int32_t shapeRank;
	std::array<std::int32_t, 4> shape;
};

// Pointwise nodes require identical input/output data types, ranks and shapes; no implicit adaptation.
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

using Onnx11Node = std::variant<AveragePool2d, BiasedConv2d, BiasedConvTranspose2d, DepthToSpace>;
using Onnx13Node = std::variant<ClipFloat32, Resize, Sigmoid, Reshape, Expand>;
using Onnx14Node = std::variant<Add, Mul, Relu>;
using FusedNode = std::variant<BiasedConv2dRelu, HardSwishGate, MPLD10DepthwiseConv2dRelu, MPLD10BinarizeFloat32>;
using Node = std::variant<InputItem, FusedNode, Onnx11Node, Onnx13Node, Onnx14Node>;

// MARK: - Helper functions

constexpr auto dmlTypeName(std::uint32_t type) noexcept -> const char *
{
	switch (type) {
	case dtFloat32:
		return "Float32";
	case dtFloat16:
		return "Float16";
	case dtUInt32:
		return "UInt32";
	case dtUInt16:
		return "UInt16";
	case dtUInt8:
		return "UInt8";
	case dtInt32:
		return "Int32";
	case dtInt16:
		return "Int16";
	case dtInt8:
		return "Int8";
	case dtFloat64:
		return "Float64";
	case dtUInt64:
		return "UInt64";
	case dtInt64:
		return "Int64";
	default:
		return "";
	}
}

constexpr auto dmlElementSize(std::uint32_t type) noexcept -> std::int32_t
{
	switch (type) {
	case dtUInt8:
	case dtInt8:
		return 1;
	case dtFloat16:
	case dtUInt16:
	case dtInt16:
		return 2;
	case dtFloat32:
	case dtUInt32:
	case dtInt32:
		return 4;
	case dtFloat64:
	case dtUInt64:
	case dtInt64:
		return 8;
	default:
		return 0;
	}
}

} // namespace BaselineDml::Definition

namespace BaselineDml {

/** Optional origin of a compilation diagnostic; absent indices have no corresponding source entry. */
struct CompileLocation final {
	std::optional<std::int64_t> nodeIndex;
	std::optional<std::int64_t> tensorSpecIndex;
};

// MARK: - Error Types

/**
 * @brief Compilation failure with a fixed diagnostic and optional source location/type information.
 *
 * Missing information is std::nullopt. result holds a signed 32-bit HRESULT for DirectML API failures.
 * msg must point to a null-terminated string that outlives the exception; named factories use literals.
 */
struct DmlCompilationError final : std::exception {
	const char *msg;
	std::optional<std::int64_t> nodeIndex;
	std::optional<std::int64_t> tensorSpecIndex;
	std::optional<std::uint32_t> dataType;
	std::optional<std::int32_t> result;

	DmlCompilationError(const char *msg, std::optional<std::int64_t> nodeIndex,
			    std::optional<std::int64_t> tensorSpecIndex, std::optional<std::uint32_t> dataType,
			    std::optional<std::int32_t> result = std::nullopt) noexcept
		: msg(msg),
		  nodeIndex(nodeIndex),
		  tensorSpecIndex(tensorSpecIndex),
		  dataType(dataType),
		  result(result)
	{
	}

	auto what() const noexcept -> const char * override { return msg; }
};

namespace Errors {

inline auto invalidShapeRank(std::optional<std::int64_t> nodeIndex = std::nullopt,
			     std::optional<std::int64_t> tensorSpecIndex = std::nullopt) noexcept -> DmlCompilationError
{
	return {"shape rank must be between 1 and 4", nodeIndex, tensorSpecIndex, std::nullopt, std::nullopt};
}

inline auto unknownTensorDataType(std::optional<std::int64_t> nodeIndex = std::nullopt,
				  std::optional<std::int64_t> tensorSpecIndex = std::nullopt,
				  std::optional<std::uint32_t> dataType = std::nullopt) noexcept -> DmlCompilationError
{
	return {"Unknown tensor data type", nodeIndex, tensorSpecIndex, dataType, std::nullopt};
}

inline auto invalidShapeDimension(std::optional<std::int64_t> nodeIndex = std::nullopt,
				  std::optional<std::int64_t> tensorSpecIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"shape dimensions must be positive", nodeIndex, tensorSpecIndex, std::nullopt, std::nullopt};
}

inline auto tensorElementCountOverflow(std::optional<std::int64_t> nodeIndex = std::nullopt,
				       std::optional<std::int64_t> tensorSpecIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"tensor element count overflow (UINT range)", nodeIndex, tensorSpecIndex, std::nullopt, std::nullopt};
}

inline auto unresolvedShapeDimension(std::optional<std::int64_t> nodeIndex = std::nullopt,
				     std::optional<std::int64_t> tensorSpecIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"shape dimensions must be positive and resolved", nodeIndex, tensorSpecIndex, std::nullopt,
		std::nullopt};
}

inline auto weightResourceOverflow(std::optional<std::int64_t> tensorSpecIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"weight exceeds its resource", std::nullopt, tensorSpecIndex, std::nullopt, std::nullopt};
}

inline auto reshapeElementCountMismatch(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"reshape changes the element count", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidClipRange(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept -> DmlCompilationError
{
	return {"ClipFloat32 minimum exceeds maximum", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto depthwiseDataTypeMismatch(std::optional<std::int64_t> nodeIndex = std::nullopt,
				      std::optional<std::uint32_t> dataType = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"MPLD10DepthwiseConv2dRelu requires Float32", nodeIndex, std::nullopt, dataType, std::nullopt};
}

inline auto invalidDepthwiseInputShape(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"MPLD10DepthwiseConv2dRelu input shape", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidDepthwiseFilterShape(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"MPLD10DepthwiseConv2dRelu filter shape", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidDepthwiseBiasShape(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"MPLD10DepthwiseConv2dRelu bias shape", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidBinarizeInputShape(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"MPLD10BinarizeFloat32 input shape", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidBinarizeThresholdShape(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"MPLD10BinarizeFloat32 threshold shape", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidConvolutionRank(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept -> DmlCompilationError
{
	return {"Conv2d requires rank-4 input and filter", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidConvolutionGroupCount(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"Conv2d group count must be positive", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidConvolutionStride(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"Conv2d stride and dilation must be positive", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidConvolutionBiasRank(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"Conv2d bias must be rank 1", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidPoolRank(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept -> DmlCompilationError
{
	return {"AveragePool2d requires rank-4 input", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidPoolStride(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept -> DmlCompilationError
{
	return {"AveragePool2d stride and window must be positive", nodeIndex, std::nullopt, std::nullopt,
		std::nullopt};
}

inline auto invalidResizeRank(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept -> DmlCompilationError
{
	return {"Resize requires rank 4", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidResizeScale(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept -> DmlCompilationError
{
	return {"Resize scales must be positive", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto unsupportedTensorDataType(std::optional<std::int64_t> nodeIndex = std::nullopt,
				      std::optional<std::int64_t> tensorSpecIndex = std::nullopt,
				      std::optional<std::uint32_t> dataType = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"tensor data type is unsupported by the DirectML device", nodeIndex, tensorSpecIndex, dataType,
		std::nullopt};
}

inline auto inputDataTypeMismatch(std::optional<std::int64_t> nodeIndex = std::nullopt,
				  std::optional<std::uint32_t> dataType = std::nullopt) noexcept -> DmlCompilationError
{
	return {"input and operator data types must match; no implicit conversion", nodeIndex, std::nullopt, dataType,
		std::nullopt};
}

inline auto invalidBroadcastRank(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept -> DmlCompilationError
{
	return {"broadcast rank is invalid", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidBroadcastShape(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept -> DmlCompilationError
{
	return {"broadcast shape is invalid", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidNodeDataType(std::optional<std::int64_t> nodeIndex = std::nullopt,
				std::optional<std::uint32_t> dataType = std::nullopt) noexcept -> DmlCompilationError
{
	return {"Unknown or invalid node data type", nodeIndex, std::nullopt, dataType, std::nullopt};
}

inline auto invalidNodeReference(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept -> DmlCompilationError
{
	return {"input must reference a preceding node", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto invalidTensorSpecIndex(std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept -> DmlCompilationError
{
	return {"input specification index is out of range", nodeIndex, std::nullopt, std::nullopt, std::nullopt};
}

inline auto inputSpecDataTypeMismatch(std::optional<std::int64_t> nodeIndex = std::nullopt,
				      std::optional<std::uint32_t> dataType = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"InputItem and TensorSpec data types must match", nodeIndex, std::nullopt, dataType, std::nullopt};
}

inline auto featureSupportError(std::int32_t result, std::optional<std::int64_t> nodeIndex = std::nullopt,
				std::optional<std::int64_t> tensorSpecIndex = std::nullopt,
				std::optional<std::uint32_t> dataType = std::nullopt) noexcept -> DmlCompilationError
{
	return {"CheckFeatureSupport failed", nodeIndex, tensorSpecIndex, dataType, result};
}

inline auto operatorCreationError(std::int32_t result, std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"CreateOperator (operator/type support) failed", nodeIndex, std::nullopt, std::nullopt, result};
}

inline auto operatorCompilationError(std::int32_t result, std::optional<std::int64_t> nodeIndex = std::nullopt) noexcept
	-> DmlCompilationError
{
	return {"CompileOperator failed", nodeIndex, std::nullopt, std::nullopt, result};
}

} // namespace Errors

template<typename Spec>
constexpr void setContiguousShape(Spec &tensor, std::span<const std::uint32_t> shape, CompileLocation location = {})
{
	if (!(!shape.empty() && shape.size() <= 4))
		throw Errors::invalidShapeRank(location.nodeIndex, location.tensorSpecIndex);
	const auto bytesPerElement = Definition::dmlElementSize(tensor.dataType);
	if (!(bytesPerElement != 0))
		throw Errors::unknownTensorDataType(location.nodeIndex, location.tensorSpecIndex, tensor.dataType);
	std::uint64_t count = 1;
	for (const auto dimension : shape) {
		if (!(dimension > 0))
			throw Errors::invalidShapeDimension(location.nodeIndex, location.tensorSpecIndex);
		if (!(count <= std::numeric_limits<std::uint32_t>::max() / dimension))
			throw Errors::tensorElementCountOverflow(location.nodeIndex, location.tensorSpecIndex);
		count *= dimension;
	}

	// The validated element count bounds every stride and count * bytesPerElement (at most eight).
	tensor.rank = static_cast<std::uint32_t>(shape.size());
	tensor.sizes.fill(0);
	tensor.strides.fill(0);
	std::copy(shape.begin(), shape.end(), tensor.sizes.begin());
	std::uint32_t stride = 1;
	for (std::size_t index = shape.size(); index-- > 0;) {
		tensor.strides[index] = stride;
		stride *= shape[index];
	}
	tensor.dataBytes = count * bytesPerElement;
	tensor.size = (tensor.dataBytes + 3) / 4 * 4;
}

// Metadata-only expansion preserves the allocation size and weight byte offset.
template<typename Spec>
constexpr void setExpandedShape(Spec &spec, std::span<const std::uint32_t> shape,
				std::span<const std::uint32_t> inputSizes, std::span<const std::uint32_t> inputStrides,
				CompileLocation location)
{
	spec.rank = static_cast<std::uint32_t>(shape.size());
	spec.sizes.fill(0);
	std::copy(shape.begin(), shape.end(), spec.sizes.begin());
	spec.strides.fill(0);
	for (std::size_t i = 0; i < shape.size(); ++i) {
		if (!(inputSizes[i] == 1 || inputSizes[i] == shape[i]))
			throw Errors::invalidBroadcastShape(location.nodeIndex);
		spec.strides[i] = inputSizes[i] == 1 ? 0 : inputStrides[i];
	}
}

constexpr auto resolvedShape(std::span<const std::int32_t> shape, CompileLocation location)
	-> std::vector<std::uint32_t>
{
	std::vector<std::uint32_t> sizes;
	sizes.reserve(shape.size());
	for (const auto dimension : shape) {
		if (!(dimension > 0))
			throw Errors::unresolvedShapeDimension(location.nodeIndex, location.tensorSpecIndex);
		sizes.push_back(static_cast<std::uint32_t>(dimension));
	}
	return sizes;
}

constexpr auto rankedShape(std::span<const std::int32_t> shape, std::int32_t rank, CompileLocation location)
	-> std::vector<std::uint32_t>
{
	if (rank <= 0 || !std::cmp_less_equal(rank, shape.size()))
		throw Errors::invalidShapeRank(location.nodeIndex, location.tensorSpecIndex);
	return resolvedShape(shape.first(static_cast<std::size_t>(rank)), location);
}

namespace Definition {

constexpr void validateTensorSpec(const TensorSpec &spec, CompileLocation location = {})
{
	if (tensorRank(spec) == 0 || tensorRank(spec) > 4)
		throw Errors::invalidShapeRank(location.nodeIndex, location.tensorSpecIndex);
	if (dmlElementSize(tensorDataType(spec)) == 0)
		throw Errors::unknownTensorDataType(location.nodeIndex, location.tensorSpecIndex, tensorDataType(spec));
}

struct StorageDefinition final {
	std::uint64_t size = 0;
};
enum class ProgramShader { None, MPLD10DepthwiseConv2dRelu, MPLD10BinarizeFloat32 };

// Temporary owning data used during constexpr evaluation or by hand-written test definitions.
struct PreparedStep final {
	std::vector<std::int64_t> inputs;
	std::vector<std::int64_t> barriersBefore;
	std::int64_t output = -1;
	ProgramShader shader = ProgramShader::None;
	std::int64_t nodeIndex = -1;
};
struct PreparedParams final {
	std::vector<TensorSpec> tensors;
	std::vector<std::int64_t> placements;
	std::vector<StorageDefinition> storages;
	std::vector<PreparedStep> steps;
	std::vector<std::int64_t> inputTensors;
	std::int64_t outputTensor = -1;
	std::uint64_t weightBytes = 0;
};

class ModelParamsBuilder final {
	using Step = PreparedStep;
	using CompiledShader = ProgramShader;

public:
	constexpr ModelParamsBuilder(std::span<const TensorSpec> specs, std::size_t weightBytes)
		: specs_(specs),
		  sourceWeightBytes_(weightBytes)
	{
	}

	constexpr auto operator()(const InputItem &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.tensorSpecIndex < 0 || !std::cmp_less(node.tensorSpecIndex, specs_.size()))
			throw Errors::invalidTensorSpecIndex(location_.nodeIndex);
		auto tensor = specs_[node.tensorSpecIndex];
		validateTensorSpec(tensor, {location_.nodeIndex, node.tensorSpecIndex});
		if (tensorDataType(tensor) != dataType)
			throw Errors::inputSpecDataTypeMismatch(location_.nodeIndex, dataType);
		const auto index = static_cast<std::int64_t>(tensors_.size());
		std::int64_t placement = -1;
		if (isWeightTensor(tensor)) {
			if (tensorByteOffset(tensor) < 0)
				throw std::invalid_argument("invalid input byte offset");
			if (!std::cmp_less_equal(tensorByteOffset(tensor), sourceWeightBytes_) ||
			    tensorSize(tensor) >
				    sourceWeightBytes_ - static_cast<std::size_t>(tensorByteOffset(tensor)))
				throw Errors::weightResourceOverflow(node.tensorSpecIndex);
			if (tensorByteOffset(tensor) % 16 != 0)
				throw std::invalid_argument("weight byte offset must be 16-byte aligned");
			placement = -1;
		} else {
			placement = static_cast<std::int64_t>(storages_.size());
			storages_.push_back({tensorSize(tensor)});
			inputTensors_.push_back(index);
		}
		tensors_.push_back(std::move(tensor));
		placements_.push_back(placement);
		return index;
	}

	constexpr auto operator()(const BiasedConv2dRelu &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		if (node.filterIndex < 0 || !std::cmp_less(node.filterIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		if (node.biasIndex < 0 || !std::cmp_less(node.biasIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		auto input = node.inputIndex;
		auto filter = node.filterIndex;
		auto bias = node.biasIndex;
		validateInputType(input, dataType);
		validateInputType(filter, dataType);
		validateInputType(bias, dataType);
		if (!(tensorRank(tensors_.at(input)) == 4 && tensorRank(tensors_.at(filter)) == 4))
			throw Errors::invalidConvolutionRank(location_.nodeIndex);
		if (!(node.groupCount > 0))
			throw Errors::invalidConvolutionGroupCount(location_.nodeIndex);
		for (std::size_t i = 0; i < 2; ++i)
			if (!(node.strides[i] > 0 && node.dilations[i] > 0))
				throw Errors::invalidConvolutionStride(location_.nodeIndex);
		const auto &biasTensor = tensors_.at(bias);
		if (!(tensorRank(biasTensor) == 1))
			throw Errors::invalidConvolutionBiasRank(location_.nodeIndex);
		auto outputShape = resolvedShape(node.shape, location_);
		auto output = createTensor(dataType, outputShape);
		compile({input, filter, bias}, output);
		return output;
	}

	constexpr auto operator()(const HardSwishGate &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		auto input = node.inputIndex;
		auto shape = rankedShape(node.shape, node.shapeRank, location_);
		validateInputType(input, dataType);
		validateInputShape(input, shape);
		auto output = createTensor(dataType, shape);
		compile({input}, output);
		return output;
	}

	constexpr auto operator()(const MPLD10DepthwiseConv2dRelu &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		if (node.filterIndex < 0 || !std::cmp_less(node.filterIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		if (node.biasIndex < 0 || !std::cmp_less(node.biasIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		auto input = node.inputIndex;
		auto filter = node.filterIndex;
		auto bias = node.biasIndex;
		if (!(dataType == dtFloat32))
			throw Errors::depthwiseDataTypeMismatch(location_.nodeIndex, dataType);
		for (const auto id : {input, filter, bias})
			validateInputType(id, dataType);
		if (!(std::ranges::equal(tensorSizes(tensors_.at(input)), std::vector<std::uint32_t>{1, 16, 72, 128})))
			throw Errors::invalidDepthwiseInputShape(location_.nodeIndex);
		if (!(isWeightTensor(tensors_.at(filter)) &&
		      std::ranges::equal(tensorSizes(tensors_.at(filter)), std::vector<std::uint32_t>{16, 1, 3, 3})))
			throw Errors::invalidDepthwiseFilterShape(location_.nodeIndex);
		if (!(isWeightTensor(tensors_.at(bias)) &&
		      std::ranges::equal(tensorSizes(tensors_.at(bias)), std::vector<std::uint32_t>{16})))
			throw Errors::invalidDepthwiseBiasShape(location_.nodeIndex);
		auto outputShape = resolvedShape(MPLD10DepthwiseConv2dRelu::shape, location_);
		auto output = createTensor(dataType, outputShape);
		Step step;
		step.nodeIndex = *location_.nodeIndex;
		step.inputs = {input, filter, bias};
		step.output = output;
		step.shader = CompiledShader::MPLD10DepthwiseConv2dRelu;
		steps_.push_back(std::move(step));
		return output;
	}

	constexpr auto operator()(const MPLD10BinarizeFloat32 &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		if (node.thresholdIndex < 0 || !std::cmp_less(node.thresholdIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		auto input = node.inputIndex;
		auto threshold = node.thresholdIndex;
		validateInputType(input, dtFloat32);
		validateInputType(threshold, dtFloat32);
		if (!(std::ranges::equal(tensorSizes(tensors_.at(input)), std::vector<std::uint32_t>{1, 1, 144, 256})))
			throw Errors::invalidBinarizeInputShape(location_.nodeIndex);
		if (!(tensorDataBytes(tensors_.at(threshold)) == sizeof(float)))
			throw Errors::invalidBinarizeThresholdShape(location_.nodeIndex);
		auto outputShape = resolvedShape(MPLD10BinarizeFloat32::shape, location_);
		auto output = createTensor(dtFloat32, outputShape);
		Step step;
		step.nodeIndex = *location_.nodeIndex;
		step.inputs = {input, threshold};
		step.output = output;
		step.shader = CompiledShader::MPLD10BinarizeFloat32;
		steps_.push_back(std::move(step));
		return output;
	}

	constexpr auto operator()(const AveragePool2d &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		auto input = node.inputIndex;
		validateInputType(input, dataType);
		if (!(tensorRank(tensors_.at(input)) == 4))
			throw Errors::invalidPoolRank(location_.nodeIndex);
		for (std::size_t i = 0; i < 2; ++i)
			if (!(node.strides[i] > 0 && node.window[i] > 0))
				throw Errors::invalidPoolStride(location_.nodeIndex);
		auto outputShape = resolvedShape(node.shape, location_);
		auto output = createTensor(dataType, outputShape);
		compile({input}, output);
		return output;
	}

	constexpr auto operator()(const BiasedConv2d &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		if (node.filterIndex < 0 || !std::cmp_less(node.filterIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		if (node.biasIndex < 0 || !std::cmp_less(node.biasIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		auto input = node.inputIndex;
		auto filter = node.filterIndex;
		auto bias = node.biasIndex;
		validateInputType(input, dataType);
		validateInputType(filter, dataType);
		validateInputType(bias, dataType);
		if (!(tensorRank(tensors_.at(input)) == 4 && tensorRank(tensors_.at(filter)) == 4))
			throw Errors::invalidConvolutionRank(location_.nodeIndex);
		if (!(node.groupCount > 0))
			throw Errors::invalidConvolutionGroupCount(location_.nodeIndex);
		for (std::size_t i = 0; i < 2; ++i)
			if (!(node.strides[i] > 0 && node.dilations[i] > 0))
				throw Errors::invalidConvolutionStride(location_.nodeIndex);
		const auto &biasTensor = tensors_.at(bias);
		if (!(tensorRank(biasTensor) == 1))
			throw Errors::invalidConvolutionBiasRank(location_.nodeIndex);
		auto outputShape = resolvedShape(node.shape, location_);
		auto output = createTensor(dataType, outputShape);
		compile({input, filter, bias}, output);
		return output;
	}

	constexpr auto operator()(const BiasedConvTranspose2d &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		if (node.filterIndex < 0 || !std::cmp_less(node.filterIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		if (node.biasIndex < 0 || !std::cmp_less(node.biasIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		auto input = node.inputIndex;
		auto filter = node.filterIndex;
		auto bias = node.biasIndex;
		validateInputType(input, dataType);
		validateInputType(filter, dataType);
		validateInputType(bias, dataType);
		if (!(tensorRank(tensors_.at(input)) == 4 && tensorRank(tensors_.at(filter)) == 4))
			throw Errors::invalidConvolutionRank(location_.nodeIndex);
		if (!(node.groupCount > 0))
			throw Errors::invalidConvolutionGroupCount(location_.nodeIndex);
		for (std::size_t i = 0; i < 2; ++i)
			if (!(node.strides[i] > 0 && node.dilations[i] > 0))
				throw Errors::invalidConvolutionStride(location_.nodeIndex);
		const auto &biasTensor = tensors_.at(bias);
		if (!(tensorRank(biasTensor) == 1))
			throw Errors::invalidConvolutionBiasRank(location_.nodeIndex);
		auto outputShape = resolvedShape(node.shape, location_);
		auto output = createTensor(dataType, outputShape);
		compile({input, filter, bias}, output);
		return output;
	}

	constexpr auto operator()(const DepthToSpace &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		auto input = node.inputIndex;
		auto outputShape = resolvedShape(node.shape, location_);
		auto output = createTensor(dataType, outputShape);
		compile({input}, output);
		return output;
	}

	constexpr auto operator()(const ClipFloat32 &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		auto input = node.inputIndex;
		if (!(node.minimum <= node.maximum))
			throw Errors::invalidClipRange(location_.nodeIndex);
		auto shape = rankedShape(node.shape, node.shapeRank, location_);
		validateInputType(input, dataType);
		validateInputShape(input, shape);
		auto output = createTensor(dataType, shape);
		compile({input}, output);
		return output;
	}

	constexpr auto operator()(const Resize &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		auto input = node.inputIndex;
		validateInputType(input, dataType);
		if (!(node.shapeRank == 4 && tensorRank(tensors_.at(input)) == 4))
			throw Errors::invalidResizeRank(location_.nodeIndex);
		if (!(node.height > 0 && node.width > 0))
			throw Errors::invalidResizeScale(location_.nodeIndex);
		auto shape = rankedShape(node.shape, node.shapeRank, location_);
		auto output = createTensor(dataType, shape);
		compile({input}, output);
		return output;
	}

	constexpr auto operator()(const Sigmoid &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		const auto shape = rankedShape(node.shape, node.shapeRank, {location_.nodeIndex, std::nullopt});
		auto input = node.inputIndex;
		validateInputType(input, dataType);
		validateInputShape(input, shape);
		auto output = createTensor(dataType, shape);
		compile({input}, output);
		return output;
	}

	constexpr auto operator()(const Add &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.leftIndex < 0 || !std::cmp_less(node.leftIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		if (node.rightIndex < 0 || !std::cmp_less(node.rightIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		const auto shape = rankedShape(node.shape, node.shapeRank, {location_.nodeIndex, std::nullopt});
		auto left = node.leftIndex;
		auto right = node.rightIndex;
		auto output = createTensor(dataType, shape);
		validateInputType(left, dataType);
		validateInputType(right, dataType);
		validateInputShape(left, shape);
		validateInputShape(right, shape);
		compile({left, right}, output);
		return output;
	}

	constexpr auto operator()(const Mul &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.leftIndex < 0 || !std::cmp_less(node.leftIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		if (node.rightIndex < 0 || !std::cmp_less(node.rightIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		const auto shape = rankedShape(node.shape, node.shapeRank, {location_.nodeIndex, std::nullopt});
		auto left = node.leftIndex;
		auto right = node.rightIndex;
		auto output = createTensor(dataType, shape);
		validateInputType(left, dataType);
		validateInputType(right, dataType);
		validateInputShape(left, shape);
		validateInputShape(right, shape);
		compile({left, right}, output);
		return output;
	}

	constexpr auto operator()(const Relu &node) -> std::int64_t
	{
		const auto dataType = static_cast<std::uint32_t>(node.dataType);
		if (node.dataType <= 0 || dataType > dtInt64)
			throw Errors::invalidNodeDataType(location_.nodeIndex, dataType);
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		const auto shape = rankedShape(node.shape, node.shapeRank, {location_.nodeIndex, std::nullopt});
		auto input = node.inputIndex;
		validateInputType(input, dataType);
		validateInputShape(input, shape);
		auto output = createTensor(dataType, shape);
		compile({input}, output);
		return output;
	}

	constexpr auto operator()(const Reshape &node) -> std::int64_t
	{
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		const auto input = node.inputIndex;
		validateInputType(input, static_cast<std::uint32_t>(node.dataType));
		const auto &tensor = tensors_.at(input);
		const auto contiguous = makeTensor(tensorDataType(tensor), tensorSizes(tensor));
		if (!std::ranges::equal(tensorStrides(tensor), std::span(contiguous.strides).first(contiguous.rank)))
			throw std::invalid_argument("Reshape requires a contiguous input");
		return reshape(input, rankedShape(node.shape, node.shapeRank, location_));
	}

	constexpr auto operator()(const Expand &node) -> std::int64_t
	{
		if (node.inputIndex < 0 || !std::cmp_less(node.inputIndex, tensors_.size()))
			throw Errors::invalidNodeReference(location_.nodeIndex);
		const auto input = node.inputIndex;
		const auto shape = rankedShape(node.shape, node.shapeRank, location_);
		if (tensorRank(tensors_.at(input)) != shape.size())
			throw std::invalid_argument("Expand requires matching ranks; insert Reshape first");
		return broadcast(input, static_cast<std::uint32_t>(node.dataType), shape);
	}

	constexpr auto operator()(const FusedNode &nodes) -> std::int64_t { return std::visit(*this, nodes); }

	constexpr auto operator()(const Onnx11Node &nodes) -> std::int64_t { return std::visit(*this, nodes); }

	constexpr auto operator()(const Onnx13Node &nodes) -> std::int64_t { return std::visit(*this, nodes); }

	constexpr auto operator()(const Onnx14Node &nodes) -> std::int64_t { return std::visit(*this, nodes); }

	constexpr auto reshape(std::int64_t input, std::span<const std::uint32_t> shape) -> std::int64_t
	{
		auto tensor = tensors_.at(input);
		const auto bytes = tensorDataBytes(tensor);
		if (auto *spec = std::get_if<InputTensorSpec>(&tensor))
			setContiguousShape(*spec, shape, location_);
		else
			setContiguousShape(std::get<WeightTensorSpec>(tensor), shape, location_);
		if (!(bytes == tensorDataBytes(tensor)))
			throw Errors::reshapeElementCountMismatch(location_.nodeIndex);
		tensors_.push_back(std::move(tensor));
		placements_.push_back(placements_.at(input));
		return static_cast<std::int64_t>(tensors_.size() - 1);
	}

	constexpr auto finish(std::int64_t output) -> PreparedParams
	{
		return {std::move(tensors_), std::move(placements_),   std::move(storages_),
			std::move(steps_),   std::move(inputTensors_), output,
			sourceWeightBytes_};
	}

	constexpr void beginNode(std::int64_t index) noexcept
	{
		location_ = {index, std::nullopt};
		nodeStepBegin_ = steps_.size();
	}

	constexpr void addBarrierStep(std::vector<std::int64_t> barriers)
	{
		if (barriers.empty())
			return;
		Step step;
		step.nodeIndex = *location_.nodeIndex;
		step.barriersBefore = std::move(barriers);
		steps_.push_back(std::move(step));
	}

	constexpr void setNodeBarriers(std::vector<std::int64_t> barriers)
	{
		steps_.at(nodeStepBegin_).barriersBefore = std::move(barriers);
		std::vector<std::int64_t> pending;
		for (auto index = nodeStepBegin_; index < steps_.size(); ++index) {
			auto &step = steps_[index];
			for (const auto input : step.inputs) {
				const auto producer =
					std::find_if(pending.begin(), pending.end(), [&](std::int64_t id) {
						return !isWeightTensor(tensors_.at(input)) &&
						       placements_.at(id) == placements_.at(input);
					});
				if (producer != pending.end()) {
					step.barriersBefore.push_back(*producer);
					pending.erase(producer);
				}
			}
			pending.push_back(step.output);
		}
	}

	constexpr auto makeTensor(std::uint32_t type, std::span<const std::uint32_t> shape) -> InputTensorSpec
	{
		InputTensorSpec tensor;
		tensor.dataType = type;
		setContiguousShape(tensor, shape);
		return tensor;
	}

	constexpr auto createTensor(std::uint32_t type, std::span<const std::uint32_t> shape) -> std::int64_t
	{
		auto tensor = makeTensor(type, shape);
		placements_.push_back(static_cast<std::int64_t>(storages_.size()));
		storages_.push_back({tensor.size});
		tensors_.push_back(std::move(tensor));
		return static_cast<std::int64_t>(tensors_.size() - 1);
	}

	constexpr auto validateInputType(std::int64_t id, std::uint32_t type) const -> void
	{
		if (!(tensorDataType(tensors_.at(id)) == type))
			throw Errors::inputDataTypeMismatch(location_.nodeIndex, type);
	}

	constexpr void validateInputShape(std::int64_t id, std::span<const std::uint32_t> shape) const
	{
		if (!std::ranges::equal(tensorSizes(tensors_.at(id)), shape))
			throw std::invalid_argument("pointwise input and output shapes must match");
	}

	constexpr auto broadcast(std::int64_t id, std::uint32_t type, std::span<const std::uint32_t> shape)
		-> std::int64_t
	{
		validateInputType(id, type);
		const auto &input = tensors_.at(id);
		if (!(tensorRank(input) == shape.size()))
			throw Errors::invalidBroadcastRank(location_.nodeIndex);
		auto tensor = input;
		if (auto *spec = std::get_if<InputTensorSpec>(&tensor))
			setExpandedShape(*spec, shape, tensorSizes(input), tensorStrides(input), location_);
		else
			setExpandedShape(std::get<WeightTensorSpec>(tensor), shape, tensorSizes(input),
					 tensorStrides(input), location_);
		// Broadcasting changes the logical shape, but not the underlying allocation.
		tensors_.push_back(std::move(tensor));
		placements_.push_back(placements_.at(id));
		return static_cast<std::int64_t>(tensors_.size() - 1);
	}

	constexpr auto compile(std::initializer_list<std::int64_t> inputs, std::int64_t output) -> std::int64_t
	{
		Step step;
		step.inputs.assign(inputs.begin(), inputs.end());
		step.output = output;
		step.nodeIndex = *location_.nodeIndex;
		steps_.push_back(std::move(step));
		return output;
	}

	std::span<const TensorSpec> specs_;
	std::vector<std::int64_t> inputTensors_;
	CompileLocation location_;
	std::size_t sourceWeightBytes_;
	std::vector<TensorSpec> tensors_;
	std::vector<std::int64_t> placements_;
	std::vector<StorageDefinition> storages_;
	std::vector<PreparedStep> steps_;
	std::size_t nodeStepBegin_ = 0;
};

/** References read by a node, in the operation's operand order. */
struct NodeInputs final {
	std::array<std::int32_t, 3> indices{-1, -1, -1};
};
constexpr auto isViewNode(const Node &node) noexcept -> bool
{
	const auto *group = std::get_if<Onnx13Node>(&node);
	return group && (std::holds_alternative<Reshape>(*group) || std::holds_alternative<Expand>(*group));
}

struct NodeInputsVisitor final {
	constexpr auto operator()(const InputItem &) const noexcept -> NodeInputs { return {}; }
	constexpr auto operator()(const BiasedConv2d &n) const noexcept -> NodeInputs
	{
		return {{n.inputIndex, n.filterIndex, n.biasIndex}};
	}
	constexpr auto operator()(const BiasedConv2dRelu &n) const noexcept -> NodeInputs
	{
		return {{n.inputIndex, n.filterIndex, n.biasIndex}};
	}
	constexpr auto operator()(const BiasedConvTranspose2d &n) const noexcept -> NodeInputs
	{
		return {{n.inputIndex, n.filterIndex, n.biasIndex}};
	}
	constexpr auto operator()(const MPLD10DepthwiseConv2dRelu &n) const noexcept -> NodeInputs
	{
		return {{n.inputIndex, n.filterIndex, n.biasIndex}};
	}
	constexpr auto operator()(const MPLD10BinarizeFloat32 &n) const noexcept -> NodeInputs
	{
		return {{n.inputIndex, n.thresholdIndex, -1}};
	}
	constexpr auto operator()(const AveragePool2d &n) const noexcept -> NodeInputs
	{
		return {{n.inputIndex, -1, -1}};
	}
	constexpr auto operator()(const DepthToSpace &n) const noexcept -> NodeInputs
	{
		return {{n.inputIndex, -1, -1}};
	}
	constexpr auto operator()(const ClipFloat32 &n) const noexcept -> NodeInputs
	{
		return {{n.inputIndex, -1, -1}};
	}
	constexpr auto operator()(const Resize &n) const noexcept -> NodeInputs { return {{n.inputIndex, -1, -1}}; }
	constexpr auto operator()(const Reshape &n) const noexcept -> NodeInputs { return {{n.inputIndex, -1, -1}}; }
	constexpr auto operator()(const Expand &n) const noexcept -> NodeInputs { return {{n.inputIndex, -1, -1}}; }
	constexpr auto operator()(const Relu &n) const noexcept -> NodeInputs { return {{n.inputIndex, -1, -1}}; }
	constexpr auto operator()(const Sigmoid &n) const noexcept -> NodeInputs { return {{n.inputIndex, -1, -1}}; }
	constexpr auto operator()(const HardSwishGate &n) const noexcept -> NodeInputs
	{
		return {{n.inputIndex, -1, -1}};
	}
	constexpr auto operator()(const Add &n) const noexcept -> NodeInputs
	{
		return {{n.leftIndex, n.rightIndex, -1}};
	}
	constexpr auto operator()(const Mul &n) const noexcept -> NodeInputs
	{
		return {{n.leftIndex, n.rightIndex, -1}};
	}
	constexpr auto operator()(const FusedNode &n) const -> NodeInputs { return std::visit(*this, n); }
	constexpr auto operator()(const Onnx11Node &n) const -> NodeInputs { return std::visit(*this, n); }
	constexpr auto operator()(const Onnx13Node &n) const -> NodeInputs { return std::visit(*this, n); }
	constexpr auto operator()(const Onnx14Node &n) const -> NodeInputs { return std::visit(*this, n); }
};

/**
 * Places one UAV barrier at the first GPU read of each computed output. InputItem is not
 * a GPU write. Reshape/Expand preserve writer identity and consume no barriers themselves.
 * Duplicate operand references share one barrier. Optional early batching
 * only moves a barrier after its producer and no later than its first consumer.
 * The expected count is supplied by the generator and verified during constant evaluation.
 */
template<std::size_t BarrierCount>
constexpr auto makeUavBarriers(std::span<const Node> nodes, bool earlyBatched = false)
	-> std::array<std::pair<std::int32_t, std::int32_t>, BarrierCount>
{
	if (nodes.size() > std::numeric_limits<std::int32_t>::max())
		throw std::invalid_argument("invalid node table");
	std::vector<bool> pending(nodes.size(), false);
	std::vector<std::int32_t> writers(nodes.size(), -1);
	std::array<std::pair<std::int32_t, std::int32_t>, BarrierCount> barriers{};
	std::size_t count = 0;
	for (std::int32_t index = 0; std::cmp_less(index, nodes.size()); ++index) {
		const auto inputs = std::visit(NodeInputsVisitor{}, nodes[index]);
		if (isViewNode(nodes[index])) {
			const auto input = inputs.indices[0];
			if (input < 0 || input >= index)
				throw std::invalid_argument("input must reference a preceding node");
			writers[index] = writers[input];
			continue;
		}
		for (const auto input : inputs.indices) {
			if (input == -1)
				continue;
			if (input < 0 || input >= index)
				throw std::invalid_argument("input must reference a preceding node");
			const auto producer = writers[input];
			if (producer >= 0 && pending[producer]) {
				if (count == BarrierCount)
					throw std::invalid_argument("UAV barrier count differs");
				barriers[count++] = {index, producer};
				pending[producer] = false;
			}
		}
		pending[index] = !std::holds_alternative<InputItem>(nodes[index]);
		writers[index] = pending[index] ? index : -1;
	}
	if (count != BarrierCount)
		throw std::invalid_argument("UAV barrier count differs");
	if (earlyBatched) {
		const auto baseline = barriers;
		for (auto &entry : barriers)
			for (const auto &candidate : baseline)
				if (entry.second < candidate.first && candidate.first <= entry.first) {
					entry.first = candidate.first;
					break;
				}
		// Stable insertion sort preserves operand order within each boundary.
		for (std::size_t i = 1; i < barriers.size(); ++i) {
			const auto entry = barriers[i];
			auto j = i;
			while (j > 0 && barriers[j - 1].first > entry.first) {
				barriers[j] = barriers[j - 1];
				--j;
			}
			barriers[j] = entry;
		}
	}
	return barriers;
}

constexpr auto isUavBarrierBoundaryOutOfRange(std::size_t nodeCount) noexcept
{
	return [nodeCount](const std::pair<std::int32_t, std::int32_t> &barrier) noexcept {
		return barrier.first < 0 || !std::cmp_less(barrier.first, nodeCount);
	};
}

constexpr auto hasInvalidUavBarrierProducer(const std::pair<std::int32_t, std::int32_t> &barrier) noexcept -> bool
{
	return barrier.second < 0 || barrier.second >= barrier.first;
}

/**
 * Resolves all device-independent metadata. In a constant expression an invalid definition
 * is a compile error; runtime callers receive the same validation errors.
 */
constexpr auto prepareModelParams(std::span<const TensorSpec> specs, std::span<const Node> nodes,
				  std::span<const std::pair<std::int32_t, std::int32_t>> barriers,
				  std::int32_t outputIndex, std::size_t sourceWeightBytes) -> PreparedParams
{
	if (specs.size() > std::numeric_limits<std::int32_t>::max())
		throw std::invalid_argument("invalid input specification table");
	if (nodes.empty() || nodes.size() > std::numeric_limits<std::int32_t>::max())
		throw std::invalid_argument("invalid node table");
	if (outputIndex < 0 || !std::cmp_less(outputIndex, nodes.size()))
		throw std::invalid_argument("output must reference a preceding node");
	if (!std::ranges::is_sorted(barriers, {}, &std::pair<std::int32_t, std::int32_t>::first))
		throw std::invalid_argument("UAV barrier boundaries must be sorted");
	if (std::ranges::any_of(barriers, isUavBarrierBoundaryOutOfRange(nodes.size())))
		throw std::invalid_argument("UAV barrier boundary is out of range");
	if (std::ranges::any_of(barriers, hasInvalidUavBarrierProducer))
		throw std::invalid_argument("UAV barrier must reference a preceding node");
	ModelParamsBuilder builder(specs, sourceWeightBytes);

	auto barrier = barriers.begin();
	for (std::int32_t index = 0; std::cmp_less(index, nodes.size()); ++index) {
		std::vector<std::int64_t> targets;
		while (barrier != barriers.end() && barrier->first == index) {
			targets.push_back(barrier->second);
			++barrier;
		}
		builder.beginNode(index);
		std::visit(builder, nodes[index]);
		if (std::holds_alternative<InputItem>(nodes[index]) || isViewNode(nodes[index]))
			builder.addBarrierStep(std::move(targets));
		else
			builder.setNodeBarriers(std::move(targets));
	}
	return builder.finish(outputIndex);
}

struct StepDefinition final {
	std::int64_t nodeIndex = -1;
	std::array<std::int64_t, 3> inputs{};
	std::uint32_t inputCount = 0;
	std::int64_t output = -1;
	ProgramShader shader = ProgramShader::None;
	std::size_t barrierBegin = 0, barrierCount = 0;
};

/**
 * Borrowed, pre-resolved sidecar. Its backing storage must remain alive through
 * construction of the runtime objects; no SDK types or device objects are stored here.
 * Tensor indices equal node indices. Every node defines exactly one tensor, placed in node order.
 * Only InputItem interprets a source TensorSpec; operands, outputs and barriers always reference nodes.
 * Internal tensors used to decompose an operation have the same type/shape/stride semantics as node tensors.
 * They belong to a separate implementation sidecar, identified by owning node and operation-local ID.
 * Local IDs are compile-time constants in each operation implementation, never part of the model definition.
 * The fixed model node index and local ID uniquely determine identity; no runtime ID allocation is needed.
 * Each operation accesses only its own internal tensors; those tensors cannot form graph edges.
 * DML opaque pointers are SDK-defined resources with no tensor layout and are managed separately.
 * Barrier entries identify nodes whose storage needs a UAV barrier before a step.
 */
struct ModelParams final {
	std::span<const TensorSpec> tensors;
	// Node-indexed storage indices. Views share an index; weights use -1 and their TensorSpec byte offset.
	std::span<const std::int64_t> placements;
	std::span<const StorageDefinition> storages;
	std::span<const StepDefinition> steps;
	std::span<const std::int64_t> inputTensors;
	std::span<const std::int64_t> uavBarriers;
	std::int64_t outputTensor = -1;
	std::uint64_t weightBytes = 0;
	std::size_t sourceWeightBytes = 0;
};

// Copies the transient constexpr vectors into caller-owned fixed or dynamic tables.
template<typename Tables> constexpr void storeModelParams(const PreparedParams &prepared, Tables &tables)
{
	for (std::size_t index = 0; index < prepared.tensors.size(); ++index) {
		tables.tensors[index] = prepared.tensors[index];
		tables.placements[index] = prepared.placements[index];
	}
	for (std::size_t i = 0; i < prepared.storages.size(); ++i)
		tables.storages[i] = prepared.storages[i];
	std::size_t barrierIndex = 0;
	for (std::size_t i = 0; i < prepared.steps.size(); ++i) {
		const auto &source = prepared.steps[i];
		auto &target = tables.steps[i];
		target.nodeIndex = source.nodeIndex;
		target.output = source.output;
		target.shader = source.shader;
		target.inputCount = static_cast<std::uint32_t>(source.inputs.size());
		for (std::size_t j = 0; j < source.inputs.size(); ++j)
			target.inputs[j] = source.inputs[j];
		target.barrierBegin = barrierIndex;
		target.barrierCount = source.barriersBefore.size();
		for (const auto tensor : source.barriersBefore)
			tables.uavBarriers[barrierIndex++] = tensor;
	}
	for (std::size_t i = 0; i < prepared.inputTensors.size(); ++i)
		tables.inputTensors[i] = prepared.inputTensors[i];
	tables.outputTensor = prepared.outputTensor;
	tables.weightBytes = prepared.weightBytes;
}

/**
 * Derived sidecar tables; nodes remain the single source of truth.
 * Exactly one tensor description is stored at each node index, including input and metadata nodes.
 * Tables keep no self-pointers, so constexpr construction and copying are safe.
 */
template<std::size_t NodeCount, std::size_t BarrierCount> struct ModelParamsStorage final {
	std::array<TensorSpec, NodeCount> tensors{};
	std::array<std::int64_t, NodeCount> placements{};
	std::array<StorageDefinition, NodeCount> storages{};
	std::array<StepDefinition, NodeCount> steps{};
	std::array<std::int64_t, NodeCount> inputTensors{};
	std::array<std::int64_t, BarrierCount> uavBarriers{};
	std::size_t storageCount = 0, stepCount = 0, inputCount = 0, sourceWeightBytes = 0;
	std::int64_t outputTensor = -1;
	std::uint64_t weightBytes = 0;

	constexpr auto params() const & noexcept -> ModelParams
	{
		return {tensors,
			placements,
			std::span(storages).first(storageCount),
			std::span(steps).first(stepCount),
			std::span(inputTensors).first(inputCount),
			uavBarriers,
			outputTensor,
			weightBytes,
			sourceWeightBytes};
	}
	auto params() const && -> ModelParams = delete;
};

template<std::size_t InputCount, std::size_t NodeCount, std::size_t BarrierCount>
constexpr auto makeModelParamsStorage(const std::array<TensorSpec, InputCount> &specs,
				      const std::array<Node, NodeCount> &nodes,
				      const std::array<std::pair<std::int32_t, std::int32_t>, BarrierCount> &barriers,
				      std::int32_t outputIndex, std::size_t sourceWeightBytes)
	-> ModelParamsStorage<NodeCount, BarrierCount>
{
	const auto prepared = prepareModelParams(specs, nodes, barriers, outputIndex, sourceWeightBytes);
	ModelParamsStorage<NodeCount, BarrierCount> tables;
	tables.storageCount = prepared.storages.size();
	tables.stepCount = prepared.steps.size();
	tables.inputCount = prepared.inputTensors.size();
	tables.sourceWeightBytes = sourceWeightBytes;
	storeModelParams(prepared, tables);
	return tables;
}

/** Runtime preparation for dynamically assembled test graphs; production uses constexpr fixed tables. */
struct DynamicModelParamsStorage final {
	std::vector<TensorSpec> tensors;
	std::vector<std::int64_t> placements;
	std::vector<StorageDefinition> storages;
	std::vector<StepDefinition> steps;
	std::vector<std::int64_t> inputTensors, uavBarriers;
	std::int64_t outputTensor = -1;
	std::uint64_t weightBytes = 0;
	std::size_t sourceWeightBytes = 0;
	constexpr auto params() const & noexcept -> ModelParams
	{
		return {tensors,      placements,  storages,         steps, inputTensors, uavBarriers,
			outputTensor, weightBytes, sourceWeightBytes};
	}
	auto params() const && -> ModelParams = delete;
};

constexpr auto makeModelParamsStorage(std::span<const TensorSpec> specs, std::span<const Node> nodes,
				      std::span<const std::pair<std::int32_t, std::int32_t>> barriers,
				      std::int32_t outputIndex, std::size_t sourceWeightBytes)
	-> DynamicModelParamsStorage
{
	const auto prepared = prepareModelParams(specs, nodes, barriers, outputIndex, sourceWeightBytes);
	DynamicModelParamsStorage tables;
	tables.tensors.resize(prepared.tensors.size());
	tables.placements.resize(prepared.tensors.size());
	tables.storages.resize(prepared.storages.size());
	tables.steps.resize(prepared.steps.size());
	tables.inputTensors.resize(prepared.inputTensors.size());
	tables.uavBarriers.resize(barriers.size());
	tables.sourceWeightBytes = sourceWeightBytes;
	storeModelParams(prepared, tables);
	return tables;
}

} // namespace Definition

/**
 * Immutable borrowed description: literal nodes/weights and constexpr-resolved model tables.
 * The backing arrays must outlive every DmlProgram created from this description.
 * Copying this handle copies spans only, not model metadata or weights.
 */
struct DmlProgramDefinition final {
	std::span<const Definition::Node> nodes;
	Definition::ModelParams params;
	std::span<const char> weights;
};

// Byte-oriented test/import callers can expose their existing storage without copying it.
inline auto makeDmlProgramDefinition(std::span<const Definition::Node> nodes, const Definition::ModelParams &params,
				     std::span<const std::byte> weights) -> DmlProgramDefinition
{
	return {nodes, params, {reinterpret_cast<const char *>(weights.data()), weights.size()}};
}

} // namespace BaselineDml
