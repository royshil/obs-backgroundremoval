// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BaselineDmlCompiler.hpp"

#include <algorithm>
#include <initializer_list>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace BaselineDml {
namespace {

using namespace ProgramDefinition;
using Microsoft::WRL::ComPtr;
using Tensor = CompiledTensor;
constexpr TensorId noTensor = -1;

void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

void check(HRESULT result, const char *operation)
{
	if (FAILED(result))
		throw std::runtime_error(std::string(operation) + " failed (HRESULT " + std::to_string(result) + ")");
}

auto typeName(DataType type) -> const char *
{
	switch (type) {
	case DataType::Float32:
		return "Float32";
	case DataType::Float16:
		return "Float16";
	case DataType::UInt32:
		return "UInt32";
	case DataType::UInt16:
		return "UInt16";
	case DataType::UInt8:
		return "UInt8";
	case DataType::Int32:
		return "Int32";
	case DataType::Int16:
		return "Int16";
	case DataType::Int8:
		return "Int8";
	case DataType::Float64:
		return "Float64";
	case DataType::UInt64:
		return "UInt64";
	case DataType::Int64:
		return "Int64";
	default:
		return "Unknown";
	}
}

auto elementSize(DataType type) -> std::uint64_t
{
	switch (type) {
	case DataType::UInt8:
	case DataType::Int8:
		return 1;
	case DataType::Float16:
	case DataType::UInt16:
	case DataType::Int16:
		return 2;
	case DataType::Float32:
	case DataType::UInt32:
	case DataType::Int32:
		return 4;
	case DataType::Float64:
	case DataType::UInt64:
	case DataType::Int64:
		return 8;
	default:
		throw std::runtime_error("Unknown tensor data type");
	}
}

auto checkedAdd(std::uint64_t a, std::uint64_t b) -> std::uint64_t
{
	require(a <= std::numeric_limits<std::uint64_t>::max() - b, "tensor size addition overflow");
	return a + b;
}

auto checkedMultiply(std::uint64_t a, std::uint64_t b) -> std::uint64_t
{
	require(!b || a <= std::numeric_limits<std::uint64_t>::max() / b, "tensor size multiplication overflow");
	return a * b;
}

auto alignTo(std::uint64_t value, std::uint64_t alignment) -> std::uint64_t
{
	return checkedAdd(value, alignment - 1) / alignment * alignment;
}

void setContiguousShape(Tensor &tensor, std::span<const UINT> shape)
{
	require(!shape.empty() && shape.size() <= 4, "shape rank must be between 1 and 4");
	tensor.sizes.assign(shape.begin(), shape.end());
	tensor.strides.resize(shape.size());
	std::uint64_t count = 1;
	for (std::size_t index = shape.size(); index-- > 0;) {
		require(shape[index] > 0, "shape dimensions must be positive");
		require(count <= std::numeric_limits<UINT>::max(), "tensor stride overflow (UINT range)");
		tensor.strides[index] = static_cast<UINT>(count);
		count = checkedMultiply(count, shape[index]);
	}
	require(count <= std::numeric_limits<UINT>::max(), "tensor exceeds DirectML element limit");
	tensor.dataBytes = checkedMultiply(count, elementSize(tensor.dataType));
	tensor.size = alignTo(tensor.dataBytes, 4);
}

auto rankedShape(const auto &value) -> std::span<const UINT>
{
	require(value.shapeRank > 0 && std::cmp_less_equal(value.shapeRank, value.shape.size()),
		"shape rank must be between 1 and 4");
	return std::span(value.shape).first(static_cast<std::size_t>(value.shapeRank));
}

struct TensorDescriptor final {
	DML_BUFFER_TENSOR_DESC buffer{};
	DML_TENSOR_DESC tensor{};
	explicit TensorDescriptor(const Tensor &value)
	{
		buffer.DataType = static_cast<DML_TENSOR_DATA_TYPE>(value.dataType);
		buffer.Flags = value.weight ? DML_TENSOR_FLAG_OWNED_BY_DML : DML_TENSOR_FLAG_NONE;
		buffer.DimensionCount = static_cast<UINT>(value.sizes.size());
		buffer.Sizes = value.sizes.data();
		buffer.Strides = value.strides.data();
		buffer.TotalTensorSizeInBytes = value.size;
		tensor = {DML_TENSOR_TYPE_BUFFER, &buffer};
	}
};

class Builder final {
public:
	explicit Builder(IDMLDevice *device, std::span<const std::byte> weights) : device_(device), weights_(weights) {}

