// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MPLSPipeline.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <utility>
#include <cstdint>
#include <stdexcept>
#include <string>

#include <graphics/vec4.h>

#include <plugin-support.h>

#include "MPLSEffect.hpp"

// The layers, defined in generated/MPLSPipeline.generated.cpp by
// Models/MediaPipeLandscape/Shader/GenerateWeights.py.
namespace BackgroundRemoval::MPLSDetail::MPLSWeights {

extern const Layer conv1;
extern const Layer conv2_1;
extern const Layer conv2_2;
extern const Layer conv3_1;
extern const Layer conv3_2;
extern const Layer conv3_3;
extern const Layer conv4_1;
extern const Layer conv4_2;
extern const Layer conv4_3;
extern const Layer conv5_1;
extern const Layer conv5_2;
extern const Layer conv5_3;
extern const Layer conv6_1;
extern const Layer conv6_2;
extern const Layer sub_block1_conv1;
extern const Layer sub_block1_conv2;
extern const Layer sub_block1_conv3;
extern const Layer block1_conv1;
extern const Layer block1_conv2;
extern const Layer block1_sub_block_conv1;
extern const Layer block1_sub_block_conv2;
extern const Layer block1_sub_block_conv3;
extern const Layer block2_conv1;
extern const Layer block2_conv2;
extern const Layer block2_sub_block_conv1;
extern const Layer block2_sub_block_conv2;
extern const Layer block2_sub_block_conv3;
extern const Layer block3_conv1;
extern const Layer block3_conv2;
extern const Layer block3_sub_block_conv1;
extern const Layer block3_sub_block_conv2;
extern const Layer block3_sub_block_conv3;
extern const Layer block4_conv1;
extern const Layer block4_conv2;
extern const Layer block4_sub_block_conv1;
extern const Layer block4_sub_block_conv2;
extern const Layer block4_sub_block_conv3;
extern const Layer conv7;
extern const Layer conv8;
extern const Layer final_block1_conv1;
extern const Layer final_block1_conv2;
extern const Layer final_block1_conv3;
extern const Layer final_block1_conv4;
extern const Layer final_block1_conv5;
extern const Layer final_block2_conv1;
extern const Layer final_block2_conv2;
extern const Layer final_block2_conv3;
extern const Layer final_block2_conv4;
extern const Layer final_block2_conv5;
extern const Layer final_block3_conv1;
extern const Layer final_block3_conv2;
extern const Layer final_block3_conv3;
extern const Layer final_block3_conv4;
extern const Layer final_block3_conv5;
extern const Layer conv_transpose;

extern const std::array<const Layer *, 55> layers;

} // namespace BackgroundRemoval::MPLSDetail::MPLSWeights

namespace BackgroundRemoval::MPLSDetail {

namespace {

constexpr auto slicesOf(std::uint32_t channels) noexcept -> std::uint32_t
{
	return (channels + 3) / 4;
}

auto toInt(std::uint32_t value) noexcept -> int
{
	return static_cast<int>(value);
}

/// Saves the render state that the pipeline changes and restores it on destruction.
class RenderStateGuard final {
public:
	RenderStateGuard() noexcept
		: renderTarget_(gs_get_render_target()),
		  zstencil_(gs_get_zstencil_target()),
		  colorSpace_(gs_get_color_space())
	{
		gs_blend_state_push();
		gs_matrix_push();
		gs_projection_push();
		gs_viewport_push();
	}

	~RenderStateGuard() noexcept
	{
		gs_viewport_pop();
		gs_projection_pop();
		gs_matrix_pop();
		gs_blend_state_pop();
		gs_set_render_target_with_color_space(renderTarget_, zstencil_, colorSpace_);
	}

