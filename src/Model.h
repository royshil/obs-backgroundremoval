#ifndef MODEL_H
#define MODEL_H

#if defined(__APPLE__)
#include <onnxruntime/core/session/onnxruntime_cxx_api.h>
#include <onnxruntime/core/providers/cpu/cpu_provider_factory.h>
#else
#include <onnxruntime_cxx_api.h>
#include <cpu_provider_factory.h>
#endif

#ifdef WITH_CUDA
#include <tensorrt_provider_factory.h>
#endif
#ifdef _WIN32
#ifndef WITH_CUDA
#include <dml_provider_factory.h>
#endif
#include <wchar.h>
#endif

template <typename T>
T vectorProduct(const std::vector<T>& v)
{
  return accumulate(v.begin(), v.end(), (T)1, std::multiplies<T>());
}


static void hwc_to_chw(cv::InputArray src, cv::OutputArray dst) {
  std::vector<cv::Mat> channels;
  cv::split(src, channels);

  // Stretch one-channel images to vector
  for (auto &img : channels) {
    img = img.reshape(1, 1);
  }

  // Concatenate three vectors to one
  cv::hconcat( channels, dst );
}


class Model
{
private:
  /* data */
public:
  Model(/* args */) {};
  ~Model() {};

  const char* name;

#if _WIN32
  const std::wstring
#else
  const std::string
#endif
  getModelFilepath(const std::string& modelSelection) {
    char* modelFilepath_rawPtr = obs_module_file(modelSelection.c_str());

    if (modelFilepath_rawPtr == nullptr) {
      blog(LOG_ERROR, "Unable to get model filename %s from plugin.", modelSelection.c_str());
      return nullptr;
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


  virtual void populateInputOutputNames(
    const std::unique_ptr<Ort::Session>& session,
    std::vector<const char*>& inputNames,
	  std::vector<const char*>& outputNames
  ) {
    Ort::AllocatorWithDefaultOptions allocator;

    inputNames.clear();
    outputNames.clear();
	  inputNames.push_back(session->GetInputName(0, allocator));
  	outputNames.push_back(session->GetOutputName(0, allocator));
  }


  virtual bool populateInputOutputShapes(
    const std::unique_ptr<Ort::Session>& session,
    std::vector<std::vector<int64_t> >& inputDims,
    std::vector<std::vector<int64_t> >& outputDims
  ) {
    // Assuming model only has one input and one output image

    inputDims.clear();
    outputDims.clear();

    inputDims.push_back(std::vector<int64_t>());
    outputDims.push_back(std::vector<int64_t>());

    // Get output shape
    const Ort::TypeInfo outputTypeInfo = session->GetOutputTypeInfo(0);
    const auto outputTensorInfo = outputTypeInfo.GetTensorTypeAndShapeInfo();
    outputDims[0] = outputTensorInfo.GetShape();

    // Get input shape
    const Ort::TypeInfo inputTypeInfo = session->GetInputTypeInfo(0);
    const auto inputTensorInfo = inputTypeInfo.GetTensorTypeAndShapeInfo();
    inputDims[0] = inputTensorInfo.GetShape();

    if (inputDims[0].size() < 3 || outputDims[0].size() < 3) {
      blog(LOG_ERROR, "Input or output tensor dims are < 3. input = %d, output = %d",
        (int)inputDims.size(), (int)outputDims.size());
      return false;
    }

    return true;
  }

  virtual void allocateTensorBuffers(
    const std::vector<std::vector<int64_t> >& inputDims,
    const std::vector<std::vector<int64_t> >& outputDims,
    std::vector<std::vector<float> >& outputTensorValues,
  	std::vector<std::vector<float> >& inputTensorValues,
    std::vector<Ort::Value>& inputTensor,
    std::vector<Ort::Value>& outputTensor
  ) {
    // Assuming model only has one input and one output images

    outputTensorValues.clear();
    outputTensor.clear();

    inputTensorValues.clear();
    inputTensor.clear();

    Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
      OrtAllocatorType::OrtDeviceAllocator, OrtMemType::OrtMemTypeDefault);

    // Allocate buffers and build input and output tensors

    for (size_t i = 0; i < inputDims.size(); i++) {
      inputTensorValues.push_back(std::vector<float>(vectorProduct(inputDims[i]), 0.0f));
      blog(LOG_INFO, "Allocated %d sized float-array for input %d", (int)inputTensorValues[i].size(), (int)i);
      inputTensor.push_back(Ort::Value::CreateTensor<float>(
        memoryInfo,
        inputTensorValues[i].data(),
        inputTensorValues[i].size(),
        inputDims[i].data(),
        inputDims[i].size()));
    }

    for (size_t i = 0; i < outputDims.size(); i++) {
      outputTensorValues.push_back(std::vector<float>(vectorProduct(outputDims[i]), 0.0f));
      blog(LOG_INFO, "Allocated %d sized float-array for output %d", (int)outputTensorValues[i].size(), (int)i);
      outputTensor.push_back(Ort::Value::CreateTensor<float>(
        memoryInfo,
        outputTensorValues[i].data(),
        outputTensorValues[i].size(),
        outputDims[i].data(),
        outputDims[i].size()));
    }
  }

  virtual void getNetworkInputSize(
    const std::vector<std::vector<int64_t> >& inputDims,
    uint32_t& inputWidth, uint32_t& inputHeight
  ) {
    // BHWC
    inputWidth  = (int)inputDims[0][2];
    inputHeight = (int)inputDims[0][1];
  }

  virtual void prepareInputToNetwork(cv::Mat& resizedImage, cv::Mat& preprocessedImage) {
    preprocessedImage = resizedImage / 255.0;
  }

  virtual void loadInputToTensor(
    const cv::Mat& preprocessedImage,
    uint32_t inputWidth,
    uint32_t inputHeight,
    std::vector<std::vector<float> >& inputTensorValues
  ) {
    preprocessedImage.copyTo(cv::Mat(inputHeight, inputWidth, CV_32FC3, &(inputTensorValues[0][0])));
  }

  virtual cv::Mat getNetworkOutput(
    const std::vector<std::vector<int64_t> >& outputDims,
    std::vector<std::vector<float> >& outputTensorValues,
    std::vector<std::vector<int64_t> >& inputDims,
    std::vector<std::vector<float> >& inputTensorValues
  ) {
    // BHWC
    uint32_t outputWidth = (int)outputDims[0].at(2);
    uint32_t outputHeight = (int)outputDims[0].at(1);
    int32_t outputChannels = CV_32FC1;

    return cv::Mat(outputHeight, outputWidth, outputChannels, outputTensorValues[0].data());
  }

  virtual void postprocessOutput(cv::Mat& outputImage) {}

  virtual void runNetworkInference(
    const std::unique_ptr<Ort::Session>& session,
    const std::vector<const char*>& inputNames,
	  const std::vector<const char*>& outputNames,
    const std::vector<Ort::Value>& inputTensor,
    std::vector<Ort::Value>& outputTensor
  ) {
    if (inputNames.size() == 0 || outputNames.size() == 0 || inputTensor.size() == 0 || outputTensor.size() == 0) {
      blog(LOG_INFO, "Skip network inference. Inputs or outputs are null.");
      return;
    }

    session->Run(
      Ort::RunOptions{nullptr},
      // inputNames.data(), &(inputTensor[0]), 1,
      // outputNames.data(), &(outputTensor[0]), 1
      inputNames.data(), inputTensor.data(), inputNames.size(),
      outputNames.data(), outputTensor.data(), outputNames.size()
    );
  }
};


class ModelSelfie : public Model
{
private:
  /* data */
public:
  ModelSelfie(/* args */) {}
  ~ModelSelfie() {}

