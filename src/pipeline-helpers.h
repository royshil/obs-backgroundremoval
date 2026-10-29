// SPDX-FileCopyrightText: 2026 Xavier Ruiz <github@xav.ie>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

namespace pipeline {

// Single resize of the full frame.
inline cv::Mat preprocess_resize(const cv::Mat &imageBGRA, int width, int height)
{
	cv::Mat resized;
	cv::resize(imageBGRA, resized, cv::Size(width, height), 0, 0, cv::INTER_NEAREST);
	return resized;
}

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
