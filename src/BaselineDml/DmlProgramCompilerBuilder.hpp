// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "Detail.hpp"

#include <array>
#include <initializer_list>

namespace BaselineDml {

using namespace Definition;

class DmlProgramCompiler::Builder final {
public:
	Builder(IDMLDevice *device, std::span<const TensorSpec> tensors, const StepDefinition &definition,
		Detail::Step &step, Detail::InternalTensors &internalTensors);
	auto operator()(const InputItem &node) -> std::int64_t;
	auto operator()(const BiasedConv2dRelu &node) -> std::int64_t;
	auto operator()(const HardSwishGate &node) -> std::int64_t;
	auto operator()(const MPLD10DepthwiseConv2dRelu &node) -> std::int64_t;
	auto operator()(const MPLD10BinarizeFloat32 &node) -> std::int64_t;
	auto operator()(const AveragePool2d &node) -> std::int64_t;
	auto operator()(const BiasedConv2d &node) -> std::int64_t;
	auto operator()(const BiasedConvTranspose2d &node) -> std::int64_t;
	auto operator()(const DepthToSpace &node) -> std::int64_t;
	auto operator()(const ClipFloat32 &node) -> std::int64_t;
	auto operator()(const Resize &node) -> std::int64_t;
	auto operator()(const Sigmoid &node) -> std::int64_t;
	auto operator()(const Reshape &node) -> std::int64_t;
	auto operator()(const Expand &node) -> std::int64_t;
	auto operator()(const Add &node) -> std::int64_t;
	auto operator()(const Mul &node) -> std::int64_t;
	auto operator()(const Relu &node) -> std::int64_t;
	auto operator()(const FusedNode &nodes) -> std::int64_t;
	auto operator()(const Onnx11Node &nodes) -> std::int64_t;
	auto operator()(const Onnx13Node &nodes) -> std::int64_t;
	auto operator()(const Onnx14Node &nodes) -> std::int64_t;

private:
	auto compile(const DML_OPERATOR_DESC &operatorDesc, std::initializer_list<std::int64_t> inputs,
		     std::int64_t output) -> std::int64_t;
	IDMLDevice *device_;
	std::span<const TensorSpec> tensors_;
	const StepDefinition &definition_;
	Detail::Step &step_;
	// Only keys encoded from definition_.nodeIndex and this operation's fixed local IDs may be accessed.
	Detail::InternalTensors &internalTensors_;
};
} // namespace BaselineDml