	auto input(const InputSpecItem &spec) -> TensorId
	{
		const auto id = createTensor(spec.dataType, rankedShape(spec));
		inputTensors_.push_back(id);
		return id;
	}

	auto weight(const InputSpecItem &spec) -> TensorId
	{
		Tensor tensor = makeTensor(spec.dataType, rankedShape(spec));
		require(spec.byteOffset >= 0 && std::cmp_less_equal(spec.byteOffset, weights_.size()) &&
				tensor.dataBytes <= weights_.size() - static_cast<std::size_t>(spec.byteOffset),
			"weight exceeds its resource");
		tensor.weight = true;
		tensor.byteOffset = spec.byteOffset;
		tensor.weightOffset = alignTo(weightBytes_, 16);
		weightBytes_ = checkedAdd(tensor.weightOffset, tensor.size);
		tensors_.push_back(std::move(tensor));
		return static_cast<TensorId>(tensors_.size() - 1);
	}

	auto reshape(TensorId input, std::span<const UINT> shape) -> TensorId
	{
		Tensor tensor = tensors_.at(input);
		const auto bytes = tensor.dataBytes;
		setContiguousShape(tensor, shape);
		require(bytes == tensor.dataBytes, "reshape changes the element count");
		tensors_.push_back(std::move(tensor));
		return static_cast<TensorId>(tensors_.size() - 1);
	}

	auto binary(DataType type, TensorId a, TensorId b, std::span<const UINT> shape, bool multiply) -> TensorId
	{
		const auto padded = paddedShape(shape);
		const auto output = createTensor(type, padded);
		a = broadcast(a, type, padded);
		b = broadcast(b, type, padded);
		TensorDescriptor aDesc(tensors_.at(a)), bDesc(tensors_.at(b)), outputDesc(tensors_.at(output));
		if (multiply) {
			DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC desc{&aDesc.tensor, &bDesc.tensor, &outputDesc.tensor};
			compile(DML_OPERATOR_ELEMENT_WISE_MULTIPLY, desc, {a, b}, output);
		} else {
			DML_ELEMENT_WISE_ADD_OPERATOR_DESC desc{&aDesc.tensor, &bDesc.tensor, &outputDesc.tensor};
			compile(DML_OPERATOR_ELEMENT_WISE_ADD, desc, {a, b}, output);
		}
		return reshape(output, shape);
	}

	auto clip(const Clip &node, TensorId input) -> TensorId
	{
		require(node.minimum <= node.maximum, "Clip minimum exceeds maximum");
		const auto shape = rankedShape(node);
		const auto padded = paddedShape(shape);
		checkInputType(input, node.dataType);
		input = reshape(input, padded);
		const auto output = createTensor(node.dataType, padded);
		TensorDescriptor inDesc(tensors_.at(input)), outDesc(tensors_.at(output));
		DML_ELEMENT_WISE_CLIP_OPERATOR_DESC desc{};
		desc.InputTensor = &inDesc.tensor;
		desc.OutputTensor = &outDesc.tensor;
		desc.Min = node.minimum;
		desc.Max = node.maximum;
		compile(DML_OPERATOR_ELEMENT_WISE_CLIP, desc, {input}, output);
		return reshape(output, shape);
	}

	template<typename Description>
	auto activation(DML_OPERATOR_TYPE operation, DataType type, TensorId input, std::span<const UINT> shape)
		-> TensorId
	{
		const auto padded = paddedShape(shape);
		checkInputType(input, type);
		input = reshape(input, padded);
		const auto output = createTensor(type, padded);
		TensorDescriptor inDesc(tensors_.at(input)), outDesc(tensors_.at(output));
		Description desc{};
		desc.InputTensor = &inDesc.tensor;
		desc.OutputTensor = &outDesc.tensor;
		compile(operation, desc, {input}, output);
		return reshape(output, shape);
	}

