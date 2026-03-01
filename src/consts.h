/*
 * SPDX-FileCopyrightText: Copyright (C) 2021 Roy Shilkrot roy.shil@gmail.com
 * SPDX-License-Identifier: GPL-3.0-or-later
 *
 * OBS Plugin: Portrait Background Removal / Virtual Green-screen and Low-Light Enhancement
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef CONSTS_H
#define CONSTS_H

const char *const MODEL_SINET = "models/SINet_Softmax_simple.with_runtime_opt.ort";
const char *const MODEL_MEDIAPIPE = "models/mediapipe.with_runtime_opt.ort";
const char *const MODEL_SELFIE = "models/selfie_segmentation.with_runtime_opt.ort";
const char *const MODEL_SELFIE_MULTICLASS = "models/selfie_multiclass_256x256.with_runtime_opt.ort";
const char *const MODEL_RVM = "models/rvm_mobilenetv3_fp32.with_runtime_opt.ort";
const char *const MODEL_PPHUMANSEG = "models/pphumanseg_fp32.with_runtime_opt.ort";
const char *const MODEL_ENHANCE_TBEFN = "models/tbefn_fp32.with_runtime_opt.ort";
const char *const MODEL_ENHANCE_URETINEX = "models/uretinex_net_180x320.with_runtime_opt.ort";
const char *const MODEL_ENHANCE_SGLLIE = "models/semantic_guided_llie_180x324.with_runtime_opt.ort";
const char *const MODEL_ENHANCE_ZERODCE = "models/zero_dce_180x320.with_runtime_opt.ort";
const char *const MODEL_DEPTH_TCMONODEPTH = "models/tcmonodepth_tcsmallnet_192x320.with_runtime_opt.ort";
const char *const MODEL_RMBG = "models/bria_rmbg_1_4_qint8.with_runtime_opt.ort";

const char *const USEGPU_CPU = "cpu";
const char *const USEGPU_CUDA = "cuda";
const char *const USEGPU_ROCM = "rocm";
const char *const USEGPU_MIGRAPHX = "migraphx";
const char *const USEGPU_TENSORRT = "tensorrt";
const char *const USEGPU_COREML = "coreml";

const char *const EFFECT_PATH = "effects/mask_alpha_filter.effect";
const char *const KAWASE_BLUR_EFFECT_PATH = "effects/kawase_blur.effect";
const char *const BLEND_EFFECT_PATH = "effects/blend_images.effect";

const char *const PLUGIN_INFO_TEMPLATE =
	"<a href=\"https://github.com/royshil/obs-backgroundremoval/\">Background Removal</a> (%1) by "
	"<a href=\"https://github.com/royshil\">Roy Shilkrot</a> ❤️ "
	"<a href=\"https://github.com/sponsors/royshil\">Support & Follow</a>";
const char *const PLUGIN_INFO_TEMPLATE_UPDATE_AVAILABLE =
	"<center><a href=\"https://github.com/royshil/obs-backgroundremoval/releases\">🚀 Update available! (%1)</a></center>";

#endif /* CONSTS_H */
