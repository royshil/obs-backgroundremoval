// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "BaselineDmlCompiler.hpp"

#include <algorithm>
#include <deque>
#include <initializer_list>
#include <stdexcept>
#include <string>
#include <utility>

namespace BackgroundRemoval {
namespace {

using Microsoft::WRL::ComPtr;
using Tensor = CompiledTensor;
using Storage = CompiledStorage;
using Step = CompiledStep;
constexpr TensorId noTensor = UINT32_MAX;

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
	if (DimensionCount > maximumTensorRank) {
		throw std::runtime_error("generated DirectML tensor rank exceeds four dimensions");
	}
	tensor.dimensionCount = DimensionCount;
	tensor.sizes.fill(1);
	tensor.strides.fill(0);
	std::copy_n(Sizes, DimensionCount, tensor.sizes.begin());
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
		buffer.DimensionCount = value.dimensionCount;
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

	auto weight(UINT DimensionCount, const UINT *Sizes, std::uint32_t sourceOffset) -> TensorId
	{
		Tensor tensor = makeTensor(DimensionCount, Sizes);
		require(sourceOffset <= weights_.size() && tensor.size <= weights_.size() - sourceOffset,
			"BaselineDml weight exceeds its resource");
		tensor.weight = true;
		tensor.weightCount = tensor.size / sizeof(float);
		tensor.weightSourceOffset = sourceOffset;
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
		if (DimensionCount != inputTensor.dimensionCount || DimensionCount > maximumTensorRank) {
			throw std::runtime_error("generated DirectML transpose rank is invalid");
		}
		Tensor tensor = inputTensor;
		tensor.dimensionCount = DimensionCount;
		tensor.sizes.fill(1);
		tensor.strides.fill(0);
		std::size_t outputIndex = 0;
		for (UINT index = 0; index < DimensionCount; ++index) {
			const auto inputIndex = Permutation[index];
			if (inputIndex >= DimensionCount) {
				throw std::runtime_error("generated DirectML transpose axis is invalid");
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
		const auto output = createTensor(input.dimensionCount, input.sizes.data());
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
		if (Bias != noTensor) {
			const auto &biasTensor = tensors_.at(Bias);
			const auto channels = biasTensor.sizes[biasTensor.dimensionCount - 1];
			const std::array<UINT, 4> biasSizes{1, channels, 1, 1};
			Bias = reshape(Bias, static_cast<UINT>(biasSizes.size()), biasSizes.data());
		}
		const auto output = createTensor(OutputDimensionCount, OutputSizes);
		TensorDescriptor inputDesc(tensors_.at(Input)), filterDesc(tensors_.at(Filter)),
			outputDesc(tensors_.at(output));
		TensorDescriptor biasDesc(Bias == noTensor ? tensors_.at(Filter) : tensors_.at(Bias));
		const std::array<UINT, maximumTensorRank> outputPadding{};
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
		const auto rows = leftTensor.sizes[leftTensor.dimensionCount - 2];
		const auto inner = leftTensor.sizes[leftTensor.dimensionCount - 1];
		const auto columns = rightTensor.sizes[rightTensor.dimensionCount - 1];
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

	auto join(UINT InputCount, const TensorId *Inputs, UINT Axis, UINT DimensionCount, const UINT *OutputSizes)
		-> TensorId
	{
		const auto output = createTensor(DimensionCount, OutputSizes);
		std::vector<TensorDescriptor> descriptors;
		descriptors.reserve(InputCount);
		for (UINT index = 0; index < InputCount; ++index) {
			descriptors.emplace_back(tensors_.at(Inputs[index]));
		}
		std::vector<DML_TENSOR_DESC> inputDescs;
		inputDescs.reserve(descriptors.size());
		for (const auto &descriptor : descriptors) {
			inputDescs.push_back(descriptor.tensor);
		}
		TensorDescriptor outputDesc(tensors_.at(output));
		DML_JOIN_OPERATOR_DESC description{};
		description.InputCount = static_cast<UINT>(inputDescs.size());
		description.InputTensors = inputDescs.data();
		description.OutputTensor = &outputDesc.tensor;
		description.Axis = Axis;
		compileInputs(DML_OPERATOR_JOIN, description, InputCount, Inputs, output);
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
		tensor.storage = static_cast<std::uint32_t>(storages_.size());
		storages_.push_back({tensor.size});
		tensors_.push_back(std::move(tensor));
		return static_cast<TensorId>(tensors_.size() - 1);
	}

	auto broadcast(TensorId Input, UINT DimensionCount, const UINT *Sizes) -> TensorId
	{
		const auto &input = tensors_.at(Input);
		if (DimensionCount > maximumTensorRank || DimensionCount < input.dimensionCount) {
			throw std::runtime_error("generated DirectML broadcast rank is invalid");
		}
		if (input.dimensionCount == DimensionCount &&
		    std::equal(input.sizes.begin(), input.sizes.begin() + DimensionCount, Sizes)) {
			return Input;
		}
		Tensor tensor = input;
		tensor.dimensionCount = DimensionCount;
		tensor.sizes.fill(1);
		tensor.strides.fill(0);
		std::copy_n(Sizes, DimensionCount, tensor.sizes.begin());
		const auto offset = DimensionCount - input.dimensionCount;
		for (UINT index = 0; index < input.dimensionCount; ++index) {
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
	void compileInputs(DML_OPERATOR_TYPE type, const Description &description, UINT InputCount,
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
		compileInputs(type, description, static_cast<UINT>(inputs.size()), inputs.begin(), output);
	}

	template<typename Description> auto activation(DML_OPERATOR_TYPE type, TensorId inputId) -> TensorId
	{
		const auto &input = tensors_.at(inputId);
		const auto output = createTensor(input.dimensionCount, input.sizes.data());
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

auto BaselineDmlCompiler::compile(const BaselineDml::Graph &graph, std::span<const std::byte> weights) const
	-> CompiledDmlProgram
{
	require(graph.inputCount == 0 || graph.inputs != nullptr, "BaselineDmlProgram has no input table");
	require(graph.nodeCount == 0 || graph.nodes != nullptr, "BaselineDmlProgram has no node table");
	require(graph.inputEdgeCount == 0 || graph.inputEdges != nullptr, "BaselineDmlProgram has no input-edge table");
	require(graph.intermediateEdgeCount == 0 || graph.intermediateEdges != nullptr,
		"BaselineDmlProgram has no intermediate-edge table");
	require(graph.outputEdgeCount == 0 || graph.outputEdges != nullptr,
		"BaselineDmlProgram has no output-edge table");

	Builder builder(device_, weights);
	std::vector<TensorId> graphInputs(graph.inputCount);
	for (std::uint32_t index = 0; index < graph.inputCount; ++index) {
		const auto &input = graph.inputs[index];
		require(input.dimensions.count != 0, "BaselineDmlProgram input has no shape");
		graphInputs[index] = input.weightOffset != BaselineDml::noIndex
					     ? builder.weight(input.dimensions.count, input.dimensions.values.data(),
							      input.weightOffset)
					     : builder.input(input.dimensions.count, input.dimensions.values.data());
	}

	std::vector<std::vector<TensorId>> nodeInputs(graph.nodeCount);
	std::vector<std::vector<std::uint32_t>> successors(graph.nodeCount);
	std::vector<std::uint32_t> indegrees(graph.nodeCount, 0);
	for (std::uint32_t index = 0; index < graph.inputEdgeCount; ++index) {
		const auto &edge = graph.inputEdges[index];
		require(edge.graphInput < graph.inputCount && edge.toNode < graph.nodeCount,
			"BaselineDmlProgram input edge is out of range");
		auto &inputs = nodeInputs[edge.toNode];
		if (inputs.size() <= edge.toNodeInput)
			inputs.resize(edge.toNodeInput + 1, noTensor);
		require(inputs[edge.toNodeInput] == noTensor, "BaselineDmlProgram node input has multiple edges");
		inputs[edge.toNodeInput] = graphInputs[edge.graphInput];
	}
	for (std::uint32_t index = 0; index < graph.intermediateEdgeCount; ++index) {
		const auto &edge = graph.intermediateEdges[index];
		require(edge.fromNode < graph.nodeCount && edge.toNode < graph.nodeCount,
			"BaselineDmlProgram intermediate edge is out of range");
		require(edge.fromNodeOutput == 0, "BaselineDmlProgram supports one output per node");
		auto &inputs = nodeInputs[edge.toNode];
		if (inputs.size() <= edge.toNodeInput)
			inputs.resize(edge.toNodeInput + 1, noTensor);
		require(inputs[edge.toNodeInput] == noTensor, "BaselineDmlProgram node input has multiple edges");
		successors[edge.fromNode].push_back(edge.toNode);
		++indegrees[edge.toNode];
	}

	std::deque<std::uint32_t> ready;
	for (std::uint32_t index = 0; index < graph.nodeCount; ++index)
		if (indegrees[index] == 0)
			ready.push_back(index);
	std::vector<TensorId> outputs(graph.nodeCount, noTensor);
	std::uint32_t compiledCount = 0;
	while (!ready.empty()) {
		const auto nodeIndex = ready.front();
		ready.pop_front();
		const auto &node = graph.nodes[nodeIndex];
		auto &inputs = nodeInputs[nodeIndex];
		require(node.constantCount <= BaselineDml::maximumConstants,
			"BaselineDml node has too many constant inputs");
		for (std::uint32_t index = 0; index < node.constantCount; ++index) {
			const auto &constant = node.constants[index];
			if (inputs.size() <= constant.inputIndex)
				inputs.resize(constant.inputIndex + 1, noTensor);
			require(inputs[constant.inputIndex] == noTensor,
				"BaselineDml node input has both an edge and a constant");
			const auto &tensor = constant.tensor;
			require(tensor.weightOffset != BaselineDml::noIndex && tensor.dimensions.count != 0,
				"BaselineDml constant input is invalid");
			inputs[constant.inputIndex] = builder.weight(
				tensor.dimensions.count, tensor.dimensions.values.data(), tensor.weightOffset);
		}
		for (std::uint32_t edgeIndex = 0; edgeIndex < graph.intermediateEdgeCount; ++edgeIndex) {
			const auto &edge = graph.intermediateEdges[edgeIndex];
			if (edge.toNode == nodeIndex)
				inputs[edge.toNodeInput] = outputs[edge.fromNode];
		}
		for (auto input : inputs)
			require(input != noTensor, "BaselineDmlProgram node has an unconnected input");
		auto unary = [&]() {
			require(inputs.size() == 1, "BaselineDmlProgram unary node input count mismatch");
			return inputs[0];
		};
		const auto &output = node.output.dimensions;
		switch (node.type) {
		case BaselineDml::NodeType::reshape:
			outputs[nodeIndex] = builder.reshape(unary(), output.count, output.values.data());
			break;
		case BaselineDml::NodeType::transpose: {
			outputs[nodeIndex] = builder.transpose(unary(), node.first.count, node.first.values.data());
			break;
		}
		case BaselineDml::NodeType::add:
			require(inputs.size() == 2, "BaselineDmlProgram add input count mismatch");
			outputs[nodeIndex] = builder.add(inputs[0], inputs[1], output.count, output.values.data());
			break;
		case BaselineDml::NodeType::multiply:
			require(inputs.size() == 2, "BaselineDmlProgram multiply input count mismatch");
			outputs[nodeIndex] = builder.multiply(inputs[0], inputs[1], output.count, output.values.data());
			break;
		case BaselineDml::NodeType::clip: {
			outputs[nodeIndex] = builder.clip(unary(), node.minimum, node.maximum);
			break;
		}
		case BaselineDml::NodeType::relu:
			outputs[nodeIndex] = builder.relu(unary());
			break;
		case BaselineDml::NodeType::sigmoid:
			outputs[nodeIndex] = builder.sigmoid(unary());
			break;
		case BaselineDml::NodeType::convolution: {
			require(inputs.size() == 2 || inputs.size() == 3,
				"BaselineDmlProgram convolution input count mismatch");
			const auto direction = node.direction == BaselineDml::ConvolutionDirection::backward
						       ? DML_CONVOLUTION_DIRECTION_BACKWARD
						       : DML_CONVOLUTION_DIRECTION_FORWARD;
			outputs[nodeIndex] = builder.convolution(inputs[0], inputs[1],
								 inputs.size() == 3 ? inputs[2] : noTensor, direction,
								 node.first.count, node.first.values.data(),
								 node.second.values.data(), node.third.values.data(),
								 node.fourth.values.data(), node.value, output.count,
								 output.values.data());
			break;
		}
		case BaselineDml::NodeType::averagePool: {
			outputs[nodeIndex] = builder.averagePool(unary(), node.first.count, node.first.values.data(),
								 node.second.values.data(), node.third.values.data(),
								 node.fourth.values.data(), output.count,
								 output.values.data());
			break;
		}
		case BaselineDml::NodeType::gemm:
			require(inputs.size() == 2, "BaselineDmlProgram gemm input count mismatch");
			outputs[nodeIndex] = builder.gemm(inputs[0], inputs[1], output.count, output.values.data());
			break;
		case BaselineDml::NodeType::upsample: {
			outputs[nodeIndex] = builder.upsample(unary(), node.first.values[0], node.first.values[1],
							      output.count, output.values.data());
			break;
		}
		case BaselineDml::NodeType::join: {
			require(!inputs.empty(), "BaselineDmlProgram join has no inputs");
			outputs[nodeIndex] = builder.join(static_cast<UINT>(inputs.size()), inputs.data(), node.value,
							  output.count, output.values.data());
			break;
		}
		}
		++compiledCount;
		for (auto successor : successors[nodeIndex])
			if (--indegrees[successor] == 0)
				ready.push_back(successor);
	}
	require(compiledCount == graph.nodeCount, "BaselineDmlProgram graph contains a cycle");
	require(graph.outputCount == 1 && graph.outputEdgeCount == 1, "BaselineDmlProgram requires one graph output");
	const auto &outputEdge = graph.outputEdges[0];
	require(outputEdge.fromNode < graph.nodeCount && outputEdge.fromNodeOutput == 0 && outputEdge.graphOutput == 0,
		"BaselineDmlProgram output edge is invalid");
	builder.setOutput(outputs[outputEdge.fromNode]);
	return builder.finish();
}

} // namespace BackgroundRemoval
