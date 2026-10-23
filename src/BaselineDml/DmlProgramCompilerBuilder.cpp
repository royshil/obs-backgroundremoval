// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include "DmlProgramCompilerBuilder.hpp"

#include <algorithm>
#include <cstring>
#include <limits>
#include <utility>
#include <variant>

namespace BaselineDml {

using namespace Detail;

DmlProgramCompiler::Builder::Builder(IDMLDevice *device, std::span<const TensorSpec> tensors,
				     const StepDefinition &definition, Step &step, InternalTensors &internalTensors)
	: device_(device),
	  tensors_(tensors),
	  definition_(definition),
	  step_(step),
	  internalTensors_(internalTensors)
{
}

auto DmlProgramCompiler::Builder::operator()(const InputItem &node) -> std::int64_t
{
	return definition_.output;
}

auto DmlProgramCompiler::Builder::operator()(const BiasedConv2dRelu &node) -> std::int64_t
{
	const auto input = definition_.inputs[0];
	const auto filter = definition_.inputs[1];
	const auto bias = definition_.inputs[2];
	const auto output = definition_.output;
	TensorDescriptor inDesc(tensors_[input]), filterDesc(tensors_[filter]), outDesc(tensors_[output]);
	ConvolutionBiasDescriptor biasDesc(tensors_[bias]);
	const std::array<UINT, 2> outputPadding{};
	DML_CONVOLUTION_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc.tensor;
	desc.FilterTensor = &filterDesc.tensor;
	desc.BiasTensor = &biasDesc.descriptor.tensor;
	desc.OutputTensor = &outDesc.tensor;
	desc.Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION;
	desc.Direction = DML_CONVOLUTION_DIRECTION_FORWARD;
	desc.DimensionCount = 2;
	desc.Strides = node.strides.data();
	desc.Dilations = node.dilations.data();
	desc.StartPadding = node.startPadding.data();
	desc.EndPadding = node.endPadding.data();
	desc.OutputPadding = outputPadding.data();
	desc.GroupCount = node.groupCount;
	DML_ACTIVATION_RELU_OPERATOR_DESC relu{};
	DML_OPERATOR_DESC activation{DML_OPERATOR_ACTIVATION_RELU, &relu};
	desc.FusedActivation = &activation;
	compile({DML_OPERATOR_CONVOLUTION, &desc}, {input, filter, bias}, output);
	return output;
}

auto DmlProgramCompiler::Builder::operator()(const HardSwishGate &node) -> std::int64_t
{
	const auto input = definition_.inputs[0];
	const auto output = definition_.output;
	TensorDescriptor inDesc(tensors_[input]), outDesc(tensors_[output]);
	DML_ACTIVATION_HARD_SIGMOID_OPERATOR_DESC desc{&inDesc.tensor, &outDesc.tensor, 1.0F / 6.0F, 0.5F};
	compile({DML_OPERATOR_ACTIVATION_HARD_SIGMOID, &desc}, {input}, output);
	return output;
}

auto DmlProgramCompiler::Builder::operator()(const MPLD10DepthwiseConv2dRelu &node) -> std::int64_t
{
	return definition_.output;
}

auto DmlProgramCompiler::Builder::operator()(const MPLD10BinarizeFloat32 &node) -> std::int64_t
{
	return definition_.output;
}

auto DmlProgramCompiler::Builder::operator()(const AveragePool2d &node) -> std::int64_t
{
	const auto input = definition_.inputs[0];
	const auto output = definition_.output;
	TensorDescriptor inDesc(tensors_[input]), outDesc(tensors_[output]);
	DML_AVERAGE_POOLING_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc.tensor;
	desc.OutputTensor = &outDesc.tensor;
	desc.DimensionCount = 2;
	desc.Strides = node.strides.data();
	desc.WindowSize = node.window.data();
	desc.StartPadding = node.startPadding.data();
	desc.EndPadding = node.endPadding.data();
	desc.IncludePadding = FALSE;
	compile({DML_OPERATOR_AVERAGE_POOLING, &desc}, {input}, output);
	return output;
}

auto DmlProgramCompiler::Builder::operator()(const BiasedConv2d &node) -> std::int64_t
{
	const auto input = definition_.inputs[0];
	const auto filter = definition_.inputs[1];
	const auto bias = definition_.inputs[2];
	const auto output = definition_.output;
	TensorDescriptor inDesc(tensors_[input]), filterDesc(tensors_[filter]), outDesc(tensors_[output]);
	ConvolutionBiasDescriptor biasDesc(tensors_[bias]);
	const std::array<UINT, 2> outputPadding{};
	DML_CONVOLUTION_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc.tensor;
	desc.FilterTensor = &filterDesc.tensor;
	desc.BiasTensor = &biasDesc.descriptor.tensor;
	desc.OutputTensor = &outDesc.tensor;
	desc.Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION;
	desc.Direction = DML_CONVOLUTION_DIRECTION_FORWARD;
	desc.DimensionCount = 2;
	desc.Strides = node.strides.data();
	desc.Dilations = node.dilations.data();
	desc.StartPadding = node.startPadding.data();
	desc.EndPadding = node.endPadding.data();
	desc.OutputPadding = outputPadding.data();
	desc.GroupCount = node.groupCount;

	compile({DML_OPERATOR_CONVOLUTION, &desc}, {input, filter, bias}, output);
	return output;
}

auto DmlProgramCompiler::Builder::operator()(const BiasedConvTranspose2d &node) -> std::int64_t
{
	const auto input = definition_.inputs[0];
	const auto filter = definition_.inputs[1];
	const auto bias = definition_.inputs[2];
	const auto output = definition_.output;
	TensorDescriptor inDesc(tensors_[input]), filterDesc(tensors_[filter]), outDesc(tensors_[output]);
	ConvolutionBiasDescriptor biasDesc(tensors_[bias]);
	const std::array<UINT, 2> outputPadding{};
	DML_CONVOLUTION_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc.tensor;
	desc.FilterTensor = &filterDesc.tensor;
	desc.BiasTensor = &biasDesc.descriptor.tensor;
	desc.OutputTensor = &outDesc.tensor;
	desc.Mode = DML_CONVOLUTION_MODE_CROSS_CORRELATION;
	desc.Direction = DML_CONVOLUTION_DIRECTION_BACKWARD;
	desc.DimensionCount = 2;
	desc.Strides = node.strides.data();
	desc.Dilations = node.dilations.data();
	desc.StartPadding = node.startPadding.data();
	desc.EndPadding = node.endPadding.data();
	desc.OutputPadding = outputPadding.data();
	desc.GroupCount = node.groupCount;

	compile({DML_OPERATOR_CONVOLUTION, &desc}, {input, filter, bias}, output);
	return output;
}

auto DmlProgramCompiler::Builder::operator()(const DepthToSpace &node) -> std::int64_t
{
	const auto input = definition_.inputs[0];
	const auto output = definition_.output;
	TensorDescriptor xd(tensors_[input]), yd(tensors_[output]);
	DML_DEPTH_TO_SPACE_OPERATOR_DESC desc{&xd.tensor, &yd.tensor, node.blockSize};
	compile({DML_OPERATOR_DEPTH_TO_SPACE, &desc}, {input}, output);
	return output;
}

auto DmlProgramCompiler::Builder::operator()(const ClipFloat32 &node) -> std::int64_t
{
	const auto input = definition_.inputs[0];
	const auto output = definition_.output;
	TensorDescriptor inDesc(tensors_[input]), outDesc(tensors_[output]);
	DML_ELEMENT_WISE_CLIP_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc.tensor;
	desc.OutputTensor = &outDesc.tensor;
	desc.Min = node.minimum;
	desc.Max = node.maximum;
	compile({DML_OPERATOR_ELEMENT_WISE_CLIP, &desc}, {input}, output);
	return output;
}

auto DmlProgramCompiler::Builder::operator()(const Resize &node) -> std::int64_t
{
	const auto input = definition_.inputs[0];
	const auto output = definition_.output;
	TensorDescriptor inDesc(tensors_[input]), outDesc(tensors_[output]);
	DML_UPSAMPLE_2D_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc.tensor;
	desc.OutputTensor = &outDesc.tensor;
	desc.ScaleSize = {node.width, node.height};
	desc.InterpolationMode = DML_INTERPOLATION_MODE_LINEAR;
	compile({DML_OPERATOR_UPSAMPLE_2D, &desc}, {input}, output);
	return output;
}

auto DmlProgramCompiler::Builder::operator()(const Sigmoid &node) -> std::int64_t
{
	const auto input = definition_.inputs[0];
	const auto output = definition_.output;
	TensorDescriptor inDesc(tensors_[input]), outDesc(tensors_[output]);
	DML_ACTIVATION_SIGMOID_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc.tensor;
	desc.OutputTensor = &outDesc.tensor;
	compile({DML_OPERATOR_ACTIVATION_SIGMOID, &desc}, {input}, output);
	return output;
}

auto DmlProgramCompiler::Builder::operator()(const Reshape &) -> std::int64_t
{
	return definition_.output;
}
auto DmlProgramCompiler::Builder::operator()(const Expand &) -> std::int64_t
{
	return definition_.output;
}

auto DmlProgramCompiler::Builder::operator()(const Add &node) -> std::int64_t
{
	const auto left = definition_.inputs[0];
	const auto right = definition_.inputs[1];
	const auto output = definition_.output;
	TensorDescriptor leftDesc(tensors_[left]), rightDesc(tensors_[right]), outputDesc(tensors_[output]);
	DML_ELEMENT_WISE_ADD_OPERATOR_DESC desc{&leftDesc.tensor, &rightDesc.tensor, &outputDesc.tensor};
	compile({DML_OPERATOR_ELEMENT_WISE_ADD, &desc}, {left, right}, output);
	return output;
}

auto DmlProgramCompiler::Builder::operator()(const Mul &node) -> std::int64_t
{
	const auto left = definition_.inputs[0];
	const auto right = definition_.inputs[1];
	const auto output = definition_.output;
	TensorDescriptor leftDesc(tensors_[left]), rightDesc(tensors_[right]), outputDesc(tensors_[output]);
	DML_ELEMENT_WISE_MULTIPLY_OPERATOR_DESC desc{&leftDesc.tensor, &rightDesc.tensor, &outputDesc.tensor};
	compile({DML_OPERATOR_ELEMENT_WISE_MULTIPLY, &desc}, {left, right}, output);
	return output;
}

auto DmlProgramCompiler::Builder::operator()(const Relu &node) -> std::int64_t
{
	const auto input = definition_.inputs[0];
	const auto output = definition_.output;
	TensorDescriptor inDesc(tensors_[input]), outDesc(tensors_[output]);
	DML_ACTIVATION_RELU_OPERATOR_DESC desc{};
	desc.InputTensor = &inDesc.tensor;
	desc.OutputTensor = &outDesc.tensor;
	compile({DML_OPERATOR_ACTIVATION_RELU, &desc}, {input}, output);
	return output;
}

auto DmlProgramCompiler::Builder::operator()(const FusedNode &nodes) -> std::int64_t
{
	return std::visit(*this, nodes);
}

auto DmlProgramCompiler::Builder::operator()(const Onnx11Node &nodes) -> std::int64_t
{
	return std::visit(*this, nodes);
}

auto DmlProgramCompiler::Builder::operator()(const Onnx13Node &nodes) -> std::int64_t
{
	return std::visit(*this, nodes);
}

auto DmlProgramCompiler::Builder::operator()(const Onnx14Node &nodes) -> std::int64_t
{
	return std::visit(*this, nodes);
}

auto DmlProgramCompiler::Builder::compile(const DML_OPERATOR_DESC &operatorDesc, std::initializer_list<std::int64_t>,
					  std::int64_t output) -> std::int64_t
{
	ComOwner<IDMLOperator> operation;
	IDMLOperator *operationPointer = nullptr;
	const HRESULT operationResult = device_->CreateOperator(&operatorDesc, IID_PPV_ARGS(&operationPointer));
	operation.reset(operationPointer);
	if (const HRESULT result = operationResult; FAILED(result))
		throw Errors::operatorCreationError(result, definition_.nodeIndex);

	ComOwner<IDMLCompiledOperator> compiledOperation;
	IDMLCompiledOperator *compiledOperationPointer = nullptr;
	const HRESULT compiledOperationResult = device_->CompileOperator(operation.get(), DML_EXECUTION_FLAG_NONE,
									 IID_PPV_ARGS(&compiledOperationPointer));
	compiledOperation.reset(compiledOperationPointer);
	if (const HRESULT result = compiledOperationResult; FAILED(result))
		throw Errors::operatorCompilationError(result, definition_.nodeIndex);
	step_.operation = std::move(compiledOperation);
	return output;
}

} // namespace BaselineDml