	auto convolution(const auto &node, TensorId input, TensorId filter, TensorId bias,
			 DML_CONVOLUTION_DIRECTION direction) -> TensorId
	{
		checkInputType(input, node.dataType);
		checkInputType(filter, node.dataType);
		checkInputType(bias, node.dataType);
		require(tensors_.at(input).sizes.size() == 4 && tensors_.at(filter).sizes.size() == 4,
			"Conv2d requires rank-4 input and filter");
		require(node.groupCount > 0, "Conv2d group count must be positive");
		for (std::size_t i = 0; i < 2; ++i)
			require(node.strides[i] > 0 && node.dilations[i] > 0,
				"Conv2d stride and dilation must be positive");
		const auto &biasTensor = tensors_.at(bias);
		require(biasTensor.sizes.size() == 1, "Conv2d bias must be rank 1");
		const std::array<UINT, 4> biasShape{1, biasTensor.sizes[0], 1, 1};
		bias = reshape(bias, biasShape);
		const auto output = createTensor(node.dataType, node.shape);
		TensorDescriptor inDesc(tensors_.at(input)), filterDesc(tensors_.at(filter)),
			biasDesc(tensors_.at(bias)), outDesc(tensors_.at(output));
		const std::array<UINT, 2> outputPadding{};
		DML_CONVOLUTION_OPERATOR_DESC desc{};
		desc.InputTensor = &inDesc.tensor;
		desc.FilterTensor = &filterDesc.tensor;
		desc.BiasTensor = &biasDesc.tensor;
		desc.OutputTensor = &outDesc.tensor;
		desc.Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION;
		desc.Direction = direction;
		desc.DimensionCount = 2;
		desc.Strides = node.strides.data();
		desc.Dilations = node.dilations.data();
		desc.StartPadding = node.startPadding.data();
		desc.EndPadding = node.endPadding.data();
		desc.OutputPadding = outputPadding.data();
		desc.GroupCount = node.groupCount;
		compile(DML_OPERATOR_CONVOLUTION, desc, {input, filter, bias}, output);
		return output;
	}

	auto averagePool(const AveragePool2d &node, TensorId input) -> TensorId
	{
		checkInputType(input, node.dataType);
		require(tensors_.at(input).sizes.size() == 4, "AveragePool2d requires rank-4 input");
		for (std::size_t i = 0; i < 2; ++i)
			require(node.strides[i] > 0 && node.window[i] > 0,
				"AveragePool2d stride and window must be positive");
		const auto output = createTensor(node.dataType, node.shape);
		TensorDescriptor inDesc(tensors_.at(input)), outDesc(tensors_.at(output));
		DML_AVERAGE_POOLING_OPERATOR_DESC desc{};
		desc.InputTensor = &inDesc.tensor;
		desc.OutputTensor = &outDesc.tensor;
		desc.DimensionCount = 2;
		desc.Strides = node.strides.data();
		desc.WindowSize = node.window.data();
		desc.StartPadding = node.startPadding.data();
		desc.EndPadding = node.endPadding.data();
		desc.IncludePadding = FALSE;
		compile(DML_OPERATOR_AVERAGE_POOLING, desc, {input}, output);
		return output;
	}

	auto resize(const Resize &node, TensorId input) -> TensorId
	{
		checkInputType(input, node.dataType);
		require(node.shapeRank == 4 && tensors_.at(input).sizes.size() == 4, "Resize requires rank 4");
		require(node.height > 0 && node.width > 0, "Resize scales must be positive");
		const auto output = createTensor(node.dataType, rankedShape(node));
		TensorDescriptor inDesc(tensors_.at(input)), outDesc(tensors_.at(output));
		DML_UPSAMPLE_2D_OPERATOR_DESC desc{};
		desc.InputTensor = &inDesc.tensor;
		desc.OutputTensor = &outDesc.tensor;
		desc.ScaleSize = {node.width, node.height};
		desc.InterpolationMode = DML_INTERPOLATION_MODE_LINEAR;
		compile(DML_OPERATOR_UPSAMPLE_2D, desc, {input}, output);
		return output;
	}

	auto finish(TensorId output) -> CompiledDmlProgram
	{
		return {std::move(tensors_), std::move(storages_), std::move(steps_), std::move(inputTensors_), output,
			weightBytes_};
	}

private:
	auto makeTensor(DataType type, std::span<const UINT> shape) -> Tensor
	{
		Tensor tensor;
		tensor.dataType = type;
		setContiguousShape(tensor, shape);
		DML_FEATURE_QUERY_TENSOR_DATA_TYPE_SUPPORT query{static_cast<DML_TENSOR_DATA_TYPE>(type)};
		DML_FEATURE_DATA_TENSOR_DATA_TYPE_SUPPORT support{};
		check(device_->CheckFeatureSupport(DML_FEATURE_TENSOR_DATA_TYPE_SUPPORT, sizeof(query), &query,
						   sizeof(support), &support),
		      "CheckFeatureSupport");
		require(support.IsSupported, "tensor data type is unsupported by the DirectML device");
		return tensor;
	}

