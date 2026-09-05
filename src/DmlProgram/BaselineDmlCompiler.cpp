// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BaselineDmlCompiler.hpp"

#include <algorithm>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

namespace BaselineDml {
namespace {

using Microsoft::WRL::ComPtr;
using Tensor = CompiledTensor;
using Storage = CompiledStorage;
using Step = CompiledStep;
constexpr TensorId noTensor = -1;

void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

void check(HRESULT result, const char *operation)
{
	if (FAILED(result)) {
		throw std::runtime_error(std::string(operation) + " failed (HRESULT " + std::to_string(result) + ")");
	}
}

auto alignTo(std::uint64_t value, std::uint64_t alignment) -> std::uint64_t
{
	return (value + alignment - 1) / alignment * alignment;
}

auto tensorSize(UINT DimensionCount, const UINT *Sizes, const UINT *Strides) -> std::uint64_t
{
	if (!DimensionCount) {
		return 0;
	}
	std::uint64_t lastElement = 0;
	for (UINT index = 0; index < DimensionCount; ++index) {
		if (!Sizes[index]) {
			return 0;
		}
		lastElement += static_cast<std::uint64_t>(Sizes[index] - 1) * Strides[index];
	}
	return alignTo((lastElement + 1) * sizeof(float), 4);
}

void setContiguousShape(Tensor &tensor, UINT DimensionCount, const UINT *Sizes)
{
	tensor.sizes.assign(Sizes, Sizes + DimensionCount);
	tensor.strides.assign(DimensionCount, 0);
	if (DimensionCount) {
		tensor.strides[DimensionCount - 1] = 1;
		for (UINT index = DimensionCount - 1; index > 0; --index) {
			tensor.strides[index - 1] = tensor.strides[index] * tensor.sizes[index];
		}
	}
	tensor.size = tensorSize(DimensionCount, tensor.sizes.data(), tensor.strides.data());
}

struct TensorDescriptor final {
	DML_BUFFER_TENSOR_DESC buffer{};
	DML_TENSOR_DESC tensor{};

	explicit TensorDescriptor(const Tensor &value)
	{
		buffer.DataType = DML_TENSOR_DATA_TYPE_FLOAT32;
		buffer.Flags = value.weight ? DML_TENSOR_FLAG_OWNED_BY_DML : DML_TENSOR_FLAG_NONE;
		buffer.DimensionCount = static_cast<UINT>(value.sizes.size());
		buffer.Sizes = value.sizes.data();
		buffer.Strides = value.strides.data();
		buffer.TotalTensorSizeInBytes = value.size;
		buffer.GuaranteedBaseOffsetAlignment = 0;
		tensor = {DML_TENSOR_TYPE_BUFFER, &buffer};
	}
};

class Builder final {
public:
	explicit Builder(IDMLDevice *device, std::span<const std::byte> weights) : device_(device), weights_(weights) {}

	auto input(UINT DimensionCount, const UINT *Sizes) -> TensorId
	{
		const auto id = createTensor(DimensionCount, Sizes);
		input_ = id;
		return id;
	}

	auto weight(UINT DimensionCount, const UINT *Sizes, std::int64_t byteOffset) -> TensorId
	{
		Tensor tensor = makeTensor(DimensionCount, Sizes);
		require(byteOffset >= 0 && std::cmp_less_equal(byteOffset, weights_.size()) &&
				tensor.size <= weights_.size() - static_cast<std::size_t>(byteOffset),
			"BaselineDml weight exceeds its resource");
		tensor.weight = true;
		tensor.weightCount = static_cast<std::int64_t>(tensor.size / sizeof(float));
		tensor.byteOffset = byteOffset;
		tensor.weightOffset = alignTo(weightBytes_, 16);
		weightBytes_ = tensor.weightOffset + tensor.size;
		tensors_.push_back(std::move(tensor));
		return static_cast<TensorId>(tensors_.size() - 1);
	}

	auto reshape(TensorId Input, UINT DimensionCount, const UINT *Sizes) -> TensorId
	{
		Tensor tensor = tensors_.at(Input);
		setContiguousShape(tensor, DimensionCount, Sizes);
		if (!tensor.weight && tensor.size > storages_.at(tensor.storage).size) {
			throw std::runtime_error("generated DirectML reshape exceeds its storage");
		}
		tensors_.push_back(std::move(tensor));
		return static_cast<TensorId>(tensors_.size() - 1);
	}

