// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstddef>

#include <BaselineDml/DmlProgramDefinition.hpp>

namespace BackgroundRemoval::MPLD10Detail {

extern const std::array<BaselineDml::Definition::TensorSpec, 112> tensorSpecs;
extern const std::array<BaselineDml::Definition::Node, 250> graphNodes;
extern const std::array<std::pair<std::int32_t, std::int32_t>, 127> uavBarriers;
extern const char weightsData[];
extern const std::size_t weightsDataSize;
extern const BaselineDml::Definition::ModelParams modelParams;
extern const BaselineDml::DmlProgramDefinition modelDefinition;
extern const std::int32_t rawOutputIndex;
extern const std::int32_t maskOutputIndex;

} // namespace BackgroundRemoval::MPLD10Detail
