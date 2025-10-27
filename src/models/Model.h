#ifndef MODEL_H
#define MODEL_H

#include <onnxruntime_cxx_api.h>
#include "plugin-support.h"

#ifdef _WIN32
#include <wchar.h>
#endif

#include <opencv2/imgproc.hpp>
#include <algorithm>

template<typename T> T vectorProduct(const std::vector<T> &v)
{
	T product = 1;
	for (auto &i : v) {
		// turn 0 or -1, which are usually used as "None" (meaning any size), to 1s
		if (i > 0) {
			product *= i;
		}
	}
	return product;
}

static void hwc_to_chw(cv::InputArray src, cv::OutputArray dst)
{
	std::vector<cv::Mat> channels;
	cv::split(src, channels);

	// Stretch one-channel images to vector
	for (auto &img : channels) {
		img = img.reshape(1, 1);
	}

	// Concatenate three vectors to one
	cv::hconcat(channels, dst);
}

/**
* Convert a CHW Mat to HWC
* Assume the input Mat is a 3D tensor of shape (C, H, W), but the Mat header has
* the correct shape (H, W, C). This function will swap the channels to make it
* (H, W, C) on the data level.
* @param src Input Mat, assume data is in CHW format, type is float32
* @param dst Output Mat, data is in HWC format, type is float32
*/
static void chw_to_hwc_32f(cv::InputArray src, cv::OutputArray dst)
{
	const cv::Mat srcMat = src.getMat();
	const int channels = srcMat.channels();
	const int height = srcMat.rows;
	const int width = srcMat.cols;
	const int dtype = srcMat.type();
	assert(dtype == CV_32F);
	const int channelStride = height * width;

	// Flatten to a vector of channels
	cv::Mat flatMat = srcMat.reshape(1, 1);

	std::vector<cv::Mat> channelsVec(channels);
	// Split the vector into channels
	for (int i = 0; i < channels; i++) {
		channelsVec[i] =
			cv::Mat(height, width, CV_MAKE_TYPE(dtype, 1), flatMat.ptr<float>(0) + i * channelStride);
	}

	cv::merge(channelsVec, dst);
}

/**
  * @brief Base class for all models
  *
  * Assume that all models have one input and one output.
  * The input is a 4D tensor of shape (1, H, W, C) where H and W are the height and width of
  * the input image.
  * The range of the input is [0, 255]. The input is in BGR format.
  * The input is a 32-bit floating point tensor.
  * This base model will convert the input to [0,1] and then the output to [0,255].
  *
  * Inheriting classes may override the methods for loading the model and running inference
  * with different pre-post processing behavior (like BCHW instead of BHWC or different ranges).
*/
class Model {
private:
	/* data */
public:
	Model(/* args */) {};
	virtual ~Model() {};

	const char *name;

#if _WIN32
	const std::wstring
#else
	const std::string
#endif
	getModelFilepath(const std::string &modelSelection)
	{
		char *modelFilepath_rawPtr = obs_module_file(modelSelection.c_str());

		if (modelFilepath_rawPtr == nullptr) {
			obs_log(LOG_ERROR, "Unable to get model filename %s from plugin.", modelSelection.c_str());
#if _WIN32
			return std::wstring();
#else
			return std::string();
#endif
		}

		std::string modelFilepath_s(modelFilepath_rawPtr);
		bfree(modelFilepath_rawPtr);

#if _WIN32
		std::wstring modelFilepath_ws(modelFilepath_s.size(), L' ');
		std::copy(modelFilepath_s.begin(), modelFilepath_s.end(), modelFilepath_ws.begin());
		return modelFilepath_ws;
#else
		return modelFilepath_s;
#endif
	}

