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

#ifndef MODELSINET_H
#define MODELSINET_H

#include "Model.h"

class ModelSINET : public ModelBCHW {
public:
	ModelSINET(/* args */) {}
	~ModelSINET() {}

	virtual void prepareInputToNetwork(cv::Mat &resizedImage, cv::Mat &preprocessedImage)
	{
		resizedImage = (resizedImage - cv::Scalar(102.890434, 111.25247, 126.91212)) /
			       cv::Scalar(62.93292 * 255.0, 62.82138 * 255.0, 66.355705 * 255.0);
		hwc_to_chw(resizedImage, preprocessedImage);
	}

	virtual cv::Mat getNetworkOutput(const std::vector<std::vector<int64_t>> &outputDims,
					 std::vector<std::vector<float>> &outputTensorValues)
	{
		UNUSED_PARAMETER(outputDims);
		return cv::Mat(320, 320, CV_32FC2, outputTensorValues[0].data());
	}

	virtual void postprocessOutput(cv::Mat &outputImage)
	{
		cv::Mat outputTransposed;
		chw_to_hwc_32f(outputImage, outputTransposed);
		// take 2nd channel
		std::vector<cv::Mat> outputImageSplit;
		cv::split(outputTransposed, outputImageSplit);
		outputImage = outputImageSplit[1];
	}
};

#endif // MODELSINET_H
