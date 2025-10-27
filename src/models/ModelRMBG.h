#ifndef MODELRMBG_H
#define MODELRMBG_H

#include "Model.h"

class ModelRMBG : public ModelBCHW {
public:
	ModelRMBG(/* args */) {}
	~ModelRMBG() {}

	bool populateInputOutputShapes(const std::unique_ptr<Ort::Session> &session,
				       std::vector<std::vector<int64_t>> &inputDims,
				       std::vector<std::vector<int64_t>> &outputDims)
	{
		ModelBCHW::populateInputOutputShapes(session, inputDims, outputDims);

		// fix the output width and height to the input width and height
		outputDims[0].at(2) = inputDims[0].at(2);
		outputDims[0].at(3) = inputDims[0].at(3);

		return true;
	}
};

#endif // MODELRMBG_H