	auto createTensor(DataType type, std::span<const UINT> shape) -> TensorId
	{
		Tensor tensor = makeTensor(type, shape);
		tensor.storage = static_cast<TensorId>(storages_.size());
		storages_.push_back({tensor.size});
		tensors_.push_back(std::move(tensor));
		return static_cast<TensorId>(tensors_.size() - 1);
	}

	void checkInputType(TensorId id, DataType type) const
	{
		require(tensors_.at(id).dataType == type,
			"input and operator data types must match; no implicit conversion");
	}

	static auto paddedShape(std::span<const UINT> shape) -> std::array<UINT, 4>
	{
		require(!shape.empty() && shape.size() <= 4, "shape rank must be between 1 and 4");
		std::array<UINT, 4> padded{1, 1, 1, 1};
		std::copy(shape.begin(), shape.end(), padded.end() - shape.size());
		return padded;
	}

	auto broadcast(TensorId id, DataType type, std::span<const UINT> shape) -> TensorId
	{
		checkInputType(id, type);
		const auto &input = tensors_.at(id);
		require(input.sizes.size() <= shape.size(), "broadcast rank is invalid");
		Tensor tensor = input;
		tensor.sizes.assign(shape.begin(), shape.end());
		tensor.strides.assign(shape.size(), 0);
		const auto offset = shape.size() - input.sizes.size();
		for (std::size_t i = 0; i < input.sizes.size(); ++i) {
			require(input.sizes[i] == 1 || input.sizes[i] == shape[offset + i],
				"broadcast shape is invalid");
			tensor.strides[offset + i] = input.sizes[i] == 1 ? 0 : input.strides[i];
		}
		// Broadcasting changes the logical shape, but not the underlying allocation.
		tensors_.push_back(std::move(tensor));
		return static_cast<TensorId>(tensors_.size() - 1);
	}

	template<typename Description>
	void compile(DML_OPERATOR_TYPE type, const Description &desc, std::initializer_list<TensorId> inputs,
		     TensorId output)
	{
		DML_OPERATOR_DESC operatorDesc{type, &desc};
		ComPtr<IDMLOperator> operation;
		check(device_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operation)),
		      "CreateOperator (operator/type support)");
		CompiledStep step;
		step.inputs.assign(inputs.begin(), inputs.end());
		step.output = output;
		check(device_->CompileOperator(operation.Get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&step.operation)),
		      "CompileOperator");
		steps_.push_back(std::move(step));
	}

	IDMLDevice *device_;
	std::span<const std::byte> weights_;
	std::vector<Tensor> tensors_;
	std::vector<CompiledStorage> storages_;
	std::vector<CompiledStep> steps_;
	std::vector<TensorId> inputTensors_;
	std::uint64_t weightBytes_ = 0;
};

auto nodeName(const auto &node) -> const char *
{
	using T = std::decay_t<decltype(node)>;
	if constexpr (std::is_same_v<T, InputItem>)
		return "InputItem";
	else if constexpr (std::is_same_v<T, Add>)
		return "Add";
	else if constexpr (std::is_same_v<T, Mul>)
		return "Mul";
	else if constexpr (std::is_same_v<T, Clip>)
		return "Clip";
	else if constexpr (std::is_same_v<T, Relu>)
		return "Relu";
	else if constexpr (std::is_same_v<T, Sigmoid>)
		return "Sigmoid";
	else if constexpr (std::is_same_v<T, AveragePool2d>)
		return "AveragePool2d";
	else if constexpr (std::is_same_v<T, Resize>)
		return "Resize";
	else if constexpr (std::is_same_v<T, BiasedConv2d>)
		return "BiasedConv2d";
	else if constexpr (std::is_same_v<T, BiasedConvTranspose2d>)
		return "BiasedConvTranspose2d";
	else {
		static_assert(std::is_same_v<T, void>, "Missing node name");
	}
}

} // namespace

BaselineDmlCompiler::BaselineDmlCompiler(IDMLDevice *device) noexcept : device_(device) {}

