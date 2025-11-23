#include "enhance-filter.h"

#include <onnxruntime_cxx_api.h>

#ifdef _WIN32
#include <wchar.h>
#endif // _WIN32

#include <opencv2/imgproc.hpp>

#include <numeric>
#include <memory>
#include <exception>
#include <fstream>
#include <new>
#include <mutex>
#include <regex>

#include <plugin-support.h>
#include "consts.h"
#include "obs-utils/obs-utils.h"
#include "ort-utils/ort-session-utils.h"
#include "models/ModelTBEFN.h"
#include "models/ModelZeroDCE.h"
#include "models/ModelURetinex.h"
#include "update-checker/update-checker.h"

struct enhance_filter : public filter_data, public std::enable_shared_from_this<enhance_filter> {
	cv::Mat outputBGRA;
	gs_effect_t *blendEffect;
	float blendFactor;

	std::mutex modelMutex;

	~enhance_filter() { obs_log(LOG_INFO, "Enhance filter destructor called"); }
};

const char *enhance_filter_getname(void *unused)
{
	UNUSED_PARAMETER(unused);
	return obs_module_text("EnhancePortrait");
}

obs_properties_t *enhance_filter_properties(void *data)
{
	UNUSED_PARAMETER(data);
	obs_properties_t *props = obs_properties_create();
	obs_properties_add_float_slider(props, "blend", obs_module_text("EffectStrengh"), 0.0, 1.0, 0.05);
	obs_properties_add_int_slider(props, "numThreads", obs_module_text("NumThreads"), 0, 8, 1);
	obs_property_t *p_model_select = obs_properties_add_list(props, "model_select",
								 obs_module_text("EnhancementModel"),
								 OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p_model_select, obs_module_text("TBEFN"), MODEL_ENHANCE_TBEFN);
	obs_property_list_add_string(p_model_select, obs_module_text("URETINEX"), MODEL_ENHANCE_URETINEX);
	obs_property_list_add_string(p_model_select, obs_module_text("SGLLIE"), MODEL_ENHANCE_SGLLIE);
	obs_property_list_add_string(p_model_select, obs_module_text("ZERODCE"), MODEL_ENHANCE_ZERODCE);
	obs_property_t *p_use_gpu = obs_properties_add_list(props, "useGPU", obs_module_text("InferenceDevice"),
							    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_STRING);
	obs_property_list_add_string(p_use_gpu, obs_module_text("CPU"), USEGPU_CPU);
#ifdef HAVE_ONNXRUNTIME_CUDA_EP
	obs_property_list_add_string(p_use_gpu, obs_module_text("GPUCUDA"), USEGPU_CUDA);
#endif
#ifdef HAVE_ONNXRUNTIME_ROCM_EP
	obs_property_list_add_string(p_use_gpu, obs_module_text("GPUROCM"), USEGPU_ROCM);
#endif
#ifdef HAVE_ONNXRUNTIME_TENSORRT_EP
	obs_property_list_add_string(p_use_gpu, obs_module_text("TENSORRT"), USEGPU_TENSORRT);
#endif
#if defined(__APPLE__)
	obs_property_list_add_string(p_use_gpu, obs_module_text("CoreML"), USEGPU_COREML);
#endif

	// Add a informative text about the plugin
	// replace the placeholder with the current version using std::regex_replace
	std::string basic_info = std::regex_replace(PLUGIN_INFO_TEMPLATE, std::regex("%1"), PLUGIN_VERSION);
	// Check for update
	if (get_latest_version() != nullptr) {
		basic_info += std::regex_replace(PLUGIN_INFO_TEMPLATE_UPDATE_AVAILABLE, std::regex("%1"),
						 get_latest_version());
	}
	obs_properties_add_text(props, "info", basic_info.c_str(), OBS_TEXT_INFO);

	return props;
}

void enhance_filter_defaults(obs_data_t *settings)
{
	obs_data_set_default_double(settings, "blend", 1.0);
	obs_data_set_default_int(settings, "numThreads", 1);
	obs_data_set_default_string(settings, "model_select", MODEL_ENHANCE_TBEFN);
#if defined(__APPLE__)
	obs_data_set_default_string(settings, "useGPU", USEGPU_CPU);
#else
	// Linux
	obs_data_set_default_string(settings, "useGPU", USEGPU_CPU);
#endif
}

