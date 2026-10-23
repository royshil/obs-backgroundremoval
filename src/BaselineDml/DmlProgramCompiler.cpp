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

#include "DmlProgramCompiler.hpp"
#include "DmlProgramCompilerBuilder.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <variant>
#include <vector>

#include <TraceLoggingActivity.h>
#include <TraceLoggingProvider.h>

#include "Detail.hpp"

TRACELOGGING_DECLARE_PROVIDER(g_hProvider);

using namespace BaselineDml::Definition;

namespace BaselineDml {

DmlProgramCompiler::DmlProgramCompiler(IDMLDevice *device, std::span<const Node> nodes, const ModelParams &params,
				       std::span<const std::byte> weights)
{
	if (!device)
		throw std::invalid_argument("device is null");
	if (weights.size() != params.sourceWeightBytes)
		throw std::invalid_argument("source weight byte count differs from the prepared model");
	if (params.tensors.size() != nodes.size() || params.placements.size() != nodes.size())
		throw std::invalid_argument("each node must define exactly one tensor");
	TraceLoggingThreadActivity<g_hProvider, 0x1> scope;
	TraceLoggingWriteStart(scope, "BDPCompiler.Build");
	std::array<bool, dtInt64 + 1> checkedTypes{};
	for (std::size_t index = 0; index < params.tensors.size(); ++index) {
		const auto &spec = params.tensors[index];
		validateTensorSpec(spec);
		const auto dataType = tensorDataType(spec);
		if (!checkedTypes[dataType]) {
			DML_FEATURE_QUERY_TENSOR_DATA_TYPE_SUPPORT query{static_cast<DML_TENSOR_DATA_TYPE>(dataType)};
			DML_FEATURE_DATA_TENSOR_DATA_TYPE_SUPPORT support{};
			if (const HRESULT result = device->CheckFeatureSupport(DML_FEATURE_TENSOR_DATA_TYPE_SUPPORT,
									       sizeof(query), &query, sizeof(support),
									       &support);
			    FAILED(result))
				throw Errors::featureSupportError(result, std::nullopt, std::nullopt, dataType);
			if (!support.IsSupported)
				throw Errors::unsupportedTensorDataType(std::nullopt, std::nullopt, dataType);
			checkedTypes[dataType] = true;
		}
	}
	for (const auto &definition : params.steps) {
		Detail::Step step;
		if (definition.output != Detail::noTensor) {
			Builder builder(device, params.tensors, definition, step, internalTensors_);
			std::visit(builder, nodes[static_cast<std::size_t>(definition.nodeIndex)]);
		}
		steps_.push_back(std::move(step));
	}
}

DmlProgramCompiler::~DmlProgramCompiler() noexcept = default;

auto DmlProgramCompiler::takeSteps() noexcept -> std::vector<Detail::Step>
{
	return std::move(steps_);
}
auto DmlProgramCompiler::takeInternalTensors() noexcept -> Detail::InternalTensors
{
	return std::move(internalTensors_);
}

} // namespace BaselineDml
