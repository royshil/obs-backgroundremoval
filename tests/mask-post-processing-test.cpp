// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "mask-post-processing.hpp"

#include <opencv2/core.hpp>

#include <cstdint>
#include <iostream>

namespace {

bool expectPixel(const cv::Mat &image, int row, int col, int expected, const char *label)
{
	const int actual = image.at<uint8_t>(row, col);
	if (actual == expected) {
		return true;
	}

	std::cerr << label << ": expected " << expected << ", got " << actual << "\n";
	return false;
}

bool expectRange(const cv::Mat &image, int row, int col, int minValue, int maxValue, const char *label)
{
	const int actual = image.at<uint8_t>(row, col);
	if (actual >= minValue && actual <= maxValue) {
		return true;
	}

	std::cerr << label << ": expected value in [" << minValue << ", " << maxValue << "], got " << actual << "\n";
	return false;
}

background_removal::MaskPostProcessingSettings baseSettings()
{
	background_removal::MaskPostProcessingSettings settings;
	settings.enableThreshold = true;
	settings.threshold = 0.5f;
	settings.edgeSoftness = 0.0f;
	settings.contourFilter = 0.0f;
	settings.smoothContour = 0.0f;
	settings.feather = 0.0f;
	settings.maskExpansion = 0;
	return settings;
}

} // namespace

int main()
{
	cv::Mat foreground(1, 5, CV_8UC1);
	foreground.at<uint8_t>(0, 0) = 0;
	foreground.at<uint8_t>(0, 1) = 64;
	foreground.at<uint8_t>(0, 2) = 128;
	foreground.at<uint8_t>(0, 3) = 192;
	foreground.at<uint8_t>(0, 4) = 255;

	auto settings = baseSettings();
	cv::Mat hardMask = background_removal::postProcessForegroundMask(foreground, foreground.size(), settings);
	bool success = true;
	success &= expectPixel(hardMask, 0, 0, 255, "hard background low confidence");
	success &= expectPixel(hardMask, 0, 1, 255, "hard background below threshold");
	success &= expectPixel(hardMask, 0, 2, 0, "hard foreground at threshold");
	success &= expectPixel(hardMask, 0, 3, 0, "hard foreground above threshold");
	success &= expectPixel(hardMask, 0, 4, 0, "hard foreground high confidence");

	settings.edgeSoftness = 0.5f;
	cv::Mat softMask = background_removal::postProcessForegroundMask(foreground, foreground.size(), settings);
	success &= expectPixel(softMask, 0, 0, 255, "soft background outside transition");
	success &= expectRange(softMask, 0, 2, 1, 254, "soft alpha inside transition");
	success &= expectPixel(softMask, 0, 4, 0, "soft foreground outside transition");

	cv::Mat resizedMask = background_removal::postProcessForegroundMask(foreground, cv::Size(10, 3), settings);
	if (resizedMask.size() != cv::Size(10, 3)) {
		std::cerr << "resize target: expected 10x3, got " << resizedMask.cols << "x" << resizedMask.rows
			  << "\n";
		success = false;
	}

	settings = baseSettings();
	settings.enableThreshold = false;
	cv::Mat noThresholdMask =
		background_removal::postProcessForegroundMask(foreground, foreground.size(), settings);
	success &= expectPixel(noThresholdMask, 0, 0, 255, "no-threshold background");
	success &= expectPixel(noThresholdMask, 0, 2, 127, "no-threshold alpha inversion");
	success &= expectPixel(noThresholdMask, 0, 4, 0, "no-threshold foreground");

	cv::Mat foregroundWithSpeck = cv::Mat(5, 5, CV_8UC1, cv::Scalar(255));
	foregroundWithSpeck.row(0).setTo(0);
	foregroundWithSpeck.at<uint8_t>(2, 2) = 0;
	settings = baseSettings();
	settings.contourFilter = 0.1f;
	cv::Mat filteredSpeckMask = background_removal::postProcessForegroundMask(foregroundWithSpeck,
										  foregroundWithSpeck.size(), settings);
	success &= expectPixel(filteredSpeckMask, 0, 0, 255, "large background component");
	success &= expectPixel(filteredSpeckMask, 2, 2, 0, "small background component");

	return success ? 0 : 1;
}
