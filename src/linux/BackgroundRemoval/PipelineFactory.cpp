// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "common/BackgroundRemoval/PipelineFactory.hpp"
#include "EmbeddedModels/mediapipe_selfie_segmentation_landscape_fp16_ncnn.mem.h"
#include "common/BackgroundRemoval/FilterProperty.hpp"
#include "common/BackgroundRemoval/MediaPipeLandscapeNcnnPipeline.hpp"

#include <stdexcept>
#include <utility>

#include <Effects/BackgroundRemovalEffect.hpp>
#include <obs-module.h>

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

auto createMediaPipeLandscapeNcnnPipeline(const FilterProperty &property, obs_source_t *source,
					  Effects::BackgroundRemovalEffect &backgroundRemovalEffect,
					  std::uint32_t width, std::uint32_t height)
	-> std::shared_ptr<IRenderingPipeline>
{
	blog(LOG_INFO, OBS_LOG_HEADER "Recreating ncnn rendering pipeline (%ux%u)", width, height);
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
	default:
		blog(LOG_ERROR, OBS_LOG_HEADER "Unknown rendering pipeline ID: %lld", property.pipelineId);
		return nullptr;
	}
}

} // namespace BackgroundRemoval