	virtual void populateInputOutputNames(const std::unique_ptr<Ort::Session> &session,
					      std::vector<Ort::AllocatedStringPtr> &inputNames,
					      std::vector<Ort::AllocatedStringPtr> &outputNames)
	{
		Ort::AllocatorWithDefaultOptions allocator;

		inputNames.clear();
		outputNames.clear();
		inputNames.push_back(session->GetInputNameAllocated(0, allocator));
		outputNames.push_back(session->GetOutputNameAllocated(0, allocator));
	}

	virtual bool populateInputOutputShapes(const std::unique_ptr<Ort::Session> &session,
					       std::vector<std::vector<int64_t>> &inputDims,
					       std::vector<std::vector<int64_t>> &outputDims)
	{
		// Assuming model only has one input and one output image

		inputDims.clear();
		outputDims.clear();

		inputDims.push_back(std::vector<int64_t>());
		outputDims.push_back(std::vector<int64_t>());

		// Get output shape
		const Ort::TypeInfo outputTypeInfo = session->GetOutputTypeInfo(0);
		const auto outputTensorInfo = outputTypeInfo.GetTensorTypeAndShapeInfo();
		outputDims[0] = outputTensorInfo.GetShape();

		// fix any -1 values in outputDims to 1
		for (auto &i : outputDims[0]) {
			if (i == -1) {
				i = 1;
			}
		}

		// Get input shape
		const Ort::TypeInfo inputTypeInfo = session->GetInputTypeInfo(0);
		const auto inputTensorInfo = inputTypeInfo.GetTensorTypeAndShapeInfo();
		inputDims[0] = inputTensorInfo.GetShape();

		// fix any -1 values in inputDims to 1
		for (auto &i : inputDims[0]) {
			if (i == -1) {
				i = 1;
			}
		}

		if (inputDims[0].size() < 3 || outputDims[0].size() < 3) {
			obs_log(LOG_ERROR, "Input or output tensor dims are < 3. input = %d, output = %d",
				(int)inputDims.size(), (int)outputDims.size());
			return false;
		}

		return true;
	}

	virtual void allocateTensorBuffers(const std::vector<std::vector<int64_t>> &inputDims,
					   const std::vector<std::vector<int64_t>> &outputDims,
					   std::vector<std::vector<float>> &outputTensorValues,
					   std::vector<std::vector<float>> &inputTensorValues,
					   std::vector<Ort::Value> &inputTensor, std::vector<Ort::Value> &outputTensor)
	{
		// Assuming model only has one input and one output images

		outputTensorValues.clear();
		outputTensor.clear();

		inputTensorValues.clear();
		inputTensor.clear();

		Ort::MemoryInfo memoryInfo =
			Ort::MemoryInfo::CreateCpu(OrtAllocatorType::OrtDeviceAllocator, OrtMemType::OrtMemTypeDefault);

		// Allocate buffers and build input and output tensors

		for (size_t i = 0; i < inputDims.size(); i++) {
			inputTensorValues.push_back(std::vector<float>(vectorProduct(inputDims[i]), 0.0f));
			obs_log(LOG_INFO, "Allocated %d sized float-array for input %d",
				(int)inputTensorValues[i].size(), (int)i);
			inputTensor.push_back(Ort::Value::CreateTensor<float>(
				memoryInfo, inputTensorValues[i].data(), inputTensorValues[i].size(),
				inputDims[i].data(), inputDims[i].size()));
		}

		for (size_t i = 0; i < outputDims.size(); i++) {
			outputTensorValues.push_back(std::vector<float>(vectorProduct(outputDims[i]), 0.0f));
			obs_log(LOG_INFO, "Allocated %d sized float-array for output %d",
				(int)outputTensorValues[i].size(), (int)i);
			outputTensor.push_back(Ort::Value::CreateTensor<float>(
				memoryInfo, outputTensorValues[i].data(), outputTensorValues[i].size(),
				outputDims[i].data(), outputDims[i].size()));
		}
	}

	virtual void getNetworkInputSize(const std::vector<std::vector<int64_t>> &inputDims, uint32_t &inputWidth,
					 uint32_t &inputHeight)
	{
		// BHWC
		inputWidth = (int)inputDims[0][2];
		inputHeight = (int)inputDims[0][1];
	}

