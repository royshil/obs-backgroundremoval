// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <BaselineDml/DmlProgramOperations.hpp>

#include <cstdint>
#include <memory>
#include <vector>

namespace DmlProgramTest {

void check(std::int32_t result, const char *message);

// Test/benchmark-owned CPU transfers and GPU submission/completion.
class ProgramTestSession final {
public:
	ProgramTestSession(std::span<const BaselineDml::Definition::Node> nodes,
			   const BaselineDml::Definition::ModelParams &params, std::span<const std::byte> weights);
	ProgramTestSession(std::span<const BaselineDml::Definition::TensorSpec> specs,
			   std::span<const BaselineDml::Definition::Node> nodes,
			   std::span<const std::pair<std::int32_t, std::int32_t>> barriers, std::int32_t outputIndex,
			   std::span<const std::byte> weights);
	~ProgramTestSession() noexcept;

	ProgramTestSession(const ProgramTestSession &) = delete;
	auto operator=(const ProgramTestSession &) -> ProgramTestSession & = delete;
	ProgramTestSession(ProgramTestSession &&) = delete;
	auto operator=(ProgramTestSession &&) -> ProgramTestSession & = delete;

	auto device() const noexcept -> ID3D12Device *;
	auto inputInfo(std::size_t index) const noexcept -> BaselineDml::TensorInfo;
	auto outputInfo() const noexcept -> BaselineDml::TensorInfo;
	void run(std::span<const std::span<const std::byte>> inputs, std::span<std::byte> output);

private:
	class Impl;
	BaselineDml::Definition::DynamicModelParamsStorage tables_;
	std::vector<BaselineDml::Definition::Node> nodes_;
	std::vector<std::byte> weights_;
	std::unique_ptr<Impl> impl_;
};

} // namespace DmlProgramTest
