#ifndef FILTERDATA_H
#define FILTERDATA_H

#include <obs-module.h>

#include <atomic>
#include <memory>

#include "models/Model.h"
#include "ort-utils/ORTModelData.h"

/**
  * @brief The filter_data struct
  *
  * This struct is used to store the base data needed for ORT filters.
  *
*/
struct filter_data : public ORTModelData, public std::enable_shared_from_this<filter_data> {
	std::string useGPU;
	uint32_t numThreads;
	std::string modelSelection;
	std::unique_ptr<Model> model;

	obs_source_t *source;
	gs_texrender_t *texrender;
	gs_stagesurf_t *stagesurface;

	cv::Mat inputBGRA;

	std::atomic<bool> isDisabled{false};

	std::mutex inputBGRALock;
	std::mutex outputLock;

#if _WIN32
	std::wstring modelFilepath;
#else
	std::string modelFilepath;
#endif
};

#endif /* FILTERDATA_H */
