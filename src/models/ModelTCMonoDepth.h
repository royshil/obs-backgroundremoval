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

#ifndef MODELTCMONODEPTH_H
#define MODELTCMONODEPTH_H

#include "Model.h"

class ModelTCMonoDepth : public ModelBCHW {
private:
	/* data */
public:
	ModelTCMonoDepth(/* args */) {}
	~ModelTCMonoDepth() {}

	virtual void prepareInputToNetwork(cv::Mat &resizedImage, cv::Mat &preprocessedImage)
	{
		// Do not normalize from [0, 255] to [0, 1].

		hwc_to_chw(resizedImage, preprocessedImage);
	}

	virtual void postprocessOutput(cv::Mat &outputImage)
	{
		cv::normalize(outputImage, outputImage, 1.0, 0.0, cv::NORM_MINMAX);
	}
};

#endif // MODELTCMONODEPTH_H