  virtual void postprocessOutput(cv::Mat& outputImage) {
    cv::normalize(outputImage, outputImage, 1.0, 0.0, cv::NORM_MINMAX);
  }
};


class ModelMediaPipe : public Model
{
private:
  /* data */
public:
  ModelMediaPipe(/* args */) {}
  ~ModelMediaPipe() {}

  virtual cv::Mat getNetworkOutput(
    const std::vector<std::vector<int64_t> >& outputDims,
    std::vector<std::vector<float> >& outputTensorValues,
    std::vector<std::vector<int64_t> >& inputDims,
    std::vector<std::vector<float> >& inputTensorValues
  ) {
    uint32_t outputWidth = (int)outputDims[0].at(2);
    uint32_t outputHeight = (int)outputDims[0].at(1);
    int32_t outputChannels = CV_32FC2;

    return cv::Mat(outputHeight, outputWidth, outputChannels, outputTensorValues[0].data());
  }

  virtual void postprocessOutput(cv::Mat& outputImage) {
    // take 1st channel
    std::vector<cv::Mat> outputImageSplit;
    cv::split(outputImage, outputImageSplit);

    // "Softmax"
    cv::Mat outputA, outputB;
    cv::exp(outputImageSplit[0], outputA);
    cv::exp(outputImageSplit[1], outputB);
    outputImage = outputA / (outputA + outputB);

    cv::normalize(outputImage, outputImage, 1.0, 0.0, cv::NORM_MINMAX);
  }
};


class ModelBCHW : public Model
{
public:
  ModelBCHW(/* args */) {}
  ~ModelBCHW() {}