auto BaselineDmlCompiler::compile(const Graph &graph, std::span<const std::byte> weights) const -> CompiledDmlProgram
{
	require(device_ != nullptr, "BaselineDml compiler requires a device");
	require(graph.inputSpecCount <= static_cast<std::size_t>(std::numeric_limits<Size>::max()) &&
			(graph.inputSpecCount == 0 || graph.inputSpecs != nullptr),
		"invalid input specification table");
	require(graph.nodeCount > 0 && graph.nodeCount <= std::numeric_limits<Size>::max() && graph.nodes != nullptr,
		"invalid node table");
	Builder builder(device_, weights);
	std::vector<TensorId> specs;
	for (std::size_t index = 0; index < graph.inputSpecCount; ++index) {
		const auto &spec = graph.inputSpecs[index];
		try {
			switch (spec.itemType) {
			case ItemType::RuntimeInput:
				specs.push_back(builder.input(spec));
				break;
			case ItemType::Weight:
				specs.push_back(builder.weight(spec));
				break;
			default:
				throw std::runtime_error("invalid input item type");
			}
		} catch (const std::exception &error) {
			throw std::runtime_error("BaselineDml input specification " + std::to_string(index) + " (" +
						 typeName(spec.dataType) + "): " + error.what());
		}
	}
	std::vector<TensorId> values;
	auto resolve = [&](auto index) -> TensorId {
		require(std::cmp_greater_equal(index, 0) && std::cmp_less(index, values.size()),
			"input must reference a preceding node");
		return values[static_cast<std::size_t>(index)];
	};
	for (std::int64_t index = 0; index < graph.nodeCount; ++index) {
		auto compileNode = [&](const auto &node) -> TensorId {
			using T = std::decay_t<decltype(node)>;
			try {
				if constexpr (std::is_same_v<T, InputItem>) {
					require(node.inputSpecIndex >= 0 &&
							std::cmp_less(node.inputSpecIndex, specs.size()),
						"input specification index is out of range");
					return specs[static_cast<std::size_t>(node.inputSpecIndex)];
				} else if constexpr (std::is_same_v<T, Add> || std::is_same_v<T, Mul>) {
					return builder.binary(node.dataType, resolve(node.leftIndex),
							      resolve(node.rightIndex), rankedShape(node),
							      std::is_same_v<T, Mul>);
				} else if constexpr (std::is_same_v<T, Clip>) {
					return builder.clip(node, resolve(node.inputIndex));
				} else if constexpr (std::is_same_v<T, Relu>) {
					return builder.activation<DML_ACTIVATION_RELU_OPERATOR_DESC>(
						DML_OPERATOR_ACTIVATION_RELU, node.dataType, resolve(node.inputIndex),
						rankedShape(node));
				} else if constexpr (std::is_same_v<T, Sigmoid>) {
					return builder.activation<DML_ACTIVATION_SIGMOID_OPERATOR_DESC>(
						DML_OPERATOR_ACTIVATION_SIGMOID, node.dataType,
						resolve(node.inputIndex), rankedShape(node));
				} else if constexpr (std::is_same_v<T, AveragePool2d>) {
					return builder.averagePool(node, resolve(node.inputIndex));
				} else if constexpr (std::is_same_v<T, Resize>) {
					return builder.resize(node, resolve(node.inputIndex));
				} else if constexpr (std::is_same_v<T, BiasedConv2d> ||
						     std::is_same_v<T, BiasedConvTranspose2d>) {
					constexpr auto direction = std::is_same_v<T, BiasedConv2d>
									   ? DML_CONVOLUTION_DIRECTION_FORWARD
									   : DML_CONVOLUTION_DIRECTION_BACKWARD;
					return builder.convolution(node, resolve(node.inputIndex),
								   resolve(node.filterIndex), resolve(node.biasIndex),
								   direction);
				} else {
					static_assert(std::is_same_v<T, void>, "Missing node compiler");
				}
			} catch (const std::exception &error) {
				std::string context =
					"BaselineDml node " + std::to_string(index) + " " + nodeName(node);
				if constexpr (requires { node.dataType; })
					context += std::string(" (") + typeName(node.dataType) + ")";
				throw std::runtime_error(context + ": " + error.what());
			}
		};
		values.push_back(std::visit(
			[&](const auto &group) -> TensorId {
				if constexpr (std::is_same_v<std::decay_t<decltype(group)>, InputItem>)
					return compileNode(group);
				else
					return std::visit(compileNode, group);
			},
			graph.nodes[index]));
	}
	return builder.finish(resolve(graph.outputIndex));
}

} // namespace BaselineDml
