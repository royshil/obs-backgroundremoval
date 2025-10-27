#ifndef MODELTBEFN_H
#define MODELTBEFN_H

#include "Model.h"

class ModelTBEFN : public ModelBCHW {
public:
	virtual void postprocessOutput(cv::Mat &output)
	{
		// output is already BHWC ...
		output = output * 255.0; // Convert to 0-255 range
	}

	virtual cv::Mat getNetworkOutput(const std::vector<std::vector<int64_t>> &outputDims,
					 std::vector<std::vector<float>> &outputTensorValues)
	{
		// BHWC
		uint32_t outputWidth = (int)outputDims[0].at(2);
		uint32_t outputHeight = (int)outputDims[0].at(1);
		int32_t outputChannels = CV_MAKE_TYPE(CV_32F, (int)outputDims[0].at(3));

		return cv::Mat(outputHeight, outputWidth, outputChannels, outputTensorValues[0].data());
	}
};

#endif /* MODELTBEFN_H */
