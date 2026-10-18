// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <BackgroundRemoval/windows/FilterContext.hpp>
#include <BackgroundRemoval/windows/MPLD10Pipeline.hpp>
#include <QApplication>
#include <QEvent>
#include <QPointer>
#include <obs.hpp>
#include <atomic>
#include <exception>
#include <functional>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition, const char *message)
{
	if (!condition)
		throw std::runtime_error(message);
}

void videoTask(std::function<void()> task)
{
	std::exception_ptr error;
	std::function<void()> guarded = [&] {
		try {
			task();
		} catch (...) {
			error = std::current_exception();
		}
	};
	obs_queue_task(
		OBS_TASK_GRAPHICS, [](void *data) { (*static_cast<std::function<void()> *>(data))(); }, &guarded, true);
	if (error)
		std::rethrow_exception(error);
}

struct Input {
	std::uint32_t width = 256; // Video thread only after source creation.
	std::atomic<unsigned> renders{0};
};
} // namespace

auto runFilterContextLifecycle() -> bool
{
	using BackgroundRemoval::FilterContext;
	using BackgroundRemoval::MPLD10Pipeline;
	try {
		obs_add_data_path(OBS_TEST_BIN_DIR "/../../data/libobs/");
		obs_video_info video{};
		video.graphics_module = OBS_TEST_BIN_DIR "/libobs-d3d11.dll";
		video.fps_num = 30;
		video.fps_den = 1;
		video.base_width = video.output_width = 256;
		video.base_height = video.output_height = 144;
		video.output_format = VIDEO_FORMAT_RGBA;
		video.colorspace = VIDEO_CS_709;
		video.range = VIDEO_RANGE_FULL;
		require(obs_reset_video(&video) == OBS_VIDEO_SUCCESS, "OBS video initialization failed");

		obs_source_info inputInfo{};
		inputInfo.id = "facade-lifecycle-input";
		inputInfo.type = OBS_SOURCE_TYPE_INPUT;
		inputInfo.output_flags = OBS_SOURCE_VIDEO | OBS_SOURCE_CUSTOM_DRAW;
		inputInfo.get_name = [](void *) {
			return "Facade lifecycle input";
		};
		inputInfo.create = [](obs_data_t *, obs_source_t *) -> void * {
			return new Input;
		};
		inputInfo.destroy = [](void *data) {
			delete static_cast<Input *>(data);
		};
		inputInfo.get_width = [](void *data) {
			return static_cast<Input *>(data)->width;
		};
		inputInfo.get_height = [](void *) -> std::uint32_t {
			return 144;
		};
		inputInfo.video_render = [](void *data, gs_effect_t *) {
			++static_cast<Input *>(data)->renders;
		};
		obs_register_source(&inputInfo);
		obs_source_info filterInfo = FilterContext::kSourceInfo;
		filterInfo.id = "facade-lifecycle-filter";
		obs_register_source(&filterInfo);
		OBSSourceAutoRelease input = obs_source_create_private(inputInfo.id, "Lifecycle input", nullptr);
		OBSDataAutoRelease settings = obs_data_create();
		obs_data_set_int(settings, "v2_PipelineId", -1);
		OBSSourceAutoRelease filter = obs_source_create_private(filterInfo.id, "Lifecycle filter", settings);
		require(input && filter, "Source creation failed");
		obs_source_filter_add(input, filter);
		obs_source_inc_showing(input);
		obs_source_inc_active(input);
		struct Deactivate {
			obs_source_t *source;
			~Deactivate()
			{
				obs_source_dec_active(source);
				obs_source_dec_showing(source);
			}
		} deactivate{input};
		auto *inputData = static_cast<Input *>(obs_obj_get_data(input));
		auto *handle = static_cast<std::shared_ptr<FilterContext> *>(obs_obj_get_data(filter));
		auto context = *handle;

		const auto tick = [&] {
			videoTask([&] { FilterContext::videoTick(handle, 1.0F / 30); });
		};
		const auto render = [&] {
			videoTask([&] {
				obs_enter_graphics();
				auto *target = gs_texture_create(256, 144, GS_BGRA, 1, nullptr, GS_RENDER_TARGET);
				if (!target) {
					obs_leave_graphics();
					throw std::runtime_error("Test render target creation failed");
				}
				auto *previous = gs_get_render_target();
				const auto previousSpace = gs_get_color_space();
				gs_viewport_push();
				gs_projection_push();
				gs_set_render_target(target, nullptr);
				gs_set_viewport(0, 0, 256, 144);
				gs_ortho(0, 256, 0, 144, -100, 100);
				obs_source_video_render(input);
				gs_set_render_target_with_color_space(previous, nullptr, previousSpace);
				gs_projection_pop();
				gs_viewport_pop();
				gs_texture_destroy(target);
				obs_leave_graphics();
			});
		};
		tick();
		require(!context->renderingPipeline_.load(), "Unknown pipeline must fail creation");
		const auto beforeFallback = inputData->renders.load();
		render();
		require(inputData->renders.load() > beforeFallback, "Missing pipeline must render the input");

		obs_data_set_int(settings, "v2_PipelineId", 0);
		obs_source_update(filter, settings);
		tick();
		auto pipeline = context->renderingPipeline_.load();
		require(bool(pipeline), "Valid selection must recover after creation failure");
		auto concrete = std::dynamic_pointer_cast<MPLD10Pipeline>(pipeline);
		require(bool(concrete), "Expected MPLD10 pipeline");
		obs_source_set_enabled(filter, false);
		obs_data_set_double(settings, "v2_MaskAdjustment", 15.0);
		obs_data_set_double(settings, "v2_MotionSuppression", 25.0);
		obs_source_update(filter, settings);
		tick();
		require(context->renderingPipeline_.load() == pipeline, "Disabling must retain the pipeline");
		require(concrete->imageSimilarityThreshold_.load() == 35.0F, "Disabled setting update was lost");
		const auto beforeDisabled = inputData->renders.load();
		render();
		require(inputData->renders.load() > beforeDisabled, "Disabled filter must pass through input");
		obs_source_set_enabled(filter, true);
		tick();
		const auto beforeEnabled = inputData->renders.load();
		render();
		require(inputData->renders.load() > beforeEnabled, "Re-enabled filter must resume input capture");

		// D3D11 rejects dimensions above its 16384 texture-size limit.
		videoTask([&] {
			inputData->width = 16385;
			FilterContext::videoTick(handle, 1.0F / 30);
		});
		require(!context->renderingPipeline_.load(), "Resource creation failure must not publish a pipeline");
		videoTask([&] {
			inputData->width = 256;
			FilterContext::videoTick(handle, 1.0F / 30);
		});
		require(bool(context->renderingPipeline_.load()),
			"Valid dimensions must recover after resource failure");
		require(context->renderingPipeline_.load() != pipeline, "Recovery must create a new instance");
		require(std::dynamic_pointer_cast<MPLD10Pipeline>(context->renderingPipeline_.load())
					->imageSimilarityThreshold_.load() == 35.0F,
			"Recovery must retain latest settings");
		pipeline.reset();
		concrete.reset();

		FilterContext::handleAutoSensitivityClick(nullptr, nullptr, handle);
		QPointer<BackgroundRemoval::AutoConfigurator> dialog = context->autoConfigurator_;
		require(bool(dialog), "Configurator did not open");
		dialog->startMeasurement();
		tick();
		require(!context->motionMeasurement_.readable(), "Measurement must be writing before removal");
		std::weak_ptr<FilterContext> weakContext = context;
		OBSWeakSourceAutoRelease weakSource = obs_source_get_weak_source(filter);
		videoTask([&] {
			obs_source_filter_remove(input, filter);
			obs_source_remove(filter);
		});
		filter = nullptr;
		context.reset();
		obs_wait_for_destroy_queue();
		require(!weakContext.expired(), "Dialog must retain its source and context after removal");
		dialog->reject();
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		require(!dialog, "Qt must destroy the closed dialog");
		videoTask([] {});
		obs_wait_for_destroy_queue();
		require(weakContext.expired(), "Context was retained after dialog destruction");
		OBSSourceAutoRelease remaining = obs_weak_source_get_source(weakSource);
		require(!remaining, "Removed source was retained after dialog destruction");
		std::cout << "FilterContext lifecycle checks passed\n";
		return true;
	} catch (const std::exception &error) {
		std::cerr << "FilterContext lifecycle failure: " << error.what() << '\n';
		return false;
	}
}