void enhance_filter_activate(void *data)
{
	auto *ptr = static_cast<std::shared_ptr<enhance_filter> *>(data);
	if (!ptr) {
		return;
	}

	std::shared_ptr<enhance_filter> tf = *ptr;
	if (tf) {
		tf->isDisabled = false;
	}
}

void enhance_filter_deactivate(void *data)
{
	auto *ptr = static_cast<std::shared_ptr<enhance_filter> *>(data);
	if (!ptr) {
		return;
	}

	std::shared_ptr<enhance_filter> tf = *ptr;
	if (tf) {
		tf->isDisabled = true;
	}
}

void enhance_filter_update(void *data, obs_data_t *settings)
{
	UNUSED_PARAMETER(settings);

	auto *ptr = static_cast<std::shared_ptr<enhance_filter> *>(data);
	if (!ptr) {
		return;
	}

	std::shared_ptr<enhance_filter> tf = *ptr;
	if (!tf) {
		return;
	}

	tf->blendFactor = (float)obs_data_get_double(settings, "blend");
	const uint32_t newNumThreads = (uint32_t)obs_data_get_int(settings, "numThreads");
	const std::string newModel = obs_data_get_string(settings, "model_select");
	const std::string newUseGpu = obs_data_get_string(settings, "useGPU");

	if (tf->modelSelection.empty() || tf->modelSelection != newModel || tf->useGPU != newUseGpu ||
	    tf->numThreads != newNumThreads) {
		// Lock modelMutex to prevent race condition with video_tick
		std::unique_lock<std::mutex> lock(tf->modelMutex);

		tf->numThreads = newNumThreads;
		tf->modelSelection = newModel;
		if (tf->modelSelection == MODEL_ENHANCE_TBEFN) {
			tf->model.reset(new ModelTBEFN);
		} else if (tf->modelSelection == MODEL_ENHANCE_ZERODCE) {
			tf->model.reset(new ModelZeroDCE);
		} else if (tf->modelSelection == MODEL_ENHANCE_URETINEX) {
			tf->model.reset(new ModelURetinex);
		} else {
			tf->model.reset(new ModelBCHW);
		}
		tf->useGPU = newUseGpu;
		createOrtSession(tf.get());
	}

	if (tf->blendEffect == nullptr) {
		obs_enter_graphics();

		char *effect_path = obs_module_file(BLEND_EFFECT_PATH);
		tf->blendEffect = gs_effect_create_from_file(effect_path, NULL);
		bfree(effect_path);

		obs_leave_graphics();
	}
}

void *enhance_filter_create(obs_data_t *settings, obs_source_t *source)
{
	try {
		// Create the instance as a shared_ptr
		auto instance = std::make_shared<enhance_filter>();

		instance->source = source;
		instance->texrender = gs_texrender_create(GS_BGRA, GS_ZS_NONE);

		std::string instanceName{"enhance-portrait-inference"};
		instance->env.reset(new Ort::Env(OrtLoggingLevel::ORT_LOGGING_LEVEL_ERROR, instanceName.c_str()));

		// Create pointer to shared_ptr for the update call
		auto ptr = new std::shared_ptr<enhance_filter>(instance);
		enhance_filter_update(ptr, settings);

		// Return the pointer to the shared_ptr
		// This keeps the reference count at least 1 until destroy is called
		return ptr;
	} catch (const std::exception &e) {
		obs_log(LOG_ERROR, "Failed to create enhance filter: %s", e.what());
		return nullptr;
	}
}

void enhance_filter_destroy(void *data)
{
	// Cast back to shared_ptr pointer
	auto *ptr = static_cast<std::shared_ptr<enhance_filter> *>(data);
	if (ptr) {
		if (*ptr) {
			// Mark as disabled to prevent further processing
			(*ptr)->isDisabled = true;

			// Perform cleanup
			obs_enter_graphics();
			gs_texrender_destroy((*ptr)->texrender);
			if ((*ptr)->stagesurface) {
				gs_stagesurface_destroy((*ptr)->stagesurface);
			}
			gs_effect_destroy((*ptr)->blendEffect);
			obs_leave_graphics();
		}
		// Delete the pointer to shared_ptr
		// This decrements the ref count. If no other threads hold a shared_ptr, the instance is deleted
		delete ptr;
	}
}

