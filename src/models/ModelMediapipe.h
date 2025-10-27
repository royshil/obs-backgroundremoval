#ifndef MODELMEDIAPIPE_H
#define MODELMEDIAPIPE_H

#include "Model.h"

class ModelMediaPipe : public Model {
private:
	/* data */
public:
	ModelMediaPipe(/* args */) {}
	~ModelMediaPipe() {}

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
		// take 2nd channel
		std::vector<cv::Mat> outputImageSplit;
		cv::split(outputImage, outputImageSplit);

		outputImage = outputImageSplit[1];
	}
};

#endif // MODELMEDIAPIPE_H
