// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "MPLSEffect.hpp"

#include <memory>
#include <stdexcept>
#include <string>

#include <util/bmem.h>

// A pass: the pixel shader PS_<name> with the given body, and the technique <name> that draws with it.
#define MPLS_PASS(name, body) \
	"\nfloat4 PS_" #name "(VertData v_in) : TARGET\n{\n" body "}\n" \
	"technique " #name " { pass { vertex_shader = VSDefault(v_in); pixel_shader = PS_" #name "(v_in); } }\n"

// A layer on an output tensor of shape width x height x slices: opos is the position of the
// fragment in it, and the pixel shader returns the expression, written in HLSL as is.
#define MPLS_LAYER(name, width, height, slices, ...) \
	MPLS_PASS(name, "\tint3 opos = vertToPos(v_in, " #width ", " #height ", " #slices ");\n" \
			"\treturn " #__VA_ARGS__ ";\n")

// A pass whose pixel shader returns the expression, written in HLSL as is.
#define MPLS_SIMPLE(name, ...) MPLS_PASS(name, "\treturn " #__VA_ARGS__ ";\n")

namespace BackgroundRemoval {

namespace {

// The effect source of MediaPipe Selfie Segmenter (landscape): the preamble, then a pixel shader
// and a technique per pass. Layers are named after the ONNX initializers (conv2_1 for
// conv2_1.weight). The preprocessor joins it all into one string literal.
// clang-format off
constexpr char kMPLSEffectCode[] = R"EFFECT(
/*
 * Pos and Coords
 *
 * Pos
 *   A position in ML terms: int3(x, y, s), column x, row y and slice s of a tensor of C channels of
 *   W x H values (the batch size is 1). Slice s is the four channels 4s .. 4s + 3; a tensor has
 *   S = ceil(C / 4) slices, and channels past C are zero.
 * Coords
 *   The texture coordinates passed to Load(): int3(column, row, 0), where the last component is
 *   the mip level.
 * NC4HW4 layout
 *   A tensor is held in an RGBA32F texture of (S * W) x H texels: Pos (x, y, s) is at Coords
 *   (s * W + x, y, 0). Weight and bias textures have their own layouts; see
 *   Models/MediaPipeLandscape/Shader/GenerateWeights.py.
 * Fragment
 *   The render-target pixel a pixel shader computes, given by v_in.uv in [0, 1] at the pixel
 *   center. A pass draws the whole texture of its output tensor.
 */

uniform float4x4 ViewProj;

uniform texture2d tensor;
uniform texture2d scale;

uniform texture2d tensorB;
uniform texture2d scaleB;

uniform texture2d tensorR;
uniform texture2d weights;
uniform texture2d bias;
uniform texture2d image;

sampler_state linearSampler {
	Filter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct VertData {
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
};

VertData VSDefault(VertData v_in)
{
	VertData vert_out;
	vert_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv = v_in.uv;
	return vert_out;
}

/*
 * Every shape is fixed. Sizes are passed as literal arguments, so after inlining all loop
 * counts and tensor sizes are constants. Texels are read with Load(), as OBS's own effects do.
 */

/* MARK: Pos and Coords */

/**
 * Pos of the current fragment in the NC4HW4 texture of an output tensor of the given shape.
 * @return int3(column, row, slice)
 */
int3 vertToPos(VertData v_in, int width, int height, int slices)
{
	int tx = int(v_in.uv.x * float(width * slices));
	int y = int(v_in.uv.y * float(height));
	int s = tx / width;
	return int3(tx - s * width, y, s);
}

/**
 * Coords of a Pos in the NC4HW4 texture of a tensor of the given width.
 * A 1x1 tensor (a scale or a bias) is read at posToCoords(int3(0, 0, s), 1).
 * @return int3(column, row, 0)
 */
int3 posToCoords(int3 pos, int width)
{
	return int3(pos.z * width + pos.x, pos.y, 0);
}

/**
 * As posToCoords, with the Pos clamped into a tensor of the given size. Multiply the loaded value
 * by zeroIfPosIsOutside() to read zero padding without branches.
 * @return int3(column, row, 0)
 */
int3 posToClampedCoords(int3 pos, int width, int height)
{
	return int3(pos.z * width + clamp(pos.x, 0, width - 1), clamp(pos.y, 0, height - 1), 0);
}

/**
 * @return 1.0 when the Pos is inside a tensor of the given size, otherwise 0.0
 */
float zeroIfPosIsOutside(int3 pos, int width, int height)
{
	return (pos.x >= 0 && pos.x < width && pos.y >= 0 && pos.y < height) ? 1.0 : 0.0;
}

/* MARK: Activations */

float4 relu(float4 value)
{
	return max(value, float4(0.0, 0.0, 0.0, 0.0));
}

float4 hardSwish(float4 value)
{
	return value * clamp(value / 6.0 + 0.5, 0.0, 1.0);
}

float4 sigmoid(float4 value)
{
	return 1.0 / (1.0 + exp(-value));
}

/* MARK: Operators */

/*
 * Dense layers read the 4x4 weight block of output slice so and block b at texels
 * (b * 4 .. b * 4 + 3, so); texel ci holds the weights from input channel ci of the block to the
 * four output channels of slice so, so the block is applied as four multiply-adds.
 */

/*
 * Pointwise convolution over A, which has the output's width and the given slice count.
 * Uniforms: tensor, weights, bias.
 */
float4 conv1x1(int3 opos, int width, int slices)
{
	int3 biaspos = int3(0, 0, opos.z);
	float4 sum = bias.Load(posToCoords(biaspos, 1));
	for (int si = 0; si < slices; si++) {
		int3 apos = int3(opos.x, opos.y, si);
		float4 value = tensor.Load(posToCoords(apos, width));
		int x = si * 4;
		sum += weights.Load(int3(x, opos.z, 0)) * value.x;
		sum += weights.Load(int3(x + 1, opos.z, 0)) * value.y;
		sum += weights.Load(int3(x + 2, opos.z, 0)) * value.z;
		sum += weights.Load(int3(x + 3, opos.z, 0)) * value.w;
	}
	return sum;
}

/*
 * Pointwise convolution over A scaled per channel by scale.
 * Uniforms: tensor, scale, weights, bias.
 */
float4 conv1x1Scaled(int3 opos, int width, int slices)
{
	int3 biaspos = int3(0, 0, opos.z);
	float4 sum = bias.Load(posToCoords(biaspos, 1));
	for (int si = 0; si < slices; si++) {
		int3 apos = int3(opos.x, opos.y, si);
		int3 spos = int3(0, 0, si);
		float4 value = tensor.Load(posToCoords(apos, width)) * scale.Load(posToCoords(spos, 1));
		int x = si * 4;
		sum += weights.Load(int3(x, opos.z, 0)) * value.x;
		sum += weights.Load(int3(x + 1, opos.z, 0)) * value.y;
		sum += weights.Load(int3(x + 2, opos.z, 0)) * value.z;
		sum += weights.Load(int3(x + 3, opos.z, 0)) * value.w;
	}
	return sum;
}

/*
 * Pointwise convolution over the input + B * scaleB; B has the shape of the input.
 * Uniforms: tensor, tensorB, scaleB, weights, bias.
 */
float4 conv1x1AddScaledB(int3 opos, int width, int slices)
{
	int3 biaspos = int3(0, 0, opos.z);
	float4 sum = bias.Load(posToCoords(biaspos, 1));
	for (int si = 0; si < slices; si++) {
		int3 apos = int3(opos.x, opos.y, si);
		int3 bpos = int3(opos.x, opos.y, si);
		int3 spos = int3(0, 0, si);
		float4 value = tensor.Load(posToCoords(apos, width)) +
			       tensorB.Load(posToCoords(bpos, width)) * scaleB.Load(posToCoords(spos, 1));
		int x = si * 4;
		sum += weights.Load(int3(x, opos.z, 0)) * value.x;
		sum += weights.Load(int3(x + 1, opos.z, 0)) * value.y;
		sum += weights.Load(int3(x + 2, opos.z, 0)) * value.z;
		sum += weights.Load(int3(x + 3, opos.z, 0)) * value.w;
	}
	return sum;
}

/*
 * Depthwise convolution over A of the given size.
 * Uniforms: tensor, weights, bias.
 */
float4 depthwise(int3 opos, int width, int height, int kernelSize, int stride, int padBegin)
{
	int3 biaspos = int3(0, 0, opos.z);
	float4 sum = bias.Load(posToCoords(biaspos, 1));
	for (int ky = 0; ky < kernelSize; ky++) {
		for (int kx = 0; kx < kernelSize; kx++) {
			int3 apos = int3(opos.x * stride - padBegin + kx, opos.y * stride - padBegin + ky, opos.z);
			float4 value = tensor.Load(posToClampedCoords(apos, width, height)) *
				       zeroIfPosIsOutside(apos, width, height);
			sum += value * weights.Load(int3(ky * kernelSize + kx, opos.z, 0));
		}
	}
	return sum;
}

/*
 * Sums each texel column of A of the given shape. The output is (width * slices) x 1.
 * Uniforms: tensor.
 */
float4 columnSum(VertData v_in, int width, int height, int slices)
{
	int tx = int(v_in.uv.x * float(width * slices));
	float4 sum = float4(0.0, 0.0, 0.0, 0.0);
	for (int y = 0; y < height; y++)
		sum += tensor.Load(int3(tx, y, 0));
	return sum;
}

/*
 * Sums each texel column of the input + B of the given shape. The output is (width * slices) x 1.
 * Uniforms: tensor, tensorB.
 */
float4 columnSumAddB(VertData v_in, int width, int height, int slices)
{
	int tx = int(v_in.uv.x * float(width * slices));
	float4 sum = float4(0.0, 0.0, 0.0, 0.0);
	for (int y = 0; y < height; y++)
		sum += tensor.Load(int3(tx, y, 0)) + tensorB.Load(int3(tx, y, 0));
	return sum;
}

/*
 * Averages a column-sum texture of a tensor of the given shape per slice: A is the column sums,
 * a (width * slices) x 1 texture read as a 1-row tensor of the given width. The output is slices x 1.
 * Uniforms: tensor.
 */
float4 rowMean(VertData v_in, int width, int height, int slices)
{
	int s = int(v_in.uv.x * float(slices));
	float4 sum = float4(0.0, 0.0, 0.0, 0.0);
	for (int x = 0; x < width; x++) {
		int3 apos = int3(x, 0, s);
		sum += tensor.Load(posToCoords(apos, width));
	}
	return sum / float(width * height);
}

/* MARK: Operators that add a tensor */

/*
 * conv1x1, then + R of the output shape (pointwise: the output has the width of A).
 * Uniforms: tensor (the input), tensorR (the residual), weights, bias.
 */
float4 conv1x1Residual(int3 opos, int width, int slices)
{
	return conv1x1(opos, width, slices) + tensorR.Load(posToCoords(opos, width));
}

/*
 * conv1x1Scaled, then + R of the output shape.
 * Uniforms: tensor (the input), tensorR (the residual), scale, weights, bias.
 */
float4 conv1x1ScaledResidual(int3 opos, int width, int slices)
{
	return conv1x1Scaled(opos, width, slices) + tensorR.Load(posToCoords(opos, width));
}

/*
 * depthwise with stride 1, then Relu, then + A.
 * Uniforms: tensor, weights, bias.
 */
float4 depthwiseReluAddInput(int3 opos, int width, int height, int kernelSize, int stride, int padBegin)
{
	return relu(depthwise(opos, width, height, kernelSize, stride, padBegin)) + tensor.Load(posToCoords(opos, width));
}

/* MARK: Layers with their own shapes */

/*
 * Dense 3x3 convolution, stride 2, padding (0, 0, 1, 1), over A of the given shape.
 * Uniforms: tensor, weights, bias.
 */
float4 conv3x3Stride2(int3 opos, int width, int height, int slices)
{
	int3 biaspos = int3(0, 0, opos.z);
	float4 sum = bias.Load(posToCoords(biaspos, 1));
	for (int si = 0; si < slices; si++) {
		for (int ky = 0; ky < 3; ky++) {
			for (int kx = 0; kx < 3; kx++) {
				int3 apos = int3(opos.x * 2 + kx, opos.y * 2 + ky, si);
				float4 value = tensor.Load(posToClampedCoords(apos, width, height)) *
					       zeroIfPosIsOutside(apos, width, height);
				int x = ((si * 3 + ky) * 3 + kx) * 4;
				sum += weights.Load(int3(x, opos.z, 0)) * value.x;
				sum += weights.Load(int3(x + 1, opos.z, 0)) * value.y;
				sum += weights.Load(int3(x + 2, opos.z, 0)) * value.z;
				sum += weights.Load(int3(x + 3, opos.z, 0)) * value.w;
			}
		}
	}
	return sum;
}

/*
 * Transposed 2x2 convolution, stride 2, to one channel of an output of the given size, over A of
 * the given width and slice count.
 * Uniforms: tensor, weights, bias.
 */
float convTranspose2x2(VertData v_in, int outWidth, int outHeight, int width, int slices)
{
	int x = int(v_in.uv.x * float(outWidth));
	int y = int(v_in.uv.y * float(outHeight));
	int ix = x / 2;
	int iy = y / 2;
	int k = (y - iy * 2) * 2 + (x - ix * 2);
	int3 biaspos = int3(0, 0, 0);
	float sum = bias.Load(posToCoords(biaspos, 1)).x;
	for (int s = 0; s < slices; s++) {
		int3 apos = int3(ix, iy, s);
		sum += dot(tensor.Load(posToCoords(apos, width)), weights.Load(int3(k, s, 0)));
	}
	return sum;
}

/*
 * convTranspose2x2, then Sigmoid, as a probability in every channel but alpha.
 * Uniforms: tensor, weights, bias.
 */
float4 convTranspose2x2Sigmoid(VertData v_in, int outWidth, int outHeight, int width, int slices)
{
	float probability = 1.0 / (1.0 + exp(-convTranspose2x2(v_in, outWidth, outHeight, width, slices)));
	return float4(probability, probability, probability, 1.0);
}

/* MARK: Drawing */

/*
 * The source image, resampled to the render target; used to shrink it to the model input.
 * Uniforms: image.
 */
float4 drawImage(VertData v_in)
{
	return image.Sample(linearSampler, v_in.uv);
}

/* MARK: Decoder inputs: resize by 2x, then pointwise convolution */

/*
 * final_block1_conv1: the head (16x9x32) gated per channel by scale, resized by 2x to 32x18, then 1x1 to 32x18x6.
 * The resize is ONNX Resize (linear, half_pixel) for exactly 2x: per axis, 3/4 of the nearest
 * element (opos / 2) and 1/4 of the next one toward opos, clamped at the edges.
 * Uniforms: tensor, scale, weights, bias.
 */
float4 PS_final_block1_conv1(VertData v_in) : TARGET
{
	int3 opos = vertToPos(v_in, 32, 18, 6);
	int nx = opos.x / 2;
	int ny = opos.y / 2;
	int fx = clamp(nx + (opos.x - nx * 2) * 2 - 1, 0, 15);
	int fy = clamp(ny + (opos.y - ny * 2) * 2 - 1, 0, 8);
	int3 biaspos = int3(0, 0, opos.z);
	float4 sum = bias.Load(posToCoords(biaspos, 1));
	for (int si = 0; si < 32; si++) {
		int3 apos00 = int3(nx, ny, si);
		int3 apos10 = int3(fx, ny, si);
		int3 apos01 = int3(nx, fy, si);
		int3 apos11 = int3(fx, fy, si);
		int3 spos = int3(0, 0, si);
		float4 value = (tensor.Load(posToCoords(apos00, 16)) * 0.5625 + tensor.Load(posToCoords(apos10, 16)) * 0.1875 +
				tensor.Load(posToCoords(apos01, 16)) * 0.1875 + tensor.Load(posToCoords(apos11, 16)) * 0.0625) * scale.Load(posToCoords(spos, 1));
		int x = si * 4;
		sum += weights.Load(int3(x, opos.z, 0)) * value.x;
		sum += weights.Load(int3(x + 1, opos.z, 0)) * value.y;
		sum += weights.Load(int3(x + 2, opos.z, 0)) * value.z;
		sum += weights.Load(int3(x + 3, opos.z, 0)) * value.w;
	}
	return sum;
}

technique final_block1_conv1 { pass { vertex_shader = VSDefault(v_in); pixel_shader = PS_final_block1_conv1(v_in); } }

/*
 * final_block2_conv1: final_block1 (32x18x6) resized by 2x to 64x36, then 1x1 to 64x36x4.
 * The resize is ONNX Resize (linear, half_pixel) for exactly 2x: per axis, 3/4 of the nearest
 * element (opos / 2) and 1/4 of the next one toward opos, clamped at the edges.
 * Uniforms: tensor, weights, bias.
 */
float4 PS_final_block2_conv1(VertData v_in) : TARGET
{
	int3 opos = vertToPos(v_in, 64, 36, 4);
	int nx = opos.x / 2;
	int ny = opos.y / 2;
	int fx = clamp(nx + (opos.x - nx * 2) * 2 - 1, 0, 31);
	int fy = clamp(ny + (opos.y - ny * 2) * 2 - 1, 0, 17);
	int3 biaspos = int3(0, 0, opos.z);
	float4 sum = bias.Load(posToCoords(biaspos, 1));
	for (int si = 0; si < 6; si++) {
		int3 apos00 = int3(nx, ny, si);
		int3 apos10 = int3(fx, ny, si);
		int3 apos01 = int3(nx, fy, si);
		int3 apos11 = int3(fx, fy, si);
		float4 value = (tensor.Load(posToCoords(apos00, 32)) * 0.5625 + tensor.Load(posToCoords(apos10, 32)) * 0.1875 +
				tensor.Load(posToCoords(apos01, 32)) * 0.1875 + tensor.Load(posToCoords(apos11, 32)) * 0.0625);
		int x = si * 4;
		sum += weights.Load(int3(x, opos.z, 0)) * value.x;
		sum += weights.Load(int3(x + 1, opos.z, 0)) * value.y;
		sum += weights.Load(int3(x + 2, opos.z, 0)) * value.z;
		sum += weights.Load(int3(x + 3, opos.z, 0)) * value.w;
	}
	return sum;
}

technique final_block2_conv1 { pass { vertex_shader = VSDefault(v_in); pixel_shader = PS_final_block2_conv1(v_in); } }

/*
 * final_block3_conv1: final_block2 (64x36x4) resized by 2x to 128x72, then 1x1 to 128x72x4.
 * The resize is ONNX Resize (linear, half_pixel) for exactly 2x: per axis, 3/4 of the nearest
 * element (opos / 2) and 1/4 of the next one toward opos, clamped at the edges.
 * Uniforms: tensor, weights, bias.
 */
float4 PS_final_block3_conv1(VertData v_in) : TARGET
{
	int3 opos = vertToPos(v_in, 128, 72, 4);
	int nx = opos.x / 2;
	int ny = opos.y / 2;
	int fx = clamp(nx + (opos.x - nx * 2) * 2 - 1, 0, 63);
	int fy = clamp(ny + (opos.y - ny * 2) * 2 - 1, 0, 35);
	int3 biaspos = int3(0, 0, opos.z);
	float4 sum = bias.Load(posToCoords(biaspos, 1));
	for (int si = 0; si < 4; si++) {
		int3 apos00 = int3(nx, ny, si);
		int3 apos10 = int3(fx, ny, si);
		int3 apos01 = int3(nx, fy, si);
		int3 apos11 = int3(fx, fy, si);
		float4 value = (tensor.Load(posToCoords(apos00, 64)) * 0.5625 + tensor.Load(posToCoords(apos10, 64)) * 0.1875 +
				tensor.Load(posToCoords(apos01, 64)) * 0.1875 + tensor.Load(posToCoords(apos11, 64)) * 0.0625);
		int x = si * 4;
		sum += weights.Load(int3(x, opos.z, 0)) * value.x;
		sum += weights.Load(int3(x + 1, opos.z, 0)) * value.y;
		sum += weights.Load(int3(x + 2, opos.z, 0)) * value.z;
		sum += weights.Load(int3(x + 3, opos.z, 0)) * value.w;
	}
	return sum;
}

technique final_block3_conv1 { pass { vertex_shader = VSDefault(v_in); pixel_shader = PS_final_block3_conv1(v_in); } }
)EFFECT"
	// MARK: Encoder
	MPLS_LAYER(conv1, 128, 72, 4, hardSwish(conv3x3Stride2(opos, 256, 144, 1)))
	MPLS_LAYER(conv2_1, 128, 72, 4, relu(conv1x1(opos, 128, 4)))
	MPLS_LAYER(conv2_2, 64, 36, 4, relu(depthwise(opos, 128, 72, 3, 2, 0)))
	MPLS_LAYER(conv3_1, 1, 1, 2, relu(conv1x1(opos, 1, 4)))
	MPLS_LAYER(conv3_2, 1, 1, 4, sigmoid(conv1x1(opos, 1, 2)))
	MPLS_LAYER(conv3_3, 64, 36, 4, conv1x1Scaled(opos, 64, 4))
	MPLS_LAYER(conv4_1, 64, 36, 18, relu(conv1x1(opos, 64, 4)))
	MPLS_LAYER(conv4_2, 32, 18, 18, relu(depthwise(opos, 64, 36, 3, 2, 0)))
	MPLS_LAYER(conv4_3, 32, 18, 6, conv1x1(opos, 32, 18))
	MPLS_LAYER(conv5_1, 32, 18, 22, relu(conv1x1(opos, 32, 6)))
	MPLS_LAYER(conv5_2, 32, 18, 22, relu(depthwise(opos, 32, 18, 3, 1, 1)))
	MPLS_LAYER(conv5_3, 32, 18, 6, conv1x1Residual(opos, 32, 22))
	MPLS_LAYER(conv6_1, 32, 18, 24, hardSwish(conv1x1(opos, 32, 6)))
	MPLS_LAYER(conv6_2, 16, 9, 24, hardSwish(depthwise(opos, 32, 18, 5, 2, 1)))
	MPLS_LAYER(sub_block1_conv1, 1, 1, 6, relu(conv1x1(opos, 1, 24)))
	MPLS_LAYER(sub_block1_conv2, 1, 1, 24, sigmoid(conv1x1(opos, 1, 6)))
	MPLS_LAYER(sub_block1_conv3, 16, 9, 8, conv1x1Scaled(opos, 16, 24))
	MPLS_LAYER(block1_conv1, 16, 9, 32, hardSwish(conv1x1(opos, 16, 8)))
	MPLS_LAYER(block1_conv2, 16, 9, 32, hardSwish(depthwise(opos, 16, 9, 5, 1, 2)))
	MPLS_LAYER(block1_sub_block_conv1, 1, 1, 8, relu(conv1x1(opos, 1, 32)))
	MPLS_LAYER(block1_sub_block_conv2, 1, 1, 32, sigmoid(conv1x1(opos, 1, 8)))
	MPLS_LAYER(block1_sub_block_conv3, 16, 9, 8, conv1x1ScaledResidual(opos, 16, 32))
	MPLS_LAYER(block2_conv1, 16, 9, 32, hardSwish(conv1x1(opos, 16, 8)))
	MPLS_LAYER(block2_conv2, 16, 9, 32, hardSwish(depthwise(opos, 16, 9, 5, 1, 2)))
	MPLS_LAYER(block2_sub_block_conv1, 1, 1, 8, relu(conv1x1(opos, 1, 32)))
	MPLS_LAYER(block2_sub_block_conv2, 1, 1, 32, sigmoid(conv1x1(opos, 1, 8)))
	MPLS_LAYER(block2_sub_block_conv3, 16, 9, 8, conv1x1ScaledResidual(opos, 16, 32))
	MPLS_LAYER(block3_conv1, 16, 9, 24, hardSwish(conv1x1(opos, 16, 8)))
	MPLS_LAYER(block3_conv2, 16, 9, 24, hardSwish(depthwise(opos, 16, 9, 5, 1, 2)))
	MPLS_LAYER(block3_sub_block_conv1, 1, 1, 6, relu(conv1x1(opos, 1, 24)))
	MPLS_LAYER(block3_sub_block_conv2, 1, 1, 24, sigmoid(conv1x1(opos, 1, 6)))
	MPLS_LAYER(block3_sub_block_conv3, 16, 9, 8, conv1x1ScaledResidual(opos, 16, 24))
	MPLS_LAYER(block4_conv1, 16, 9, 24, hardSwish(conv1x1(opos, 16, 8)))
	MPLS_LAYER(block4_conv2, 16, 9, 24, hardSwish(depthwise(opos, 16, 9, 5, 1, 2)))
	MPLS_LAYER(block4_sub_block_conv1, 1, 1, 6, relu(conv1x1(opos, 1, 24)))
	MPLS_LAYER(block4_sub_block_conv2, 1, 1, 24, sigmoid(conv1x1(opos, 1, 6)))
	MPLS_LAYER(block4_sub_block_conv3, 16, 9, 8, conv1x1ScaledResidual(opos, 16, 24))
	MPLS_LAYER(conv7, 16, 9, 32, relu(conv1x1(opos, 16, 8)))
	MPLS_LAYER(conv8, 1, 1, 32, sigmoid(conv1x1(opos, 1, 8)))

	// MARK: Decoder
	MPLS_LAYER(final_block1_conv2, 1, 1, 6, relu(conv1x1(opos, 1, 6)))
	MPLS_LAYER(final_block1_conv3, 1, 1, 6, sigmoid(conv1x1(opos, 1, 6)))
	MPLS_LAYER(final_block1_conv4, 32, 18, 6, relu(conv1x1AddScaledB(opos, 32, 6)))
	MPLS_LAYER(final_block1_conv5, 32, 18, 6, depthwiseReluAddInput(opos, 32, 18, 3, 1, 1))
	MPLS_LAYER(final_block2_conv2, 1, 1, 4, relu(conv1x1(opos, 1, 4)))
	MPLS_LAYER(final_block2_conv3, 1, 1, 4, sigmoid(conv1x1(opos, 1, 4)))
	MPLS_LAYER(final_block2_conv4, 64, 36, 4, relu(conv1x1AddScaledB(opos, 64, 4)))
	MPLS_LAYER(final_block2_conv5, 64, 36, 4, depthwiseReluAddInput(opos, 64, 36, 3, 1, 1))
	MPLS_LAYER(final_block3_conv2, 1, 1, 4, relu(conv1x1(opos, 1, 4)))
	MPLS_LAYER(final_block3_conv3, 1, 1, 4, sigmoid(conv1x1(opos, 1, 4)))
	MPLS_LAYER(final_block3_conv4, 128, 72, 4, relu(conv1x1AddScaledB(opos, 128, 4)))
	MPLS_LAYER(final_block3_conv5, 128, 72, 4, depthwiseReluAddInput(opos, 128, 72, 3, 1, 1))
	MPLS_SIMPLE(conv_transpose, convTranspose2x2Sigmoid(v_in, 256, 144, 128, 4))

	// MARK: ReduceMean
	MPLS_SIMPLE(ColumnSum_64x36x4, columnSum(v_in, 64, 36, 4))
	MPLS_SIMPLE(ColumnSum_16x9x24, columnSum(v_in, 16, 9, 24))
	MPLS_SIMPLE(ColumnSum_16x9x32, columnSum(v_in, 16, 9, 32))
	MPLS_SIMPLE(ColumnSum_16x9x8, columnSum(v_in, 16, 9, 8))
	MPLS_SIMPLE(ColumnSumAddB_32x18x6, columnSumAddB(v_in, 32, 18, 6))
	MPLS_SIMPLE(ColumnSumAddB_64x36x4, columnSumAddB(v_in, 64, 36, 4))
	MPLS_SIMPLE(ColumnSumAddB_128x72x4, columnSumAddB(v_in, 128, 72, 4))
	MPLS_SIMPLE(RowMean_64x36x4, rowMean(v_in, 64, 36, 4))
	MPLS_SIMPLE(RowMean_16x9x24, rowMean(v_in, 16, 9, 24))
	MPLS_SIMPLE(RowMean_16x9x32, rowMean(v_in, 16, 9, 32))
	MPLS_SIMPLE(RowMean_16x9x8, rowMean(v_in, 16, 9, 8))
	MPLS_SIMPLE(RowMean_32x18x6, rowMean(v_in, 32, 18, 6))
	MPLS_SIMPLE(RowMean_128x72x4, rowMean(v_in, 128, 72, 4))

	// MARK: Drawing
	MPLS_SIMPLE(Draw, drawImage(v_in));
// clang-format on

auto requireParam(gs_effect_t *effect, const char *name) -> gs_eparam_t *
{
	gs_eparam_t *const param = gs_effect_get_param_by_name(effect, name);
	if (!param) {
		throw std::runtime_error(std::string("MediaPipeLandscape effect parameter is missing: ") + name);
	}
	return param;
}

} // namespace

MPLSEffect::MPLSEffect(gs_effect_t *effect)
	: effect(effect),
	  epTensor(requireParam(effect, "tensor")),
	  epScale(requireParam(effect, "scale")),
	  epTensorB(requireParam(effect, "tensorB")),
	  epScaleB(requireParam(effect, "scaleB")),
	  epTensorR(requireParam(effect, "tensorR")),
	  epWeights(requireParam(effect, "weights")),
	  epBias(requireParam(effect, "bias")),
	  epImage(requireParam(effect, "image"))
{
	// gs_effect_loop() draws nothing when a technique is missing. The per-layer techniques are
	// checked by MPLSPipeline.
	if (!gs_effect_get_technique(effect, "Draw")) {
		throw std::runtime_error("MediaPipeLandscape effect technique is missing: Draw");
	}
}

const MPLSEffect &makeMPLSEffect()
{
	static char *s_pErrorString = nullptr;
	static gs_effect_t *s_effect = gs_effect_create(kMPLSEffectCode, "kMPLSEffectCode", &s_pErrorString);
	static std::unique_ptr<char, void (*)(void *)> s_errorString(s_pErrorString, &bfree);
	if (!s_effect) {
		throw std::runtime_error(s_pErrorString ? s_pErrorString : "Unknown error");
	}
	static const MPLSEffect s_mplsEffect(s_effect);
	return s_mplsEffect;
}

} // namespace BackgroundRemoval
