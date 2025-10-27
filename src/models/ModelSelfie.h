#ifndef MODELSELFIE_H
#define MODELSELFIE_H

#include "Model.h"

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
