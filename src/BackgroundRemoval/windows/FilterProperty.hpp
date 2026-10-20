// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <obs-module.h>

namespace BackgroundRemoval {

struct FilterProperty {
	long long pipelineId = 62828;
	double imageSimilarityThreshold = 60.0;
	bool refineMaskEnabled = false;
	double refineMaskThreshold = 0.5;
	double refineMaskContour = 0.0;
	bool blurBackgroundEnabled = false;
	long long blurBackgroundFactor = 4;
	double blurFocusPoint = 0.1;
	double blurFocusDepth = 0.0;
};

auto decodeFilterProperty(obs_data_t *settings) noexcept -> FilterProperty;
void encodeFilterProperty(obs_data_t *settings, const FilterProperty &property) noexcept;

} // namespace BackgroundRemoval
