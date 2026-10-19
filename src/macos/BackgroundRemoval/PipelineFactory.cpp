// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/BackgroundRemoval/PipelineFactory.hpp"
#include "EmbeddedModels/mediapipe_selfie_segmentation_landscape_fp16_ncnn.mem.h"
#include "MediaPipeLandscapeCoreMLPipeline.hpp"
#include "common/BackgroundRemoval/FilterProperty.hpp"
#include "common/BackgroundRemoval/MediaPipeLandscapeNcnnPipeline.hpp"

#include <stdexcept>
#include <utility>

#include <Effects/BackgroundRemovalEffect.hpp>
#include <obs-module.h>
#include <os/log.h>

#if __has_include(<ncnn/net.h>)
#include <ncnn/net.h>
#if NCNN_VULKAN
#include <ncnn/gpu.h>
#endif
#else
#include <net.h>
#if NCNN_VULKAN
#include <gpu.h>
#endif
#endif

namespace BackgroundRemoval {

namespace {

auto osLogger() noexcept -> os_log_t
{
	static os_log_t logger = os_log_create("com.royshil.obs-backgroundremoval", "PipelineFactory");
	return logger;
}

auto createMediaPipeLandscapeNcnnPipeline(const FilterProperty &property, obs_source_t *source,
					  Effects::BackgroundRemovalEffect &backgroundRemovalEffect,
					  std::uint32_t width, std::uint32_t height)
	-> std::shared_ptr<IRenderingPipeline>
{
	blog(LOG_INFO, OBS_LOG_HEADER "Recreating ncnn rendering pipeline (%ux%u)", width, height);
	os_log_with_type(osLogger(), OS_LOG_TYPE_INFO, "Recreating ncnn rendering pipeline (%{public}ux%{public}u)",
			 width, height);
	auto net = std::make_unique<ncnn::Net>();
	net->opt.num_threads = 1;
	net->opt.use_local_pool_allocator = true;
	net->opt.openmp_blocktime = 1;
	net->opt.use_fp16_packed = false;
	net->opt.use_fp16_storage = false;
	net->opt.use_fp16_arithmetic = false;
	net->opt.use_fp16_uniform = false;
#if NCNN_VULKAN
	ncnn::create_gpu_instance();
	net->opt.use_vulkan_compute = ncnn::get_gpu_count() > 0;
#endif
	if (net->load_param(model_fp16_ncnn_param_bin) == 0 || net->load_model(model_fp16_ncnn_bin) == 0) {
		throw std::runtime_error("Failed to load the MediaPipe Landscape ncnn model");
	}
	return std::make_shared<MediaPipeLandscapeNcnnPipeline>(property, source, backgroundRemovalEffect,
								std::move(net), width, height);
}

auto createMediaPipeLandscapeNcnnCpuForcedPipeline(const FilterProperty &property, obs_source_t *source,
						   Effects::BackgroundRemovalEffect &backgroundRemovalEffect,
						   std::uint32_t width, std::uint32_t height)
	-> std::shared_ptr<IRenderingPipeline>
{
	blog(LOG_INFO, OBS_LOG_HEADER "Recreating ncnn CPU rendering pipeline (%ux%u)", width, height);
	os_log_with_type(osLogger(), OS_LOG_TYPE_INFO, "Recreating ncnn CPU rendering pipeline (%{public}ux%{public}u)",
			 width, height);
	auto net = std::make_unique<ncnn::Net>();
	net->opt.num_threads = 1;
	net->opt.use_local_pool_allocator = true;
	net->opt.openmp_blocktime = 1;
	net->opt.use_fp16_packed = false;
	net->opt.use_fp16_storage = false;
	net->opt.use_fp16_arithmetic = false;
	net->opt.use_fp16_uniform = false;
#if NCNN_VULKAN
	net->opt.use_vulkan_compute = false;
#endif
	if (net->load_param(model_fp16_ncnn_param_bin) == 0 || net->load_model(model_fp16_ncnn_bin) == 0) {
		throw std::runtime_error("Failed to load the MediaPipe Landscape ncnn model");
	}
	return std::make_shared<MediaPipeLandscapeNcnnCpuForcedPipeline>(property, source, backgroundRemovalEffect,
									 std::move(net), width, height);
}

auto createMediaPipeLandscapeCoreMLPipeline(const FilterProperty &property, obs_source_t *source,
					    Effects::BackgroundRemovalEffect &backgroundRemovalEffect,
					    std::uint32_t width, std::uint32_t height)
	-> std::shared_ptr<IRenderingPipeline>
{
	blog(LOG_INFO, OBS_LOG_HEADER "Recreating CoreML rendering pipeline (%ux%u)", width, height);
	os_log_with_type(osLogger(), OS_LOG_TYPE_INFO, "Recreating CoreML rendering pipeline (%{public}ux%{public}u)",
			 width, height);
	return std::make_shared<MediaPipeLandscapeCoreMLPipeline>(property, source, backgroundRemovalEffect, width,
								  height);
}

} // namespace

auto createRenderingPipeline(const FilterProperty &property, obs_source_t *source,
			     Effects::BackgroundRemovalEffect &backgroundRemovalEffect, std::uint32_t width,
			     std::uint32_t height) -> std::shared_ptr<IRenderingPipeline>
{
	switch (property.pipelineId) {
	case MediaPipeLandscapeNcnnPipeline::kPipelineId:
		return createMediaPipeLandscapeNcnnPipeline(property, source, backgroundRemovalEffect, width, height);
	case MediaPipeLandscapeNcnnCpuForcedPipeline::kPipelineId:
		return createMediaPipeLandscapeNcnnCpuForcedPipeline(property, source, backgroundRemovalEffect, width,
								     height);
	case MediaPipeLandscapeCoreMLPipeline::kPipelineId:
		return createMediaPipeLandscapeCoreMLPipeline(property, source, backgroundRemovalEffect, width, height);
	default:
		blog(LOG_ERROR, OBS_LOG_HEADER "Unknown rendering pipeline ID: %lld", property.pipelineId);
		os_log_with_type(osLogger(), OS_LOG_TYPE_ERROR, "Unknown rendering pipeline ID: %{public}lld",
				 property.pipelineId);
		return nullptr;
	}
}

} // namespace BackgroundRemoval
