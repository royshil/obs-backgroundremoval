/*
 * SPDX-FileCopyrightText: Copyright (C) 2021 Roy Shilkrot roy.shil@gmail.com
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * OBS Plugin: Portrait Background Removal / Virtual Green-screen and Low-Light Enhancement
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef MODELPPHUMANSEG_H
#define MODELPPHUMANSEG_H

#include "Model.h"

class ModelPPHumanSeg : public ModelBCHW {
public:
	ModelPPHumanSeg(/* args */) {}
	~ModelPPHumanSeg() {}

	virtual void prepareInputToNetwork(cv::Mat &resizedImage, cv::Mat &preprocessedImage)
	{
		resizedImage = (resizedImage / 256.0 - cv::Scalar(0.5, 0.5, 0.5)) / cv::Scalar(0.5, 0.5, 0.5);

		hwc_to_chw(resizedImage, preprocessedImage);
	}

	virtual cv::Mat getNetworkOutput(const std::vector<std::vector<int64_t>> &outputDims,
					 std::vector<std::vector<float>> &outputTensorValues)
	{
		uint32_t outputWidth = (int)outputDims[0].at(2);
		uint32_t outputHeight = (int)outputDims[0].at(1);
		int32_t outputChannels = CV_32FC2;

		return cv::Mat(outputHeight, outputWidth, outputChannels, outputTensorValues[0].data());
	}

	virtual void postprocessOutput(cv::Mat &outputImage)
	{
		// take 1st channel
		std::vector<cv::Mat> outputImageSplit;
		cv::split(outputImage, outputImageSplit);

		cv::normalize(outputImageSplit[1], outputImage, 1.0, 0.0, cv::NORM_MINMAX);
	}
};

#endif // MODELPPHUMANSEG_H
