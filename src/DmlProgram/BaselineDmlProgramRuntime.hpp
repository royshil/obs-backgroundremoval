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
	std::size_t inputSpecCount;
	const ProgramDefinition::InputSpecItem *inputSpecs;
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

	// Inputs follow RuntimeInput entries in the specification table, excluding weights.
	// Spans contain packed elements, without DirectML's buffer padding.
	void run(std::span<const std::span<const std::byte>> inputs, std::span<std::byte> output);

private:
	class Impl;
	std::unique_ptr<Impl> impl_;
};

} // namespace BaselineDml