	virtual void prepareInputToNetwork(cv::Mat &resizedImage, cv::Mat &preprocessedImage)
	{
		preprocessedImage = resizedImage / 255.0;
	}

	/**
    * @brief Postprocess the output of the network
    *
    * @param output The output of the network. This function should ensure the output is with
    * values in the range 0-1 (float 32), and in the BHWC format
  */
	virtual void postprocessOutput(cv::Mat &output) { UNUSED_PARAMETER(output); }

	virtual void loadInputToTensor(const cv::Mat &preprocessedImage, uint32_t inputWidth, uint32_t inputHeight,
				       std::vector<std::vector<float>> &inputTensorValues)
	{
		preprocessedImage.copyTo(cv::Mat(inputHeight, inputWidth, CV_32FC3, &(inputTensorValues[0][0])));
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

	virtual void assignOutputToInput(std::vector<std::vector<float>> &, std::vector<std::vector<float>> &) {}

	virtual void runNetworkInference(const std::unique_ptr<Ort::Session> &session,
					 const std::vector<Ort::AllocatedStringPtr> &inputNames,
					 const std::vector<Ort::AllocatedStringPtr> &outputNames,
					 const std::vector<Ort::Value> &inputTensor,
					 std::vector<Ort::Value> &outputTensor)
	{
		if (inputNames.size() == 0 || outputNames.size() == 0 || inputTensor.size() == 0 ||
		    outputTensor.size() == 0) {
			obs_log(LOG_INFO, "Skip network inference. Inputs or outputs are null.");
			return;
		}

		std::vector<const char *> rawInputNames;
		for (auto &inputName : inputNames) {
			rawInputNames.push_back(inputName.get());
		}

		std::vector<const char *> rawOutputNames;
		for (auto &outputName : outputNames) {
			rawOutputNames.push_back(outputName.get());
		}

		session->Run(Ort::RunOptions{nullptr}, rawInputNames.data(), inputTensor.data(), inputNames.size(),
			     rawOutputNames.data(), outputTensor.data(), outputNames.size());
	}
};

class ModelBCHW : public Model {
public:
	ModelBCHW(/* args */) {}
	~ModelBCHW() {}

	virtual void prepareInputToNetwork(cv::Mat &resizedImage, cv::Mat &preprocessedImage)
	{
		resizedImage = resizedImage / 255.0;
		hwc_to_chw(resizedImage, preprocessedImage);
	}

	virtual void postprocessOutput(cv::Mat &output)
	{
		cv::Mat outputTransposed;
		chw_to_hwc_32f(output, outputTransposed);
		outputTransposed.copyTo(output);
	}

	virtual void getNetworkInputSize(const std::vector<std::vector<int64_t>> &inputDims, uint32_t &inputWidth,
					 uint32_t &inputHeight)
	{
		// BCHW
		inputWidth = (int)inputDims[0][3];
		inputHeight = (int)inputDims[0][2];
	}

	virtual cv::Mat getNetworkOutput(const std::vector<std::vector<int64_t>> &outputDims,
					 std::vector<std::vector<float>> &outputTensorValues)
	{
		// BCHW
		uint32_t outputWidth = (int)outputDims[0].at(3);
		uint32_t outputHeight = (int)outputDims[0].at(2);
		int32_t outputChannels = CV_MAKE_TYPE(CV_32F, (int)outputDims[0].at(1));

		return cv::Mat(outputHeight, outputWidth, outputChannels, outputTensorValues[0].data());
	}

	virtual void loadInputToTensor(const cv::Mat &preprocessedImage, uint32_t, uint32_t,
				       std::vector<std::vector<float>> &inputTensorValues)
	{
		inputTensorValues[0].assign(preprocessedImage.begin<float>(), preprocessedImage.end<float>());
	}
};

#endif
