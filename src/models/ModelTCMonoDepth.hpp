// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELTCMONODEPTH_H
#define MODELTCMONODEPTH_H

#include "Model.hpp"

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
