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
