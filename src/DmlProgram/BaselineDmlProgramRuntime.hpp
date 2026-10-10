// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "BaselineDmlProgramDefinition.hpp"

#include <cstddef>
#include <memory>
#include <span>

namespace BaselineDml {

// Execution selection is separate from the static operator/node definitions.
struct Graph final {
	std::int64_t nodeCount;
	const ProgramDefinition::Node *nodes;
	std::size_t outputIndex;
};

class BaselineDmlProgramRuntime final {
public:
	explicit BaselineDmlProgramRuntime(const Graph &graph, std::span<const std::byte> weights);
	~BaselineDmlProgramRuntime() noexcept;

	BaselineDmlProgramRuntime(const BaselineDmlProgramRuntime &) = delete;
	auto operator=(const BaselineDmlProgramRuntime &) -> BaselineDmlProgramRuntime & = delete;
	BaselineDmlProgramRuntime(BaselineDmlProgramRuntime &&) = delete;
	auto operator=(BaselineDmlProgramRuntime &&) -> BaselineDmlProgramRuntime & = delete;

	void run(std::span<const float> input, std::span<float> output);

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace BaselineDml