  virtual void getNetworkInputSize(
    const std::vector<std::vector<int64_t> >& inputDims,
    uint32_t& inputWidth, uint32_t& inputHeight
  ) {
    // BCHW
    inputWidth  = (int)inputDims[0][3];
    inputHeight = (int)inputDims[0][2];
  }

  virtual cv::Mat getNetworkOutput(
    const std::vector<std::vector<int64_t> >& outputDims,
    std::vector<std::vector<float> >& outputTensorValues,
    std::vector<std::vector<int64_t> >& inputDims,
    std::vector<std::vector<float> >& inputTensorValues
  ) {
    // BCHW
    uint32_t outputWidth = (int)outputDims[0].at(3);
    uint32_t outputHeight = (int)outputDims[0].at(2);
    int32_t outputChannels = CV_32FC1;

    return cv::Mat(outputHeight, outputWidth, outputChannels, outputTensorValues[0].data());
  }

  virtual void loadInputToTensor(
    const cv::Mat& preprocessedImage,
    uint32_t inputWidth,
    uint32_t inputHeight,
    std::vector<std::vector<float> >& inputTensorValues
  ) {
    inputTensorValues[0].assign(
      preprocessedImage.begin<float>(),
      preprocessedImage.end<float>()
    );
  }
};


class ModelSINET : public ModelBCHW
{
public:
  ModelSINET(/* args */) {}
  ~ModelSINET() {}

  virtual void prepareInputToNetwork(cv::Mat& resizedImage, cv::Mat& preprocessedImage) {
    cv::subtract(resizedImage, cv::Scalar(102.890434, 111.25247, 126.91212), resizedImage);
    cv::multiply(resizedImage, cv::Scalar(1.0 / 62.93292,  1.0 / 62.82138, 1.0 / 66.355705) / 255.0, resizedImage);
    hwc_to_chw(resizedImage, preprocessedImage);
  }
};


class ModelMODNET : public ModelBCHW
{
public:
  ModelMODNET(/* args */) {}
  ~ModelMODNET() {}

  virtual void prepareInputToNetwork(cv::Mat& resizedImage, cv::Mat& preprocessedImage) {
    cv::subtract(resizedImage, cv::Scalar::all(127.5), resizedImage);
    resizedImage = resizedImage / 127.5;
    hwc_to_chw(resizedImage, preprocessedImage);
  }
};

class ModelRVM : public ModelBCHW
{
private:
  /* data */
public:
  ModelRVM(/* args */) {}
  ~ModelRVM() {}

