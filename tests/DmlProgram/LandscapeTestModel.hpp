// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <BackgroundRemoval/windows/MediaPipeLandscapeDirectML10/MPLD10Model.hpp>
#include <DmlProgram/BaselineDmlProgramRuntime.hpp>

#include <algorithm>
#include <memory>

namespace LandscapeTestModel {

// These prefixes deliberately depend on the pinned model's internal layout.
// Production always compiles all 240 nodes, including mask postprocessing.
inline constexpr std::int32_t rawOutputIndex = 236;

inline auto inputSpecs()
{
	return std::span(BackgroundRemoval::MPLD10Detail::inputSpecs).first(111);
}

inline auto nodes()
{
	return std::span(BackgroundRemoval::MPLD10Detail::graphNodes).first(237);
}

inline auto barriers(std::size_t nodeCount = 237)
{
	const auto entries = std::span(BackgroundRemoval::MPLD10Detail::uavBarriers).first(125);
	const auto end =
		std::lower_bound(entries.begin(), entries.end(), nodeCount,
				 [](const auto &entry, auto boundary) { return std::cmp_less(entry.first, boundary); });
	return entries.first(end - entries.begin());
}

inline auto weights()
{
	return std::as_bytes(std::span(BackgroundRemoval::MPLD10Detail::weightsData,
				       BackgroundRemoval::MPLD10Detail::weightsDataSize));
}

inline auto makeRawRuntime()
{
	return std::make_unique<BaselineDml::BaselineDmlProgramRuntime>(inputSpecs(), nodes(), barriers(),
									rawOutputIndex, weights());
}

inline auto makeMaskRuntime()
{
	return std::make_unique<BaselineDml::BaselineDmlProgramRuntime>(BackgroundRemoval::MPLD10Detail::inputSpecs,
									BackgroundRemoval::MPLD10Detail::graphNodes,
									BackgroundRemoval::MPLD10Detail::uavBarriers,
									239, weights());
}

} // namespace LandscapeTestModel