	auto transpose(TensorId Input, UINT DimensionCount, const UINT *Permutation) -> TensorId
	{
		const auto &inputTensor = tensors_.at(Input);
		if (DimensionCount != inputTensor.sizes.size()) {
			throw std::runtime_error("generated DirectML transpose rank is invalid");
		}
		Tensor tensor = inputTensor;
		tensor.sizes.assign(DimensionCount, 1);
		tensor.strides.assign(DimensionCount, 0);
		std::size_t outputIndex = 0;
		for (UINT index = 0; index < DimensionCount; ++index) {
			const auto inputIndex = Permutation[index];
			if (inputIndex >= DimensionCount) {
				throw std::runtime_error("generated DirectML transpose axis is invalid");
			}
			for (UINT previous = 0; previous < index; ++previous) {
				if (Permutation[previous] == inputIndex)
					throw std::runtime_error("generated DirectML transpose axis is duplicated");
			}
			tensor.sizes[outputIndex] = inputTensor.sizes[inputIndex];
			tensor.strides[outputIndex] = inputTensor.strides[inputIndex];
			++outputIndex;
		}
		tensor.size = tensorSize(DimensionCount, tensor.sizes.data(), tensor.strides.data());
		tensors_.push_back(std::move(tensor));
		return static_cast<TensorId>(tensors_.size() - 1);
	}

	auto add(TensorId A, TensorId B, UINT DimensionCount, const UINT *Sizes) -> TensorId
	{
		const auto output = createTensor(DimensionCount, Sizes);
		A = broadcast(A, DimensionCount, Sizes);
		B = broadcast(B, DimensionCount, Sizes);
		TensorDescriptor aDesc(tensors_.at(A)), bDesc(tensors_.at(B)), outputDesc(tensors_.at(output));
		DML_ELEMENT_WISE_ADD_OPERATOR_DESC description{&aDesc.tensor, &bDesc.tensor, &outputDesc.tensor};
		compile(DML_OPERATOR_ELEMENT_WISE_ADD, description, {A, B}, output);
		return output;
	}

	auto multiply(TensorId A, TensorId B, UINT DimensionCount, const UINT *Sizes) -> TensorId
	{
		const auto output = createTensor(DimensionCount, Sizes);
		A = broadcast(A, DimensionCount, Sizes);
		B = broadcast(B, DimensionCount, Sizes);
		TensorDescriptor aDesc(tensors_.at(A)), bDesc(tensors_.at(B)), outputDesc(tensors_.at(output));
		DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC description{&aDesc.tensor, &bDesc.tensor, &outputDesc.tensor};
		compile(DML_OPERATOR_ELEMENT_WISE_MULTIPLY, description, {A, B}, output);
		return output;
	}

	auto clip(TensorId inputId, float minimum, float maximum) -> TensorId
	{
		const auto &input = tensors_.at(inputId);
		const auto output = createTensor(static_cast<UINT>(input.sizes.size()), input.sizes.data());
		TensorDescriptor inputDesc(tensors_.at(inputId)), outputDesc(tensors_.at(output));
		DML_ELEMENT_WISE_CLIP_OPERATOR_DESC description{};
		description.InputTensor = &inputDesc.tensor;
		description.OutputTensor = &outputDesc.tensor;
		description.Min = minimum;
		description.Max = maximum;
		compile(DML_OPERATOR_ELEMENT_WISE_CLIP, description, {inputId}, output);
		return output;
	}

	auto relu(TensorId inputId) -> TensorId
	{
		return activation<DML_ACTIVATION_RELU_OPERATOR_DESC>(DML_OPERATOR_ACTIVATION_RELU, inputId);
	}

	auto sigmoid(TensorId inputId) -> TensorId
	{
		return activation<DML_ACTIVATION_SIGMOID_OPERATOR_DESC>(DML_OPERATOR_ACTIVATION_SIGMOID, inputId);
	}

