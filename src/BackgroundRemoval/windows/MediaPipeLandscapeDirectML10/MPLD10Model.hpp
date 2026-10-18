// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <array>
#include <cstddef>

#include <DmlProgram/BaselineDmlProgramDefinition.hpp>

namespace BackgroundRemoval::MPLD10Detail {

extern const std::array<BaselineDml::ProgramDefinition::InputSpecItem, 112> inputSpecs;
extern const std::array<BaselineDml::ProgramDefinition::Node, 240> graphNodes;
extern const std::array<std::pair<std::int32_t, std::int32_t>, 127> uavBarriers;
extern const char weightsData[];
extern const std::size_t weightsDataSize;

} // namespace BackgroundRemoval::MPLD10Detail
