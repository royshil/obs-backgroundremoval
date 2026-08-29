// SPDX-FileCopyrightText: 2021-2026 Roy Shilkrot <roy.shil@gmail.com>
// SPDX-FileCopyrightText: 2023-2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef MODELRVMRESNET50_H
#define MODELRVMRESNET50_H

#include "Model.hpp"

/**
 * Robust Video Matting (RVM) ResNet50 background matting model (float16 ONNX).
 *
 * The ONNX graph has six inputs and six outputs:
 *   - src:              [1, 3, H, W]  (BCHW), float16, RGB in [0, 1]
 *   - r1i .. r4i:      recurrent states, float16
 *   - downsample_ratio: scalar, float32
 *   - fgr:             [1, 3, H, W]  (foreground, unused), float16
 *   - pha:             [1, 1, H, W]  (alpha matte, float16)  <- used as the mask
 *   - r1o .. r4o:      new recurrent states, float16
 *
 * RVM is a recurrent/stateful model: the states produced for one frame must be
 * fed back as inputs for the next frame. We pin the input resolution to 288x512
 * (landscape) and downsample_ratio to 0.5, which fixes the recurrent state
 * spatial sizes (72x128, 36x64, 18x32, 9x16) and the alpha output to 288x512.
 */
class ModelRVMResNet50 : public ModelBCHW {
private:
	static constexpr int IN_H = 288;
	static constexpr int IN_W = 512;
	static constexpr float DS_RATIO = 0.5f;

	// fp16 buffers for src (index 0) and recurrent states r1i..r4i (index 1..4)
	std::vector<std::vector<Ort::Float16_t>> inputFp16;
	// float32 buffer for the downsample_ratio scalar (input index 5)
	std::vector<float> dsRatio;
	// fp16 buffers for pha (index 0) and recurrent states r1o..r4o (index 1..4)
	std::vector<std::vector<Ort::Float16_t>> outputFp16;

public:
	ModelRVMResNet50(/* args */) {}
	~ModelRVMResNet50() {}

	virtual void populateInputOutputNames(const std::unique_ptr<Ort::Session> &session,
					     std::vector<Ort::AllocatedStringPtr> &inputNames,
					     std::vector<Ort::AllocatedStringPtr> &outputNames) override
	{
		Ort::AllocatorWithDefaultOptions allocator;

		inputNames.clear();
		outputNames.clear();

		for (size_t i = 0; i < session->GetInputCount(); i++) {
			inputNames.push_back(session->GetInputNameAllocated(i, allocator));
		}
		// Skip fgr (output index 0); we only need pha + the recurrent states.
		for (size_t i = 1; i < session->GetOutputCount(); i++) {
			outputNames.push_back(session->GetOutputNameAllocated(i, allocator));
		}
	}

	virtual bool populateInputOutputShapes(const std::unique_ptr<Ort::Session> &session,
					       std::vector<std::vector<int64_t>> &inputDims,
					       std::vector<std::vector<int64_t>> &outputDims) override
	{
		UNUSED_PARAMETER(session);

		inputDims.clear();
		outputDims.clear();

		// inputs: src, r1i, r2i, r3i, r4i, downsample_ratio
		inputDims.push_back({1, 3, IN_H, IN_W});
		inputDims.push_back({1, 16, 72, 128});
		inputDims.push_back({1, 32, 36, 64});
		inputDims.push_back({1, 64, 18, 32});
		inputDims.push_back({1, 128, 9, 16});
		inputDims.push_back({1});

		// outputs (fgr skipped): pha, r1o, r2o, r3o, r4o
		outputDims.push_back({1, 1, IN_H, IN_W});
		outputDims.push_back({1, 16, 72, 128});
		outputDims.push_back({1, 32, 36, 64});
		outputDims.push_back({1, 64, 18, 32});
		outputDims.push_back({1, 128, 9, 16});

		return true;
	}