	auto convolution(TensorId Input, TensorId Filter, TensorId Bias, DML_CONVOLUTION_DIRECTION Direction,
			 UINT DimensionCount, const UINT *Strides, const UINT *Dilations, const UINT *StartPadding,
			 const UINT *EndPadding, UINT GroupCount, UINT OutputDimensionCount, const UINT *OutputSizes)
		-> TensorId
	{
		require(DimensionCount == 2 && OutputDimensionCount == 4 && tensors_.at(Input).sizes.size() == 4 &&
				tensors_.at(Filter).sizes.size() == 4,
			"BaselineDml Conv2d requires four-dimensional input, filter and output");
		if (Bias != noTensor) {
			const auto &biasTensor = tensors_.at(Bias);
			const auto channels = biasTensor.sizes[biasTensor.sizes.size() - 1];
			const std::array<UINT, 4> biasSizes{1, channels, 1, 1};
			Bias = reshape(Bias, static_cast<UINT>(biasSizes.size()), biasSizes.data());
		}
		const auto output = createTensor(OutputDimensionCount, OutputSizes);
		TensorDescriptor inputDesc(tensors_.at(Input)), filterDesc(tensors_.at(Filter)),
			outputDesc(tensors_.at(output));
		TensorDescriptor biasDesc(Bias == noTensor ? tensors_.at(Filter) : tensors_.at(Bias));
		const std::vector<UINT> outputPadding(DimensionCount);
		DML_CONVOLUTION_OPERATOR_DESC description{};
		description.InputTensor = &inputDesc.tensor;
		description.FilterTensor = &filterDesc.tensor;
		description.BiasTensor = Bias == noTensor ? nullptr : &biasDesc.tensor;
		description.OutputTensor = &outputDesc.tensor;
		description.Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION;
		description.Direction = Direction;
		description.DimensionCount = DimensionCount;
		description.Strides = Strides;
		description.Dilations = Dilations;
		description.StartPadding = StartPadding;
		description.EndPadding = EndPadding;
		description.OutputPadding = outputPadding.data();
		description.GroupCount = GroupCount;
		compile(DML_OPERATOR_CONVOLUTION, description, {Input, Filter, Bias}, output);
		return output;
	}

	auto averagePool(TensorId Input, UINT DimensionCount, const UINT *Strides, const UINT *WindowSize,
			 const UINT *StartPadding, const UINT *EndPadding, UINT OutputDimensionCount,
			 const UINT *OutputSizes) -> TensorId
	{
		require(DimensionCount == 2 && OutputDimensionCount == 4 && tensors_.at(Input).sizes.size() == 4,
			"BaselineDml AveragePool2d requires four-dimensional input and output");
		const auto output = createTensor(OutputDimensionCount, OutputSizes);
		TensorDescriptor inputDesc(tensors_.at(Input)), outputDesc(tensors_.at(output));
		DML_AVERAGE_POOLING_OPERATOR_DESC description{};
		description.InputTensor = &inputDesc.tensor;
		description.OutputTensor = &outputDesc.tensor;
		description.DimensionCount = DimensionCount;
		description.Strides = Strides;
		description.WindowSize = WindowSize;
		description.StartPadding = StartPadding;
		description.EndPadding = EndPadding;
		description.IncludePadding = FALSE;
		compile(DML_OPERATOR_AVERAGE_POOLING, description, {Input}, output);
		return output;
	}

	auto gemm(TensorId left, TensorId right, UINT DimensionCount, const UINT *OutputSizes) -> TensorId
	{
		const auto &leftTensor = tensors_.at(left);
		const auto &rightTensor = tensors_.at(right);
		const auto rows = leftTensor.sizes[leftTensor.sizes.size() - 2];
		const auto inner = leftTensor.sizes[leftTensor.sizes.size() - 1];
		const auto columns = rightTensor.sizes[rightTensor.sizes.size() - 1];
		const std::array<UINT, 4> leftTensorSizes{1, 1, rows, inner};
		const std::array<UINT, 4> rightTensorSizes{1, 1, inner, columns};
		const std::array<UINT, 4> outputTensorSizes{1, 1, rows, columns};
		left = reshape(left, static_cast<UINT>(leftTensorSizes.size()), leftTensorSizes.data());
		right = reshape(right, static_cast<UINT>(rightTensorSizes.size()), rightTensorSizes.data());
		const auto output = createTensor(static_cast<UINT>(outputTensorSizes.size()), outputTensorSizes.data());
		TensorDescriptor leftDesc(tensors_.at(left)), rightDesc(tensors_.at(right)),
			outputDesc(tensors_.at(output));
		DML_GEMM_OPERATOR_DESC description{};
		description.ATensor = &leftDesc.tensor;
		description.BTensor = &rightDesc.tensor;
		description.OutputTensor = &outputDesc.tensor;
		description.TransA = DML_MATRIX_TRANSFORM_NONE;
		description.TransB = DML_MATRIX_TRANSFORM_NONE;
		description.Alpha = 1.0F;
		description.Beta = 1.0F;
		compile(DML_OPERATOR_GEMM, description, {left, right, noTensor}, output);
		return reshape(output, DimensionCount, OutputSizes);
	}

