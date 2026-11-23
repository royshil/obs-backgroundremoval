// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <obs.h>

#include <BackgroundRemoval/IRenderingPipeline.hpp>
#include <BackgroundRemoval/MaskCompositor/MaskCompositor.hpp>

#include "MPLSEffect.hpp"

namespace BackgroundRemoval {

namespace MPLSDetail::MPLSWeights {

/// One convolution layer: an RGBA16F weight texture of weightsWidth x weightsHeight texels and
/// an RGBA16F bias texture of biasWidth x 1 texels, both listed row by row without padding as
/// IEEE half bit patterns. The effect technique for the layer has the same name. The layers are
/// defined in generated/MPLSPipeline.generated.cpp.
struct Layer final {
	const char *name;
	const std::uint16_t *weights;
	std::uint32_t weightsWidth;
	std::uint32_t weightsHeight;
	const std::uint16_t *bias;
	std::uint32_t biasWidth;
};

} // namespace MPLSDetail::MPLSWeights

/// Baseline background removal with MediaPipe Selfie Segmenter (landscape, 256x144).
///
/// The network is evaluated entirely by hand-written OBS pixel shaders, so it runs wherever
/// OBS itself renders, with no inference runtime and no CPU readback. The source frame is then
/// drawn by MaskCompositor with the background removed or blurred.
///
/// Construct, render and destroy instances only inside the OBS graphics context.
class MPLSPipeline final : public IRenderingPipeline {
public:
	/// Processes frames of sourceWidth x sourceHeight, the size of the filter's target, and draws the
	/// output at 1 / outputDownscaleFactor of it (see downscaledSize()). Evaluates the network and
	/// draws with effects shared by all pipelines (makeMPLSEffect() and makeMaskCompositorEffect()).
	/// @throws std::runtime_error when an effect cannot be created or lacks a technique or a
	/// parameter, or a texture cannot be created.
	MPLSPipeline(obs_source_t *source, std::uint32_t sourceWidth, std::uint32_t sourceHeight,
		     std::int64_t outputDownscaleFactor);
	~MPLSPipeline() noexcept override;

	MPLSPipeline(const MPLSPipeline &) = delete;
	auto operator=(const MPLSPipeline &) -> MPLSPipeline & = delete;
	MPLSPipeline(MPLSPipeline &&) = delete;
	auto operator=(MPLSPipeline &&) -> MPLSPipeline & = delete;

	auto pipelineId() const noexcept -> std::int64_t override { return kMPLSPipelineId; }
	auto width() const noexcept -> std::uint32_t override { return outputWidth_; }
	auto height() const noexcept -> std::uint32_t override { return outputHeight_; }

	void setBlurBgEnabled(bool value) noexcept override;
	void setBlurBgStrength(double value) noexcept override;
	/// Places the ratio at focusPointFromRatio().
	void setBlurBgFocusRatio(double percent) noexcept override;

	/// The focus point at focus ratio 0%, where the two blur modes balance for the distribution of
	/// this model's output.
	static constexpr double kBalanceFocusPoint = 0.5;

	/// The focus point of a focus ratio in [-100, 100]. The refined mask puts the person at
	/// background share 0 and the background at 1, so -100% is focus point 0, 0% is
	/// kBalanceFocusPoint and 100% is 1, linear on each side.
	static constexpr auto focusPointFromRatio(double ratio) noexcept -> double
	{
		return ratio < 0.0 ? kBalanceFocusPoint * (1.0 + ratio / 100.0)
				   : kBalanceFocusPoint + (1.0 - kBalanceFocusPoint) * ratio / 100.0;
	}

	/// The focus ratio of a focus point in [0, 1]; the inverse of focusPointFromRatio().
	static constexpr auto focusRatioFromPoint(double point) noexcept -> double
	{
		return point < kBalanceFocusPoint ? 100.0 * (point / kBalanceFocusPoint - 1.0)
						  : 100.0 * (point - kBalanceFocusPoint) / (1.0 - kBalanceFocusPoint);
	}

	/// Renders the filtered source frame to the current render target at the output size. Draws
	/// nothing after a GPU failure, or while the target's size differs from the one the pipeline
	/// was created for.
	void videoRender() noexcept override;

	/// The foreground probability of the latest videoRender(), kModelWidth x kModelHeight in R8.
	/// For tests and diagnostics.
	auto probability() const noexcept -> gs_texture_t * { return probability_.get(); }

	static constexpr std::uint32_t kModelWidth = 256;
	static constexpr std::uint32_t kModelHeight = 144;

private:
	struct TextureDeleter {
		void operator()(gs_texture_t *texture) const noexcept { gs_texture_destroy(texture); }
	};
	using UniqueTexture = std::unique_ptr<gs_texture_t, TextureDeleter>;

