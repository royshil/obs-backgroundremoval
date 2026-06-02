// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MASK_POST_PROCESSING_HPP
#define MASK_POST_PROCESSING_HPP

#include <opencv2/core.hpp>

namespace background_removal {

struct MaskPostProcessingSettings {
	bool enableThreshold = true;
	float threshold = 0.5f;
	float edgeSoftness = 0.0f;
	float foregroundCleanup = 0.0f;
	float contourFilter = 0.05f;
	float smoothContour = 0.5f;
	float feather = 0.0f;
	int maskExpansion = 0;
};

struct TemporalMaskSmoothingSettings {
	float temporalSmoothFactor = 0.0f;
	bool protectForeground = true;
};

cv::Mat postProcessForegroundMask(const cv::Mat &foregroundMask, const cv::Size &targetSize,
				  const MaskPostProcessingSettings &settings);

cv::Mat smoothTemporalBackgroundMask(const cv::Mat &currentBackgroundMask, const cv::Mat &previousBackgroundMask,
				     const TemporalMaskSmoothingSettings &settings);

} // namespace background_removal

#endif // MASK_POST_PROCESSING_HPP
