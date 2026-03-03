// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELSELFIE_H
#define MODELSELFIE_H

#include "Model.hpp"

class ModelSelfie : public Model {
private:
	/* data */
public:
	ModelSelfie(/* args */) {}
	~ModelSelfie() {}

	virtual void postprocessOutput(cv::Mat &outputImage)
	{
		cv::normalize(outputImage, outputImage, 1.0, 0.0, cv::NORM_MINMAX);
	}
};

#endif // MODELSELFIE_H
