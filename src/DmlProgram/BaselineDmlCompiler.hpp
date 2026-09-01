// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "BaselineDmlProgram.hpp"

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

namespace BackgroundRemoval {

using TensorId = std::uint32_t;
constexpr std::uint32_t maximumTensorRank = 4;

struct CompiledTensor final {
	std::uint32_t dimensionCount = 0;
	std::array<std::uint32_t, maximumTensorRank> sizes{};
	std::array<std::uint32_t, maximumTensorRank> strides{};
	std::uint64_t size = 0;
	std::uint32_t storage = 0;
	bool weight = false;
	std::size_t weightCount = 0;
	std::uint32_t weightSourceOffset = 0;
	std::uint64_t weightOffset = 0;
};

struct CompiledStorage final {
	std::uint64_t size = 0;
};

struct CompiledStep final {
	std::vector<TensorId> inputs;
	TensorId output = UINT32_MAX;
	Microsoft::WRL::ComPtr<IDMLCompiledOperator> operation;
};

struct CompiledDmlProgram final {
	std::vector<CompiledTensor> tensors;
	std::vector<CompiledStorage> storages;
	std::vector<CompiledStep> steps;
	TensorId inputTensor = UINT32_MAX;
	TensorId outputTensor = UINT32_MAX;
	std::uint64_t weightBytes = 0;
};

class BaselineDmlCompiler final {
public:
	explicit BaselineDmlCompiler(IDMLDevice *device) noexcept;
	auto compile(const BaselineDml::Graph &graph, std::span<const std::byte> weights) const -> CompiledDmlProgram;

private:
	IDMLDevice *device_;
};

} // namespace BackgroundRemoval