	auto upsample(TensorId Input, UINT Height, UINT Width, UINT DimensionCount, const UINT *OutputSizes) -> TensorId
	{
		const auto output = createTensor(DimensionCount, OutputSizes);
		TensorDescriptor inputDesc(tensors_.at(Input)), outputDesc(tensors_.at(output));
		DML_UPSAMPLE_2D_OPERATOR_DESC description{};
		description.InputTensor = &inputDesc.tensor;
		description.OutputTensor = &outputDesc.tensor;
		description.ScaleSize = {Width, Height};
		description.InterpolationMode = DML_INTERPOLATION_MODE_LINEAR;
		compile(DML_OPERATOR_UPSAMPLE_2D, description, {Input}, output);
		return output;
	}

	auto join(TensorId Left, TensorId Right, UINT Axis, UINT DimensionCount, const UINT *OutputSizes) -> TensorId
	{
		const auto output = createTensor(DimensionCount, OutputSizes);
		TensorDescriptor leftDesc(tensors_.at(Left));
		TensorDescriptor rightDesc(tensors_.at(Right));
		const std::array<DML_TENSOR_DESC, 2> inputDescs{leftDesc.tensor, rightDesc.tensor};
		TensorDescriptor outputDesc(tensors_.at(output));
		DML_JOIN_OPERATOR_DESC description{};
		description.InputCount = static_cast<UINT>(inputDescs.size());
		description.InputTensors = inputDescs.data();
		description.OutputTensor = &outputDesc.tensor;
		description.Axis = Axis;
		compile(DML_OPERATOR_JOIN, description, {Left, Right}, output);
		return output;
	}

	void setOutput(TensorId output) { output_ = output; }

	auto tensors() -> std::vector<Tensor> & { return tensors_; }
	auto storages() -> std::vector<Storage> & { return storages_; }
	auto steps() -> std::vector<Step> & { return steps_; }
	auto inputTensor() const -> TensorId { return input_; }
	auto outputTensor() const -> TensorId { return output_; }
	auto weightBytes() const -> std::uint64_t { return weightBytes_; }

	auto finish() -> CompiledDmlProgram
	{
		return {std::move(tensors_), std::move(storages_), std::move(steps_), input_, output_, weightBytes_};
	}

private:
	auto makeTensor(UINT DimensionCount, const UINT *Sizes) const -> Tensor
	{
		Tensor tensor;
		setContiguousShape(tensor, DimensionCount, Sizes);
		return tensor;
	}

	auto createTensor(UINT DimensionCount, const UINT *Sizes) -> TensorId
	{
		Tensor tensor = makeTensor(DimensionCount, Sizes);
		tensor.storage = static_cast<std::int64_t>(storages_.size());
		storages_.push_back({tensor.size});
		tensors_.push_back(std::move(tensor));
		return static_cast<TensorId>(tensors_.size() - 1);
	}

	auto broadcast(TensorId Input, UINT DimensionCount, const UINT *Sizes) -> TensorId
	{
		const auto &input = tensors_.at(Input);
		if (DimensionCount < input.sizes.size()) {
			throw std::runtime_error("generated DirectML broadcast rank is invalid");
		}
		if (input.sizes.size() == DimensionCount && std::equal(input.sizes.begin(), input.sizes.end(), Sizes)) {
			return Input;
		}
		Tensor tensor = input;
		tensor.sizes.assign(Sizes, Sizes + DimensionCount);
		tensor.strides.assign(DimensionCount, 0);
		const auto offset = DimensionCount - input.sizes.size();
		for (UINT index = 0; index < input.sizes.size(); ++index) {
			const auto destination = offset + index;
			if (input.sizes[index] != 1 && input.sizes[index] != tensor.sizes[destination]) {
				throw std::runtime_error("generated DirectML broadcast is invalid");
			}
			tensor.strides[destination] = input.sizes[index] == 1 ? 0 : input.strides[index];
		}
		tensor.size = tensorSize(DimensionCount, tensor.sizes.data(), tensor.strides.data());
		tensors_.push_back(std::move(tensor));
		return static_cast<TensorId>(tensors_.size() - 1);
	}

