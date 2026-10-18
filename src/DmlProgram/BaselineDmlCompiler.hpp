// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BaselineDmlProgramRuntime.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef DML_TARGET_VERSION
#define DML_TARGET_VERSION 0x1000
#endif
#include <DirectML.h>
#include <wrl/client.h>

namespace BaselineDml {

using TensorId = std::int64_t;
struct CompiledTensor final {
	std::uint32_t dataType = ProgramDefinition::dtUnknown;
	std::vector<std::uint32_t> sizes;
	std::vector<std::uint32_t> strides;
	std::uint64_t size = 0;
	std::uint64_t dataBytes = 0;
	std::int64_t storage = 0;
	bool weight = false;
	std::int64_t byteOffset = 0;
	std::uint64_t weightOffset = 0;
};

struct CompiledStorage final {
	std::uint64_t size = 0;
};

enum class CompiledShader { None, MPLD10DepthwiseConv2dRelu, MPLD10BinarizeFloat32 };

struct CompiledStep final {
	std::vector<TensorId> inputs;
	// -1 denotes a barrier-only step, without a GPU dispatch.
	TensorId output = -1;
	std::vector<TensorId> barriersBefore;
	Microsoft::WRL::ComPtr<IDMLCompiledOperator> operation;
	CompiledShader shader = CompiledShader::None;
};

struct CompiledDmlProgram final {
	std::vector<CompiledTensor> tensors;
	std::vector<CompiledStorage> storages;
	std::vector<CompiledStep> steps;
	std::vector<TensorId> inputTensors;
	TensorId outputTensor = -1;
	std::uint64_t weightBytes = 0;
};

class BaselineDmlCompiler final {
public:
	explicit BaselineDmlCompiler(IDMLDevice *device) noexcept;
	auto compile(std::span<const ProgramDefinition::InputSpecItem> inputSpecs,
		     std::span<const ProgramDefinition::Node> graphNodes,
		     std::span<const std::pair<std::int32_t, std::int32_t>> uavBarriers, std::int32_t outputIndex,
		     std::span<const std::byte> weights) const -> CompiledDmlProgram;

private:
	IDMLDevice *device_;
};

} // namespace BaselineDml