  virtual void prepareInputToNetwork(cv::Mat& resizedImage, cv::Mat& preprocessedImage) {
    resizedImage = resizedImage / 256.0;
    hwc_to_chw(resizedImage, preprocessedImage);
  }

  virtual void populateInputOutputNames(
    const std::unique_ptr<Ort::Session>& session,
    std::vector<const char*>& inputNames,
	  std::vector<const char*>& outputNames
  ) {
    Ort::AllocatorWithDefaultOptions allocator;

    inputNames.clear();
    outputNames.clear();

    for (size_t i = 0; i < session->GetInputCount(); i++) {
  	  inputNames.push_back(session->GetInputName(i, allocator));
    }
    for (size_t i = 1; i < session->GetOutputCount(); i++) {
  	  outputNames.push_back(session->GetOutputName(i, allocator));
    }
  }

  virtual bool populateInputOutputShapes(
    const std::unique_ptr<Ort::Session>& session,
    std::vector<std::vector<int64_t> >& inputDims,
    std::vector<std::vector<int64_t> >& outputDims
  ) {
    // Assuming model only has one input and one output image

    inputDims.clear();
    outputDims.clear();

    for (size_t i = 0; i < session->GetInputCount(); i++) {
      // Get input shape
      const Ort::TypeInfo inputTypeInfo = session->GetInputTypeInfo(i);
      const auto inputTensorInfo = inputTypeInfo.GetTensorTypeAndShapeInfo();
      inputDims.push_back(inputTensorInfo.GetShape());
    }

    for (size_t i = 1; i < session->GetOutputCount(); i++) {
      // Get output shape
      const Ort::TypeInfo outputTypeInfo = session->GetOutputTypeInfo(i);
      const auto outputTensorInfo = outputTypeInfo.GetTensorTypeAndShapeInfo();
      outputDims.push_back(outputTensorInfo.GetShape());
    }

    inputDims[0][0] = 1;
    inputDims[0][2] = 192;
    inputDims[0][3] = 192;
    for (size_t i = 1; i < 5; i++) {
      inputDims[i][0] = 1;
      inputDims[i][1] = (i == 1) ? 16 : (i == 2) ? 20 : (i == 3) ? 40 : 64;
      inputDims[i][2] = 192 / std::pow(2, i);
      inputDims[i][3] = 192 / std::pow(2, i);
    }

    outputDims[0][0] = 1;
    outputDims[0][2] = 192;
    outputDims[0][3] = 192;
    for (size_t i = 1; i < 5; i++) {
      outputDims[i][0] = 1;
      outputDims[i][2] = 192 / std::pow(2, i);
      outputDims[i][3] = 192 / std::pow(2, i);
    }
    return true;
	}

  virtual void loadInputToTensor(
    const cv::Mat& preprocessedImage,
    uint32_t inputWidth,
    uint32_t inputHeight,
    std::vector<std::vector<float> >& inputTensorValues
  ) {
    inputTensorValues[0].assign(
      preprocessedImage.begin<float>(),
      preprocessedImage.end<float>()
    );
    inputTensorValues[5][0] = 1.0f;
  }

  virtual cv::Mat getNetworkOutput(
    const std::vector<std::vector<int64_t> >& outputDims,
    std::vector<std::vector<float> >& outputTensorValues,
    std::vector<std::vector<int64_t> >& inputDims,
    std::vector<std::vector<float> >& inputTensorValues
  ) {
    // BCHW
    uint32_t outputWidth = (int)outputDims[0].at(3);
    uint32_t outputHeight = (int)outputDims[0].at(2);
    int32_t outputChannels = CV_32FC1;

    for (size_t i = 1; i < 5; i++) {
      inputTensorValues[i].assign(outputTensorValues[i].begin(), outputTensorValues[i].end());
    }

    return cv::Mat(outputHeight, outputWidth, outputChannels, outputTensorValues[0].data());
  }
};

#endif
