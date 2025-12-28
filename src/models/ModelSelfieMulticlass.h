#ifndef MODELSELFIE_MULTICLASS_H
#define MODELSELFIE_MULTICLASS_H

#include "Model.h"

/**
 * @brief MediaPipe Selfie Multiclass Segmentation Model
 * 
 * This model provides improved segmentation quality compared to the standard 
 * selfie segmentation model, especially for:
 * - Multiple people in the frame
 * - Full-body segmentation
 * - Fine-grained class segmentation (hair, clothes, body, face)
 * 
 * Model Details:
 * - Input: [1, 256, 256, 3] - 256x256 RGB image, float32, range [0, 1]
 * - Output: [1, 256, 256, 6] - 256x256 with 6 classes per pixel:
 *   - Class 0: Background
 *   - Class 1: Hair
 *   - Class 2: Body-skin
 *   - Class 3: Face-skin
 *   - Class 4: Clothes
 *   - Class 5: Others (accessories)
 * 
 * For background removal, we use argmax across the 6 channels to find the 
 * dominant class per pixel, then create a binary mask where:
 * - Class 0 (background) = 0 (remove)
 * - Classes 1-5 (person-related) = 1 (keep)
 * 
 * Model Card: https://storage.googleapis.com/mediapipe-assets/Model%20Card%20Multiclass%20Segmentation.pdf
 * Source: https://storage.googleapis.com/mediapipe-models/image_segmenter/selfie_multiclass_256x256/float32/latest/selfie_multiclass_256x256.tflite
 * 
 * Conversion from TFLite to ONNX required - see scripts/convert_selfie_multiclass.py
 */
class ModelSelfieMulticlass : public Model {
private:
	/* data */
public:
	ModelSelfieMulticlass(/* args */) {}
	~ModelSelfieMulticlass() {}

	/**
	 * @brief Get the network output and reshape it appropriately
	 * 
	 * The output is [1, 256, 256, 6] with 6 channels representing class probabilities.
	 * We need to convert this to a single-channel mask for background removal.
	 */
	virtual cv::Mat getNetworkOutput(const std::vector<std::vector<int64_t>> &outputDims,
					 std::vector<std::vector<float>> &outputTensorValues)
	{
		// Output is BHWC format: [1, 256, 256, 6]
		uint32_t outputWidth = (int)outputDims[0].at(2);
		uint32_t outputHeight = (int)outputDims[0].at(1);
		int32_t numClasses = (int)outputDims[0].at(3); // Should be 6

		// Create a multi-channel Mat with all class probabilities
		return cv::Mat(outputHeight, outputWidth, CV_MAKE_TYPE(CV_32F, numClasses),
			       outputTensorValues[0].data());
	}

	/**
	 * @brief Post-process the multi-class output to create a binary foreground mask
	 * 
	 * This function:
	 * 1. Finds the class with maximum probability for each pixel (argmax)
	 * 2. Creates a binary mask where class > 0 (any non-background) = foreground
	 * 3. Outputs a single-channel float mask with values in [0, 1]
	 */
	virtual void postprocessOutput(cv::Mat &outputImage)
	{
		// outputImage is [H, W, 6] with float32 class probabilities

		const int height = outputImage.rows;
		const int width = outputImage.cols;
		const int numClasses = outputImage.channels();

		// Split multi-channel output into separate channels
		std::vector<cv::Mat> channels(numClasses);
		cv::split(outputImage, channels);

		// Create matrices to store argmax results
		cv::Mat maxValues = cv::Mat::zeros(height, width, CV_32FC1);
		cv::Mat maxIndices = cv::Mat::zeros(height, width, CV_8UC1);

		// Find argmax across all channels
		for (int c = 0; c < numClasses; c++) {
			// Create a mask where this channel has the maximum value
			cv::Mat mask = channels[c] > maxValues;

			// Update maxValues where this channel is larger
			maxValues.copyTo(channels[c], ~mask);
			channels[c].copyTo(maxValues, mask);

			// Update maxIndices where this channel is larger
			maxIndices.setTo(c, mask);
		}

		// Create output mask: foreground = 1 where maxIndices > 0 (not background)
		cv::Mat foregroundMask = maxIndices > 0;

		// Convert boolean mask to float and multiply by confidence values
		cv::Mat mask;
		foregroundMask.convertTo(mask, CV_32FC1, 1.0 / 255.0); // Convert uint8 to float [0,1]
		mask = mask.mul(maxValues);                            // Multiply by confidence

		// Replace the multi-channel output with single-channel mask
		mask.copyTo(outputImage);
	}
};

#endif // MODELSELFIE_MULTICLASS_H