	template<typename Description>
	void compileInputs(DML_OPERATOR_TYPE type, const Description &description, std::int64_t InputCount,
			   const TensorId *Inputs, TensorId output)
	{
		DML_OPERATOR_DESC operatorDescription{type, &description};
		ComPtr<IDMLOperator> operation;
		check(device_->CreateOperator(&operatorDescription, IID_PPV_ARGS(&operation)),
		      "IDMLDevice::CreateOperator");
		Step step;
		step.inputs.assign(Inputs, Inputs + InputCount);
		step.output = output;
		check(device_->CompileOperator(operation.Get(), DML_EXECUTION_FLAG_NONE, IID_PPV_ARGS(&step.operation)),
		      "IDMLDevice::CompileOperator");
		steps_.push_back(std::move(step));
	}

	template<typename Description>
	void compile(DML_OPERATOR_TYPE type, const Description &description, std::initializer_list<TensorId> inputs,
		     TensorId output)
	{
		compileInputs(type, description, static_cast<std::int64_t>(inputs.size()), inputs.begin(), output);
	}

	template<typename Description> auto activation(DML_OPERATOR_TYPE type, TensorId inputId) -> TensorId
	{
		const auto &input = tensors_.at(inputId);
		const auto output = createTensor(static_cast<UINT>(input.sizes.size()), input.sizes.data());
		TensorDescriptor inputDesc(tensors_.at(inputId)), outputDesc(tensors_.at(output));
		Description description{};
		description.InputTensor = &inputDesc.tensor;
		description.OutputTensor = &outputDesc.tensor;
		compile(type, description, {inputId}, output);
		return output;
	}

