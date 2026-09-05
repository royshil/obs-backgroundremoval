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

namespace BaselineDml {

using TensorId = std::int64_t;
struct CompiledTensor final {
	std::vector<std::uint32_t> sizes;
	std::vector<std::uint32_t> strides;
	std::uint64_t size = 0;
	std::int64_t storage = 0;
	bool weight = false;
	std::int64_t weightCount = 0;
	std::int64_t byteOffset = 0;
	std::uint64_t weightOffset = 0;
};

struct CompiledStorage final {
	std::uint64_t size = 0;
};

struct CompiledStep final {
	std::vector<TensorId> inputs;
	TensorId output = -1;
	Microsoft::WRL::ComPtr<IDMLCompiledOperator> operation;
};

struct CompiledDmlProgram final {
	std::vector<CompiledTensor> tensors;
	std::vector<CompiledStorage> storages;
	std::vector<CompiledStep> steps;
	TensorId inputTensor = -1;
	TensorId outputTensor = -1;
	std::uint64_t weightBytes = 0;
};

class BaselineDmlCompiler final {
public:
	explicit BaselineDmlCompiler(IDMLDevice *device) noexcept;
	auto compile(const Graph &graph, std::span<const std::byte> weights) const -> CompiledDmlProgram;

private:
	IDMLDevice *device_;
};

} // namespace BaselineDml