void enhance_filter_video_tick(void *data, float seconds)
{
	UNUSED_PARAMETER(seconds);

	auto *ptr = static_cast<std::shared_ptr<enhance_filter> *>(data);
	if (!ptr) {
		return;
	}

	// Create a local shared_ptr
	// This guarantees the object stays alive for the duration of this function scope
	// even if filter_destroy is called on the main thread
	std::shared_ptr<enhance_filter> tf = *ptr;

	if (!tf || tf->isDisabled) {
		return;
	}

	if (!obs_source_enabled(tf->source)) {
		return;
	}

	if (tf->inputBGRA.empty()) {
		return;
	}

	// Get input image from source rendering pipeline
	cv::Mat imageBGRA;
	{
		std::unique_lock<std::mutex> lock(tf->inputBGRALock, std::try_to_lock);
		if (!lock.owns_lock()) {
			return;
		}
		imageBGRA = tf->inputBGRA.clone();
	}

	cv::Mat outputImage;
	{
		std::unique_lock<std::mutex> lock(tf->modelMutex);
		try {
			if (!runFilterModelInference(tf.get(), imageBGRA, outputImage)) {
				return;
			}
		} catch (const std::exception &e) {
			obs_log(LOG_ERROR, "Exception caught: %s", e.what());
			return;
		}
	}

	// Put output image back to source rendering pipeline
	{
		std::unique_lock<std::mutex> lock(tf->outputLock, std::try_to_lock);
		if (!lock.owns_lock()) {
			return;
		}

		// convert to RGBA
		cv::cvtColor(outputImage, tf->outputBGRA, cv::COLOR_BGR2RGBA);
	}
}

void enhance_filter_video_render(void *data, gs_effect_t *_effect)
{
	UNUSED_PARAMETER(_effect);

	auto *ptr = static_cast<std::shared_ptr<enhance_filter> *>(data);
	if (!ptr) {
		return;
	}

	// Create a local shared_ptr
	std::shared_ptr<enhance_filter> tf = *ptr;

	if (!tf) {
		return;
	}

	// Get input from source
	uint32_t width, height;
	if (!getRGBAFromStageSurface(tf.get(), width, height)) {
		obs_source_skip_video_filter(tf->source);
		return;
	}

	// Engage filter
	if (!obs_source_process_filter_begin(tf->source, GS_RGBA, OBS_ALLOW_DIRECT_RENDERING)) {
		obs_source_skip_video_filter(tf->source);
		return;
	}

	// Get output from neural network into texture
	gs_texture_t *outputTexture = nullptr;
	{
		std::lock_guard<std::mutex> lock(tf->outputLock);
		outputTexture = gs_texture_create(tf->outputBGRA.cols, tf->outputBGRA.rows, GS_BGRA, 1,
						  (const uint8_t **)&tf->outputBGRA.data, 0);
		if (!outputTexture) {
			obs_log(LOG_ERROR, "Failed to create output texture");
			obs_source_skip_video_filter(tf->source);
			return;
		}
	}

	gs_eparam_t *blendimage = gs_effect_get_param_by_name(tf->blendEffect, "blendimage");
	gs_eparam_t *blendFactor = gs_effect_get_param_by_name(tf->blendEffect, "blendFactor");
	gs_eparam_t *xOffset = gs_effect_get_param_by_name(tf->blendEffect, "xOffset");
	gs_eparam_t *yOffset = gs_effect_get_param_by_name(tf->blendEffect, "yOffset");

	gs_effect_set_texture(blendimage, outputTexture);
	gs_effect_set_float(blendFactor, tf->blendFactor);
	gs_effect_set_float(xOffset, 1.0f / float(width));
	gs_effect_set_float(yOffset, 1.0f / float(height));

	// Render texture
	gs_blend_state_push();
	gs_reset_blend_state();

	obs_source_process_filter_tech_end(tf->source, tf->blendEffect, 0, 0, "Draw");

	gs_blend_state_pop();

	gs_texture_destroy(outputTexture);
}