	IDMLDevice *device_;
	std::span<const std::byte> weights_;
	std::vector<Tensor> tensors_;
	std::vector<Storage> storages_;
	std::vector<Step> steps_;
	TensorId input_ = noTensor;
	TensorId output_ = noTensor;
	std::uint64_t weightBytes_ = 0;
};

} // namespace

BaselineDmlCompiler::BaselineDmlCompiler(IDMLDevice *device) noexcept : device_(device) {}

auto BaselineDmlCompiler::compile(const Graph &graph, std::span<const std::byte> weights) const -> CompiledDmlProgram
{
	require(graph.inputSpecCount == 1 && graph.inputSpecs != nullptr,
		"BaselineDmlProgram requires one input specification");
	require(graph.nodeCount >= 0 && (graph.nodeCount == 0 || graph.nodes != nullptr),
		"BaselineDmlProgram has no node table");
	Builder builder(device_, weights);
	std::vector<TensorId> values;
	auto validateShape = [](const auto &value) {
		require(value.shapeRank > 0 && value.shapeRank <= value.shape.size(),
			"BaselineDml shape rank must be between 1 and 4");
		const auto shape = std::span(value.shape).first(value.shapeRank);
		require(std::ranges::all_of(shape, [](auto size) { return size > 0; }),
			"BaselineDml shape dimensions must be positive");
	};
	bool hasInput = false;
	auto resolve = [&](std::size_t sourceIndex) {
		require(sourceIndex < values.size(), "BaselineDmlProgram input must reference a preceding node");
		return values[sourceIndex];
	};
	for (std::int64_t index = 0; index < graph.nodeCount; ++index) {
		const auto result = std::visit(
			[&](const auto &node) -> TensorId {
				using T = std::decay_t<decltype(node)>;
				if constexpr (requires { node.shapeRank; })
					validateShape(node);
				if constexpr (std::is_same_v<T, Input>) {
					require(node.inputSpecIndex < graph.inputSpecCount,
						"BaselineDml input specification index is out of range");
					require(!hasInput, "BaselineDmlProgram supports one Input node");
					hasInput = true;
					return std::visit(
						[&](const auto &shape) {
							require(std::ranges::all_of(shape,
										    [](auto size) { return size > 0; }),
								"BaselineDml input shape dimensions must be positive");
							return builder.input(static_cast<UINT>(shape.size()),
									     shape.data());
						},
						graph.inputSpecs[node.inputSpecIndex]);
				} else if constexpr (std::is_same_v<T, Constant>) {
					return builder.weight(static_cast<UINT>(node.shapeRank), node.shape.data(),
							      node.byteOffset);
				} else if constexpr (std::is_same_v<T, Reshape>) {
					return builder.reshape(resolve(node.inputIndex),
							       static_cast<UINT>(node.shapeRank), node.shape.data());
				} else if constexpr (std::is_same_v<T, TransposeRank2> ||
						     std::is_same_v<T, TransposeRank3> ||
						     std::is_same_v<T, TransposeRank4>) {
					return builder.transpose(resolve(node.inputIndex),
								 static_cast<UINT>(node.permutation.size()),
								 node.permutation.data());
				} else if constexpr (std::is_same_v<T, AddRank4>) {
					require(std::ranges::all_of(node.shape, [](auto size) { return size > 0; }),
						"BaselineDml rank-4 output shape dimensions must be positive");
					return builder.add(resolve(node.leftIndex), resolve(node.rightIndex),
							   static_cast<UINT>(node.shape.size()), node.shape.data());
				} else if constexpr (std::is_same_v<T, MulRank4>) {
					require(std::ranges::all_of(node.shape, [](auto size) { return size > 0; }),
						"BaselineDml rank-4 output shape dimensions must be positive");
					return builder.multiply(resolve(node.leftIndex), resolve(node.rightIndex),
								static_cast<UINT>(node.shape.size()),
								node.shape.data());
				} else if constexpr (std::is_same_v<T, Clip>) {
					return builder.clip(resolve(node.inputIndex), node.minimum, node.maximum);
				} else if constexpr (std::is_same_v<T, Relu>) {
					return builder.relu(resolve(node.inputIndex));
				} else if constexpr (std::is_same_v<T, Sigmoid>) {
					return builder.sigmoid(resolve(node.inputIndex));
				} else if constexpr (std::is_same_v<T, Conv2d> || std::is_same_v<T, BiasedConv2d> ||
						     std::is_same_v<T, ConvTranspose2d> ||
						     std::is_same_v<T, BiasedConvTranspose2d>) {
					require(std::ranges::all_of(node.shape, [](auto size) { return size > 0; }),
						"BaselineDml Conv2d shape dimensions must be positive");
					constexpr auto direction = (std::is_same_v<T, ConvTranspose2d> ||
								    std::is_same_v<T, BiasedConvTranspose2d>)
									   ? DML_CONVOLUTION_DIRECTION_BACKWARD
									   : DML_CONVOLUTION_DIRECTION_FORWARD;
					const auto bias = [&]() -> TensorId {
						if constexpr (requires { node.biasIndex; })
							return resolve(node.biasIndex);
						else
							return noTensor;
					}();
					return builder.convolution(
						resolve(node.inputIndex), resolve(node.filterIndex), bias, direction,
						static_cast<UINT>(node.strides.size()), node.strides.data(),
						node.dilations.data(), node.startPadding.data(), node.endPadding.data(),
						node.groupCount, static_cast<UINT>(node.shape.size()),
						node.shape.data());
				} else if constexpr (std::is_same_v<T, AveragePool2d>) {
					require(std::ranges::all_of(node.shape, [](auto size) { return size > 0; }),
						"BaselineDml AveragePool2d shape dimensions must be positive");
					return builder.averagePool(resolve(node.inputIndex),
								   static_cast<UINT>(node.strides.size()),
								   node.strides.data(), node.window.data(),
								   node.startPadding.data(), node.endPadding.data(),
								   static_cast<UINT>(node.shape.size()),
								   node.shape.data());
				} else if constexpr (std::is_same_v<T, MatMulRank4>) {
					require(std::ranges::all_of(node.shape, [](auto size) { return size > 0; }),
						"BaselineDml rank-4 output shape dimensions must be positive");
					return builder.gemm(resolve(node.leftIndex), resolve(node.rightIndex),
							    static_cast<UINT>(node.shape.size()), node.shape.data());
				} else if constexpr (std::is_same_v<T, ResizeRank4>) {
					require(std::ranges::all_of(node.shape, [](auto size) { return size > 0; }),
						"BaselineDml rank-4 output shape dimensions must be positive");
					return builder.upsample(resolve(node.inputIndex), node.height, node.width,
								static_cast<UINT>(node.shape.size()),
								node.shape.data());
				} else if constexpr (std::is_same_v<T, Concat>) {
					require(std::ranges::all_of(node.shape, [](auto size) { return size > 0; }),
						"BaselineDml rank-4 output shape dimensions must be positive");
					require(node.axis < node.shape.size(),
						"BaselineDml Concat axis is out of range");
					return builder.join(resolve(node.leftIndex), resolve(node.rightIndex),
							    node.axis, static_cast<UINT>(node.shape.size()),
							    node.shape.data());
				} else {
					static_assert(std::is_same_v<T, void>, "Missing BaselineDml node compiler");
				}
			},
			graph.nodes[index]);
		values.push_back(result);
	}
	require(hasInput, "BaselineDmlProgram requires an Input node");
	builder.setOutput(resolve(graph.outputIndex));
	return builder.finish();
}

} // namespace BaselineDml
