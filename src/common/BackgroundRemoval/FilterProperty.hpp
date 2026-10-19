// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <obs-module.h>

namespace BackgroundRemoval {

struct FilterProperty {
	long long pipelineId;
	double imageSimilarityThreshold;
	bool refineMaskEnabled;
	double refineMaskThreshold;
	double refineMaskContour;
	long long refineMaskExpansion;
	bool blurBackgroundEnabled;
	long long blurBackgroundFactor;
	double blurFocusPoint;
	double blurFocusDepth;
};

auto decodeFilterProperty(obs_data_t *settings) noexcept -> FilterProperty;
void encodeFilterProperty(obs_data_t *settings, const FilterProperty &property) noexcept;

} // namespace BackgroundRemoval
