#ifndef MODELRVM_H
#define MODELRVM_H

#include "Model.h"

class ModelRVM : public ModelBCHW {
private:
	/* data */
public:
	ModelRVM(/* args */) {}
	~ModelRVM() {}

	virtual void populateInputOutputNames(const std::unique_ptr<Ort::Session> &session,
					      std::vector<Ort::AllocatedStringPtr> &inputNames,
					      std::vector<Ort::AllocatedStringPtr> &outputNames)
	{
		Ort::AllocatorWithDefaultOptions allocator;

		inputNames.clear();
		outputNames.clear();

		for (size_t i = 0; i < session->GetInputCount(); i++) {
			inputNames.push_back(session->GetInputNameAllocated(i, allocator));
		}
		for (size_t i = 1; i < session->GetOutputCount(); i++) {
			outputNames.push_back(session->GetOutputNameAllocated(i, allocator));
		}
	}

	virtual bool populateInputOutputShapes(const std::unique_ptr<Ort::Session> &session,
					       std::vector<std::vector<int64_t>> &inputDims,
					       std::vector<std::vector<int64_t>> &outputDims)
	{
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

		const int base_width = 320;
		const int base_height = 192;

		inputDims[0][0] = 1;
		inputDims[0][2] = base_height;
		inputDims[0][3] = base_width;
		for (size_t i = 1; i < 5; i++) {
			inputDims[i][0] = 1;
			inputDims[i][1] = (i == 1) ? 16 : (i == 2) ? 20 : (i == 3) ? 40 : 64;
			inputDims[i][2] = base_height / (2 << (i - 1));
			inputDims[i][3] = base_width / (2 << (i - 1));
		}

		outputDims[0][0] = 1;
		outputDims[0][2] = base_height;
		outputDims[0][3] = base_width;
		for (size_t i = 1; i < 5; i++) {
			outputDims[i][0] = 1;
			outputDims[i][2] = base_height / (2 << (i - 1));
			outputDims[i][3] = base_width / (2 << (i - 1));
		}
		return true;
	}

	virtual void loadInputToTensor(const cv::Mat &preprocessedImage, uint32_t, uint32_t,
				       std::vector<std::vector<float>> &inputTensorValues)
	{
		inputTensorValues[0].assign(preprocessedImage.begin<float>(), preprocessedImage.end<float>());
		inputTensorValues[5][0] = 1.0f;
	}

	virtual void assignOutputToInput(std::vector<std::vector<float>> &outputTensorValues,
					 std::vector<std::vector<float>> &inputTensorValues)
	{
		for (size_t i = 1; i < 5; i++) {
			inputTensorValues[i].assign(outputTensorValues[i].begin(), outputTensorValues[i].end());
		}
	}
};

#endif /* MODELRVM_H */