	RenderStateGuard(const RenderStateGuard &) = delete;
	auto operator=(const RenderStateGuard &) -> RenderStateGuard & = delete;
	RenderStateGuard(RenderStateGuard &&) = delete;
	auto operator=(RenderStateGuard &&) -> RenderStateGuard & = delete;

private:
	gs_texture_t *const renderTarget_;
	gs_zstencil_t *const zstencil_;
	const gs_color_space colorSpace_;
};

// gs_effect_loop() draws nothing when a technique is missing, so check the names up front.
void requireTechnique(gs_effect_t *effect, const char *name)
{
	if (!gs_effect_get_technique(effect, name)) {
		throw std::runtime_error(std::string("MediaPipeLandscape effect technique is missing: ") + name);
	}
}

} // namespace

} // namespace BackgroundRemoval::MPLSDetail

namespace BackgroundRemoval {

namespace {

namespace W = MPLSDetail::MPLSWeights;

using MPLSDetail::RenderStateGuard;
using MPLSDetail::requireTechnique;
using MPLSDetail::slicesOf;
using MPLSDetail::toInt;

} // namespace

MPLSPipeline::MPLSPipeline(obs_source_t *source, std::uint32_t sourceWidth, std::uint32_t sourceHeight,
			   std::int64_t outputDownscaleFactor)
	: source_(source),
	  effect_(makeMPLSEffect()),
	  sourceWidth_(sourceWidth),
	  sourceHeight_(sourceHeight),
	  outputWidth_(downscaledSize(sourceWidth, outputDownscaleFactor)),
	  outputHeight_(downscaledSize(sourceHeight, outputDownscaleFactor))
{
	if (!source_ || sourceWidth_ == 0 || sourceHeight_ == 0) {
		throw std::invalid_argument("MPLSPipeline requires a source and a frame size");
	}

	// One technique (named after the layer) and one weight and bias texture per layer. Weights are
	// RGBA16F, which holds them exactly; the generated arrays are rows of IEEE half texels without
	// padding, as gs_texture_create() reads them. Shaders read them as floats.
	for (const Layer *layer : W::layers) {
		LayerResources resources;
		requireTechnique(effect_.effect, layer->name);
		const auto *weights = reinterpret_cast<const std::uint8_t *>(layer->weights);
		const auto *bias = reinterpret_cast<const std::uint8_t *>(layer->bias);
		resources.weights.reset(
			gs_texture_create(layer->weightsWidth, layer->weightsHeight, GS_RGBA16F, 1, &weights, 0));
		resources.bias.reset(gs_texture_create(layer->biasWidth, 1, GS_RGBA16F, 1, &bias, 0));
		if (!resources.weights || !resources.bias) {
			throw std::runtime_error(std::string("Failed to create the weight textures of ") + layer->name);
		}
		layers_.emplace(layer, std::move(resources));
	}

	modelInput_.texture.reset(gs_texture_create(kModelWidth, kModelHeight, GS_BGRA, 1, nullptr, GS_RENDER_TARGET));
	modelInput_.width = kModelWidth;
	modelInput_.height = kModelHeight;
	modelInput_.slices = 1;
	// MaskCompositor only compares the foreground probability with the threshold, so 8 bits are
	// enough.
	probability_.reset(gs_texture_create(kModelWidth, kModelHeight, GS_R8, 1, nullptr, GS_RENDER_TARGET));
	sourceTexture_.reset(gs_texture_create(sourceWidth_, sourceHeight_, GS_BGRA, 1, nullptr, GS_RENDER_TARGET));
	if (!modelInput_.texture || !probability_ || !sourceTexture_) {
		throw std::runtime_error("Failed to create the MediaPipeLandscape textures");
	}
}

MPLSPipeline::~MPLSPipeline() noexcept = default;

void MPLSPipeline::setBlurBgEnabled(bool value) noexcept
{
	compositor_.setBlurEnabled(value);
}

void MPLSPipeline::setBlurBgStrength(double value) noexcept
{
	compositor_.setBlurStrength(static_cast<float>(value));
}

void MPLSPipeline::setBlurBgFocusPoint(double value) noexcept
{
	compositor_.setBlurFocusPoint(static_cast<float>(value));
}

// MARK: - Rendering

void MPLSPipeline::videoRender() noexcept
{
	// The filter context recreates the pipeline when the target's size changes; until then, and
	// after a failure, draw nothing rather than the unfiltered background.
	obs_source_t *const target = obs_filter_get_target(source_);
	if (failed_ || !target || obs_source_get_base_width(target) != sourceWidth_ ||
	    obs_source_get_base_height(target) != sourceHeight_) {
		return;
	}
	const std::uint32_t width = sourceWidth_;
	const std::uint32_t height = sourceHeight_;

	try {
		RenderStateGuard guard;
		vec4 zero;
		vec4_zero(&zero);

		gs_matrix_identity();

		// Render the filter target the same way as obs_source_process_filter_begin().
		gs_set_render_target_with_color_space(sourceTexture_.get(), nullptr, GS_CS_SRGB);
		gs_ortho(0.0F, static_cast<float>(width), 0.0F, static_cast<float>(height), -100.0F, 100.0F);
		gs_set_viewport(0, 0, toInt(width), toInt(height));
		gs_clear(GS_CLEAR_COLOR, &zero, 0.0F, 0);
		gs_blend_function_separate(GS_BLEND_SRCALPHA, GS_BLEND_INVSRCALPHA, GS_BLEND_ONE, GS_BLEND_INVSRCALPHA);
		obs_source_t *const parent = obs_filter_get_parent(source_);
		const std::uint32_t parentFlags = parent ? obs_source_get_output_flags(parent) : 0;
		if (target == parent && (parentFlags & (OBS_SOURCE_CUSTOM_DRAW | OBS_SOURCE_ASYNC)) == 0) {
			obs_source_default_render(target);
		} else {
			obs_source_video_render(target);
		}

		gs_enable_blending(false);
		drawPass("Draw", modelInput_, PassTextures{.image = sourceTexture_.get()});

		infer();
		compositor_.prepare(sourceTexture_.get(), probability_.get(), width, height);
	} catch (const std::exception &exception) {
		obs_log(LOG_ERROR, "MPLSPipeline failed: %s", exception.what());
		failed_ = true;
		return;
	}

	// Processed at the target's size; only the output is drawn at the output size.
	compositor_.draw(sourceTexture_.get(), outputWidth_, outputHeight_);
}

// MARK: - Network

// MediaPipe Selfie Segmenter (landscape), written out by hand. Comments name the ONNX nodes of
// onnx-community/mediapipe_selfie_segmentation_landscape that each pass replaces.
void MPLSPipeline::infer()
{
	nextTensor_ = 0;

	// MARK: Encoder

	// conv1 + HardSwish; also the 128x72 skip connection.
	const Tensor &stem = conv(W::conv1, modelInput_, 16, 128, 72);
	const Tensor &expanded1 = conv(W::conv2_1, stem, 16, 128, 72);
	const Tensor &reduced1 = depthwise(W::conv2_2, expanded1, 64, 36);
	const Tensor &gate1 = squeezeExcitation(W::conv3_1, W::conv3_2, 8, reduced1);
	// Mul + conv3_3; also the 64x36 skip connection.
	const Tensor &skip64x36 = scaledConv(W::conv3_3, reduced1, gate1, 16, 64, 36);

	const Tensor &expanded2 = conv(W::conv4_1, skip64x36, 72, 64, 36);
	const Tensor &reduced2 = depthwise(W::conv4_2, expanded2, 32, 18);
	const Tensor &projected2 = conv(W::conv4_3, reduced2, 24, 32, 18);

	const Tensor &expanded3 = conv(W::conv5_1, projected2, 88, 32, 18);
	const Tensor &filtered3 = depthwise(W::conv5_2, expanded3, 32, 18);
	// conv5_3 + Add; also the 32x18 skip connection.
	const Tensor &skip32x18 = residualConv(W::conv5_3, filtered3, projected2, 24, 32, 18);

	const Tensor &expanded4 = conv(W::conv6_1, skip32x18, 96, 32, 18);
	const Tensor &reduced4 = depthwise(W::conv6_2, expanded4, 16, 9);
	const Tensor &gate4 = squeezeExcitation(W::sub_block1_conv1, W::sub_block1_conv2, 24, reduced4);
	const Tensor *features = &scaledConv(W::sub_block1_conv3, reduced4, gate4, 32, 16, 9);

	// block1 .. block4: inverted residual blocks with squeeze-and-excitation at 16x9.
	struct ResidualBlock {
		const Layer &conv1;
		const Layer &conv2;
		const Layer &reduce;
		const Layer &expand;
		const Layer &conv3;
		std::uint32_t channels;
		std::uint32_t hidden;
	};
	const ResidualBlock blocks[] = {
		{W::block1_conv1, W::block1_conv2, W::block1_sub_block_conv1, W::block1_sub_block_conv2,
		 W::block1_sub_block_conv3, 128, 32},
		{W::block2_conv1, W::block2_conv2, W::block2_sub_block_conv1, W::block2_sub_block_conv2,
		 W::block2_sub_block_conv3, 128, 32},
		{W::block3_conv1, W::block3_conv2, W::block3_sub_block_conv1, W::block3_sub_block_conv2,
		 W::block3_sub_block_conv3, 96, 24},
		{W::block4_conv1, W::block4_conv2, W::block4_sub_block_conv1, W::block4_sub_block_conv2,
		 W::block4_sub_block_conv3, 96, 24},
	};
	for (const ResidualBlock &block : blocks) {
		const Tensor &expanded = conv(block.conv1, *features, block.channels, 16, 9);
		const Tensor &filtered = depthwise(block.conv2, expanded, 16, 9);
		const Tensor &gate = squeezeExcitation(block.reduce, block.expand, block.hidden, filtered);
		// Mul + conv3 + Add
		features = &scaledResidualConv(block.conv3, filtered, gate, *features, 32, 16, 9);
	}

	// conv7 + Relu, gated by sigmoid(conv8(mean(block4))). The gate is applied when the decoder
	// reads the head, which is exact because resizing commutes with a per-channel scale.
	const Tensor &head = conv(W::conv7, *features, 128, 16, 9);
	const Tensor &headGate = conv(W::conv8, mean(*features), 128, 1, 1);

	// MARK: Decoder

	const Tensor &decoded1 = decoderBlock(W::final_block1_conv1, W::final_block1_conv2, W::final_block1_conv3,
					      W::final_block1_conv4, W::final_block1_conv5, head, &headGate, skip32x18,
					      24);
	const Tensor &decoded2 = decoderBlock(W::final_block2_conv1, W::final_block2_conv2, W::final_block2_conv3,
					      W::final_block2_conv4, W::final_block2_conv5, decoded1, nullptr,
					      skip64x36, 16);
	const Tensor &decoded3 = decoderBlock(W::final_block3_conv1, W::final_block3_conv2, W::final_block3_conv3,
					      W::final_block3_conv4, W::final_block3_conv5, decoded2, nullptr, stem,
					      16);

	// conv_transpose + Sigmoid into the 256x144 probability texture.
	deconv(W::conv_transpose, decoded3);
}

// final_blockN: resize 2x, project, merge the skip connection through a channel gate, then a
// residual depthwise refinement.
auto MPLSPipeline::decoderBlock(const Layer &conv1, const Layer &conv2, const Layer &conv3, const Layer &conv4,
				const Layer &conv5, const Tensor &low, const Tensor *lowScale, const Tensor &skip,
				std::uint32_t channels) -> const Tensor &
{
	const std::uint32_t width = skip.width;
	const std::uint32_t height = skip.height;

	// interp/Resize + conv1
	const Tensor &projected = lowScale ? scaledConv(conv1, low, *lowScale, channels, width, height)
					   : conv(conv1, low, channels, width, height);
	// Add + ReduceMean + conv2 + Relu + conv3 + Sigmoid
	const Tensor &pooled = mean(projected, &skip);
	const Tensor &hidden = conv(conv2, pooled, channels, 1, 1);
	const Tensor &gate = conv(conv3, hidden, channels, 1, 1);
	// Mul + Add_1 + conv4 + Relu_1
	const Tensor &merged = mergeConv(conv4, projected, skip, gate, channels, width, height);
	// conv5 + Relu_2 + Add_2
	return depthwise(conv5, merged, width, height);
}

// ReduceMean + conv1 + Relu + conv2 + Sigmoid: the per-channel gate of a squeeze-and-excitation block.
auto MPLSPipeline::squeezeExcitation(const Layer &reduce, const Layer &expand, std::uint32_t hidden, const Tensor &a)
	-> const Tensor &
{
	const Tensor &pooled = mean(a);
	const Tensor &squeezed = conv(reduce, pooled, hidden, 1, 1);
	return conv(expand, squeezed, a.slices * 4, 1, 1);
}

// MARK: - Passes

auto MPLSPipeline::allocate(std::uint32_t width, std::uint32_t height, std::uint32_t slices) -> const Tensor &
{
	if (nextTensor_ == tensors_.size()) {
		Tensor tensor;
		tensor.texture.reset(
			gs_texture_create(width * slices, height, GS_RGBA32F, 1, nullptr, GS_RENDER_TARGET));
		if (!tensor.texture) {
			throw std::runtime_error("Failed to create a MediaPipeLandscape tensor texture");
		}
		tensor.width = width;
		tensor.height = height;
		tensor.slices = slices;
		tensors_.push_back(std::move(tensor));
	}

	const Tensor &tensor = tensors_[nextTensor_++];
	if (tensor.width != width || tensor.height != height || tensor.slices != slices) {
		throw std::logic_error("MediaPipeLandscape tensor allocation sequence changed");
	}
	return tensor;
}

auto MPLSPipeline::technique(const std::string &name) -> const char *
{
	auto [it, inserted] = techniques_.insert(name);
	if (inserted) {
		requireTechnique(effect_.effect, it->c_str());
	}
	return it->c_str();
}

auto MPLSPipeline::resources(const Layer &layer) const -> const LayerResources &
{
	const auto it = layers_.find(&layer);
	if (it == layers_.end()) {
		throw std::logic_error(std::string("MediaPipeLandscape layer has no resources: ") + layer.name);
	}
	return it->second;
}

// Draws a pass. Every texture parameter is set in the loop body, right before the draw it is for;
// the end of the loop clears them all.
void MPLSPipeline::draw(const char *technique, gs_texture_t *target, std::uint32_t width, std::uint32_t height,
			const PassTextures &textures) noexcept
{
	gs_set_render_target_with_color_space(target, nullptr, GS_CS_SRGB);
	gs_ortho(0.0F, static_cast<float>(width), 0.0F, static_cast<float>(height), -100.0F, 100.0F);
	gs_set_viewport(0, 0, toInt(width), toInt(height));
	while (gs_effect_loop(effect_.effect, technique)) {
		gs_effect_set_texture(effect_.epTensor, textures.tensor);
		gs_effect_set_texture(effect_.epScale, textures.scale);
		gs_effect_set_texture(effect_.epTensorB, textures.tensorB);
		gs_effect_set_texture(effect_.epScaleB, textures.scaleB);
		gs_effect_set_texture(effect_.epTensorR, textures.tensorR);
		gs_effect_set_texture(effect_.epWeights, textures.weights);
		gs_effect_set_texture(effect_.epBias, textures.bias);
		gs_effect_set_texture(effect_.epImage, textures.image);
		gs_draw_sprite(nullptr, 0, width, height);
	}
}

void MPLSPipeline::drawPass(const char *technique, const Tensor &output, const PassTextures &textures) noexcept
{
	draw(technique, output.texture.get(), output.width * output.slices, output.height, textures);
}

// The textures of a layer's pass on input A: A, and the layer's weights and bias.
auto MPLSPipeline::layerTextures(const Layer &layer, const Tensor &a) const -> PassTextures
{
	const LayerResources &layerResources = resources(layer);
	return PassTextures{
		.tensor = a.texture.get(),
		.weights = layerResources.weights.get(),
		.bias = layerResources.bias.get(),
	};
}

// A dense layer reading A only.
auto MPLSPipeline::conv(const Layer &layer, const Tensor &a, std::uint32_t channels, std::uint32_t width,
			std::uint32_t height) -> const Tensor &
{
	return dense(layer, a, channels, width, height, layerTextures(layer, a));
}

// Mul + a dense layer: A scaled per channel by a 1x1 tensor.
auto MPLSPipeline::scaledConv(const Layer &layer, const Tensor &a, const Tensor &scale, std::uint32_t channels,
			      std::uint32_t width, std::uint32_t height) -> const Tensor &
{
	PassTextures textures = layerTextures(layer, a);
	textures.scale = scale.texture.get();
	return dense(layer, a, channels, width, height, textures);
}

// A dense layer + Add: a residual of the output size added after the activation.
auto MPLSPipeline::residualConv(const Layer &layer, const Tensor &a, const Tensor &residual, std::uint32_t channels,
				std::uint32_t width, std::uint32_t height) -> const Tensor &
{
	PassTextures textures = layerTextures(layer, a);
	textures.tensorR = residual.texture.get();
	return dense(layer, a, channels, width, height, textures);
}

// Mul + a dense layer + Add.
auto MPLSPipeline::scaledResidualConv(const Layer &layer, const Tensor &a, const Tensor &scale, const Tensor &residual,
				      std::uint32_t channels, std::uint32_t width, std::uint32_t height)
	-> const Tensor &
{
	PassTextures textures = layerTextures(layer, a);
	textures.scale = scale.texture.get();
	textures.tensorR = residual.texture.get();
	return dense(layer, a, channels, width, height, textures);
}

// Mul + Add + a dense layer: B, of the size of A, scaled per channel by a 1x1 tensor and added to A.
auto MPLSPipeline::mergeConv(const Layer &layer, const Tensor &a, const Tensor &b, const Tensor &scaleB,
			     std::uint32_t channels, std::uint32_t width, std::uint32_t height) -> const Tensor &
{
	PassTextures textures = layerTextures(layer, a);
	textures.tensorB = b.texture.get();
	textures.scaleB = scaleB.texture.get();
	return dense(layer, a, channels, width, height, textures);
}

// Draws a dense layer. Its technique fixes all shapes; the weight texture shape is checked against
// A and the output so that a wrong layer cannot be wired in silently.
auto MPLSPipeline::dense(const Layer &layer, const Tensor &a, std::uint32_t channels, std::uint32_t width,
			 std::uint32_t height, const PassTextures &textures) -> const Tensor &
{
	const std::uint32_t slices = slicesOf(channels);
	if (layer.weightsHeight != slices || layer.weightsWidth % (a.slices * 4) != 0) {
		throw std::logic_error(std::string("MediaPipeLandscape layer shape mismatch: ") + layer.name);
	}
	const Tensor &output = allocate(width, height, slices);
	drawPass(layer.name, output, textures);
	return output;
}

// A depthwise layer; the output has the slices of A.
auto MPLSPipeline::depthwise(const Layer &layer, const Tensor &a, std::uint32_t width, std::uint32_t height)
	-> const Tensor &
{
	if (layer.weightsHeight != a.slices) {
		throw std::logic_error(std::string("MediaPipeLandscape layer shape mismatch: ") + layer.name);
	}
	const Tensor &output = allocate(width, height, a.slices);
	drawPass(layer.name, output, layerTextures(layer, a));
	return output;
}

// ReduceMean over A, or over A + B: per-column sums, then per-slice means. The techniques are
// named after the shape of A.
auto MPLSPipeline::mean(const Tensor &a, const Tensor *b) -> const Tensor &
{
	const std::string shape =
		std::to_string(a.width) + "x" + std::to_string(a.height) + "x" + std::to_string(a.slices);

	const Tensor &columns = allocate(a.width * a.slices, 1, 1);
	drawPass(technique((b ? "ColumnSumAddB_" : "ColumnSum_") + shape), columns,
		 PassTextures{.tensor = a.texture.get(), .tensorB = b ? b->texture.get() : nullptr});

	const Tensor &output = allocate(1, 1, a.slices);
	drawPass(technique("RowMean_" + shape), output, PassTextures{.tensor = columns.texture.get()});
	return output;
}

void MPLSPipeline::deconv(const Layer &layer, const Tensor &a)
{
	draw(layer.name, probability_.get(), kModelWidth, kModelHeight, layerTextures(layer, a));
}

} // namespace BackgroundRemoval