	/// A packed tensor; see MPLSEffect.cpp for the layout.
	struct Tensor {
		UniqueTexture texture;
		std::uint32_t width = 0;
		std::uint32_t height = 0;
		std::uint32_t slices = 0;
	};

	using Layer = MPLSDetail::MPLSWeights::Layer;

	/// The weight and bias textures of one layer, created once. The technique has the layer's name.
	struct LayerResources {
		UniqueTexture weights;
		UniqueTexture bias;
	};

	/// The textures of one pass, one per texture parameter; null for the ones it does not read.
	struct PassTextures {
		/// The input of every pass (A), and its per-channel scale.
		gs_texture_t *tensor = nullptr;
		gs_texture_t *scale = nullptr;
		/// A second input (B), and its per-channel scale.
		gs_texture_t *tensorB = nullptr;
		gs_texture_t *scaleB = nullptr;
		/// A residual added to the output (R).
		gs_texture_t *tensorR = nullptr;
		gs_texture_t *weights = nullptr;
		gs_texture_t *bias = nullptr;
		gs_texture_t *image = nullptr;
	};

	void infer();

	auto allocate(std::uint32_t width, std::uint32_t height, std::uint32_t slices) -> const Tensor &;
	/// Checks the technique on first use, and returns its name, valid for the pipeline's lifetime.
	auto technique(const std::string &name) -> const char *;
	auto resources(const Layer &layer) const -> const LayerResources &;
	void draw(const char *technique, gs_texture_t *target, std::uint32_t width, std::uint32_t height,
		  const PassTextures &textures) noexcept;
	void drawPass(const char *technique, const Tensor &output, const PassTextures &textures) noexcept;
	auto layerTextures(const Layer &layer, const Tensor &a) const -> PassTextures;

	// Dense layers, one per combination of inputs its technique reads.
	auto conv(const Layer &layer, const Tensor &a, std::uint32_t channels, std::uint32_t width,
		  std::uint32_t height) -> const Tensor &;
	auto scaledConv(const Layer &layer, const Tensor &a, const Tensor &scale, std::uint32_t channels,
			std::uint32_t width, std::uint32_t height) -> const Tensor &;
	auto residualConv(const Layer &layer, const Tensor &a, const Tensor &residual, std::uint32_t channels,
			  std::uint32_t width, std::uint32_t height) -> const Tensor &;
	auto scaledResidualConv(const Layer &layer, const Tensor &a, const Tensor &scale, const Tensor &residual,
				std::uint32_t channels, std::uint32_t width, std::uint32_t height) -> const Tensor &;
	auto mergeConv(const Layer &layer, const Tensor &a, const Tensor &b, const Tensor &scaleB,
		       std::uint32_t channels, std::uint32_t width, std::uint32_t height) -> const Tensor &;
	auto dense(const Layer &layer, const Tensor &a, std::uint32_t channels, std::uint32_t width,
		   std::uint32_t height, const PassTextures &textures) -> const Tensor &;
	auto depthwise(const Layer &layer, const Tensor &a, std::uint32_t width, std::uint32_t height)
		-> const Tensor &;
	auto mean(const Tensor &a, const Tensor *b = nullptr) -> const Tensor &;
	auto squeezeExcitation(const Layer &reduce, const Layer &expand, std::uint32_t hidden, const Tensor &a)
		-> const Tensor &;
	auto decoderBlock(const Layer &conv1, const Layer &conv2, const Layer &conv3, const Layer &conv4,
			  const Layer &conv5, const Tensor &low, const Tensor *lowScale, const Tensor &skip,
			  std::uint32_t channels) -> const Tensor &;
	void deconv(const Layer &layer, const Tensor &a);

	obs_source_t *const source_;
	const MPLSEffect &effect_;
	MaskCompositor compositor_;
	std::unordered_map<const Layer *, LayerResources> layers_;
	/// Technique names checked on first use (the per-shape ReduceMean passes).
	std::unordered_set<std::string> techniques_;
	const std::uint32_t sourceWidth_;
	const std::uint32_t sourceHeight_;
	const std::uint32_t outputWidth_;
	const std::uint32_t outputHeight_;
	/// The filter's target, rendered at its own size.
	UniqueTexture sourceTexture_;
	Tensor modelInput_;
	UniqueTexture probability_;
	/// Intermediate tensors in program order. The program is fixed, so the same sequence of
	/// allocations repeats every frame; the textures are created on the first frame.
	std::deque<Tensor> tensors_;
	std::size_t nextTensor_ = 0;
	bool failed_ = false;
};

} // namespace BackgroundRemoval
