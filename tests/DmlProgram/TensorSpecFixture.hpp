// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <BaselineDml/DmlProgramDefinition.hpp>

namespace DmlProgramTest {

// Convenience for hand-written test graphs; production TensorSpecs are emitted by Python.
constexpr auto makeInputTensorSpec(std::uint32_t dataType, std::int32_t rank, const std::array<std::int32_t, 4> &shape)
	-> BaselineDml::Definition::TensorSpec
{
	BaselineDml::Definition::InputTensorSpec spec;
	spec.dataType = dataType;
	BaselineDml::setContiguousShape(spec, BaselineDml::rankedShape(shape, rank, {}));
	return spec;
}

constexpr auto makeWeightTensorSpec(std::int64_t byteOffset, std::uint32_t dataType, std::int32_t rank,
				    const std::array<std::int32_t, 4> &shape) -> BaselineDml::Definition::TensorSpec
{
	BaselineDml::Definition::WeightTensorSpec spec;
	spec.dataType = dataType;
	BaselineDml::setContiguousShape(spec, BaselineDml::rankedShape(shape, rank, {}));
	spec.byteOffset = byteOffset;
	return spec;
}

} // namespace DmlProgramTest