	virtual void allocateTensorBuffers(const std::vector<std::vector<int64_t>> &inputDims,
					   const std::vector<std::vector<int64_t>> &outputDims,
					   std::vector<std::vector<float>> &outputTensorValues,
					   std::vector<std::vector<float>> &inputTensorValues,
					   std::vector<Ort::Value> &inputTensor,
					   std::vector<Ort::Value> &outputTensor) override
	{
		inputFp16.clear();
		outputFp16.clear();
		inputTensor.clear();
		outputTensor.clear();
		inputTensorValues.clear();
		outputTensorValues.clear();

		Ort::MemoryInfo memoryInfo = Ort::MemoryInfo::CreateCpu(
			OrtAllocatorType::OrtDeviceAllocator, OrtMemType::OrtMemTypeDefault);

		// Inputs 0..4 (src + recurrent states) are float16
		for (int i = 0; i < 5; i++) {
			inputFp16.push_back(
				std::vector<Ort::Float16_t>(vectorProduct(inputDims[i]), Ort::Float16_t(0.0f)));
			inputTensor.push_back(Ort::Value::CreateTensor<Ort::Float16_t>(
				memoryInfo, inputFp16.back().data(), inputFp16.back().size(),
				inputDims[i].data(), inputDims[i].size()));
		}
		// Input 5 (downsample_ratio) is float32 scalar
		dsRatio.assign(vectorProduct(inputDims[5]), 0.0f);
		inputTensor.push_back(Ort::Value::CreateTensor<float>(
			memoryInfo, dsRatio.data(), dsRatio.size(), inputDims[5].data(),
			inputDims[5].size()));

		// Outputs (pha + recurrent states) are float16
		for (size_t i = 0; i < outputDims.size(); i++) {
			outputFp16.push_back(
				std::vector<Ort::Float16_t>(vectorProduct(outputDims[i]), Ort::Float16_t(0.0f)));
			outputTensor.push_back(Ort::Value::CreateTensor<Ort::Float16_t>(
				memoryInfo, outputFp16.back().data(), outputFp16.back().size(),
				outputDims[i].data(), outputDims[i].size()));
		}
	}

	virtual void loadInputToTensor(const cv::Mat &preprocessedImage, uint32_t, uint32_t,
				       std::vector<std::vector<float>> &) override
	{
		// preprocessedImage is float32 CHW, flattened to (1, 3*H*W), in [0, 1]
		cv::Mat srcFp16;
		cv::convertFp16(preprocessedImage, srcFp16); // float32 -> float16
		memcpy(inputFp16[0].data(), srcFp16.data,
		       inputFp16[0].size() * sizeof(Ort::Float16_t));

		dsRatio[0] = DS_RATIO;
		// Recurrent states (inputFp16[1..4]) are left intact: they hold the
		// previous frame's states (copied back by assignOutputToInput).
	}

	virtual cv::Mat getNetworkOutput(const std::vector<std::vector<int64_t>> &outputDims,
					 std::vector<std::vector<float>> &) override
	{
		// pha is outputFp16[0], shape [1, 1, H, W], float16
		int64_t H = outputDims[0][2];
		int64_t W = outputDims[0][3];

		cv::Mat phaFp16(1, (int)(H * W), CV_16FC1, outputFp16[0].data());
		cv::Mat pha;
		phaFp16.convertTo(pha, CV_32F); // (1, H*W) float32
		pha = pha.reshape(1, (int)H);   // (H, W) CV_32FC1
		return pha;
	}

	virtual void assignOutputToInput(std::vector<std::vector<float>> &,
					 std::vector<std::vector<float>> &) override
	{
		// Carry the recurrent states forward to the next frame.
		for (int i = 1; i < 5; i++) {
			inputFp16[i].assign(outputFp16[i].begin(), outputFp16[i].end());
		}
	}
};

#endif // MODELRVMRESNET50_H
