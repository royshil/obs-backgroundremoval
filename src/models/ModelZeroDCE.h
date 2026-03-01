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

#ifndef MODELZERODCE_H
#define MODELZERODCE_H

#include "Model.h"

class ModelZeroDCE : public ModelBCHW {
public:
	virtual void postprocessOutput(cv::Mat &output)
	{
		UNUSED_PARAMETER(output);
		// output is already BHWC and 0-255... nothing to do
	}

	virtual cv::Mat getNetworkOutput(const std::vector<std::vector<int64_t>> &outputDims,
					 std::vector<std::vector<float>> &outputTensorValues)
	{
		// BHWC
		uint32_t outputWidth = (int)outputDims[0].at(1);
		uint32_t outputHeight = (int)outputDims[0].at(0);
		int32_t outputChannels = CV_MAKE_TYPE(CV_32F, (int)outputDims[0].at(2));

		return cv::Mat(outputHeight, outputWidth, outputChannels, outputTensorValues[0].data());
	}
};

#endif /* MODELZERODCE_H */
