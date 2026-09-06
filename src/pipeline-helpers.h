// SPDX-FileCopyrightText: 2026 Xavier Ruiz <github@xav.ie>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <opencv2/core.hpp>

namespace pipeline {

// Similarity check: skip processing when the frame is nearly identical
// to the previous one (PSNR above threshold).
inline bool check_similarity(const cv::Mat &current, cv::Mat &lastImage, double threshold)
{
	if (!lastImage.empty() && lastImage.size() == current.size()) {
		if (cv::PSNR(lastImage, current) > threshold)
			return true;
	}
	current.copyTo(lastImage);
	return false;
}

} // namespace pipeline
