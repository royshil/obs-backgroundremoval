// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include <BackgroundRemoval/windows/MediaPipeLandscapeDirectML10/MPLD10Model.hpp>
#include "ProgramTestSession.hpp"

#include <algorithm>
#include <memory>

namespace LandscapeTestModel {

// These prefixes deliberately depend on the pinned model's internal layout.
// Production always compiles the complete graph, including mask postprocessing.
inline const std::int32_t rawOutputIndex = BackgroundRemoval::MPLD10Detail::rawOutputIndex;

inline auto tensorSpecs()
{
	const auto specs = std::span(BackgroundRemoval::MPLD10Detail::tensorSpecs);
	return specs.first(specs.size() - 1);
}

inline auto nodes()
{
	return std::span(BackgroundRemoval::MPLD10Detail::graphNodes).first(rawOutputIndex + 1);
}

inline auto barriers(std::size_t nodeCount = rawOutputIndex + 1)
{
	const auto entries = std::span(BackgroundRemoval::MPLD10Detail::uavBarriers);
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
	return std::make_unique<DmlProgramTest::ProgramTestSession>(tensorSpecs(), nodes(), barriers(), rawOutputIndex,
								    weights());
}

inline auto makeMaskRuntime()
{
	return std::make_unique<DmlProgramTest::ProgramTestSession>(
		BackgroundRemoval::MPLD10Detail::graphNodes, BackgroundRemoval::MPLD10Detail::modelParams, weights());
}

} // namespace LandscapeTestModel
