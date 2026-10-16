// SPDX-FileCopyrightText: 2026 Kaito Udagawa <umireon@kaito.tokyo>
//
// SPDX-License-Identifier: GPL-3.0-or-later

namespace BackgroundRemoval::MPLDDetail {

const char *gateEffectString = R"EFFECT(
uniform float4x4 ViewProj;
uniform texture2d image;
uniform texture2d image1;

sampler_state textureSampler {
	Filter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct VertDataIn {
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
};

struct VertDataOut {
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
};

VertDataOut VSDefault(VertDataIn v_in)
{
	VertDataOut vert_out;
	vert_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv = v_in.uv;
	return vert_out;
}

float luma709(float3 color)
{
	return dot(color, float3(0.2126, 0.7152, 0.0722));
}

float4 PSDraw(VertDataOut v_in) : TARGET
{
	return image.Sample(textureSampler, v_in.uv);
}

float4 PSLumaAbsDiff(VertDataOut v_in) : TARGET
{
	float4 leftPixel = image.Sample(textureSampler, v_in.uv);
	float4 rightPixel = image1.Sample(textureSampler, v_in.uv);
	float value = abs(luma709(leftPixel.rgb) - luma709(rightPixel.rgb));
	return float4(value, value, value, 1.0);
}

technique LumaAbsDiff
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSLumaAbsDiff(v_in);
	}
}

technique Draw
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSDraw(v_in);
	}
}
)EFFECT";

const char *renderingEffectString = R"EFFECT(
uniform float4x4 ViewProj;

uniform texture2d image;
uniform texture2d image1;
uniform texture2d image2;


uniform float xOffset;
uniform float yOffset;
uniform int blurIter;
uniform int blurTotal;
uniform float blurFocusPoint;
uniform float blurFocusDepth;

sampler_state textureSampler {
	Filter = Linear;
	AddressU = Clamp;
	AddressV = Clamp;
};

struct VertDataIn {
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
};

struct VertDataOut {
	float4 pos : POSITION;
	float2 uv : TEXCOORD0;
};

VertDataOut VSDefault(VertDataIn v_in)
{
	VertDataOut vert_out;
	vert_out.pos = mul(float4(v_in.pos.xyz, 1.0), ViewProj);
	vert_out.uv = v_in.uv;
	return vert_out;
}

float4 PSTakeBlur(VertDataOut v_in) : TARGET
{
	return float4(image2.Sample(textureSampler, v_in.uv).rgb, 1.0);
}

float4 PSDraw(VertDataOut v_in) : TARGET
{
	return image.Sample(textureSampler, v_in.uv);
}

float4 PSExtractScaledRGBPlanes(VertDataOut v_in) : TARGET
{
	float packedY = v_in.uv.y * 3.0;
	int plane = min(int(packedY), 2);
	float4 pixel = image.Sample(textureSampler, float2(v_in.uv.x, frac(packedY)));
	float value = plane == 0 ? pixel.r : (plane == 1 ? pixel.g : pixel.b);
	return float4(value, 0.0, 0.0, 1.0);
}

float4 PSAlphaMaskRGBAWithoutBlur(VertDataOut v_in) : TARGET
{
	float4 inputRGBA = image.Sample(textureSampler, v_in.uv);
	inputRGBA.rgb = max(float3(0.0, 0.0, 0.0), inputRGBA.rgb / inputRGBA.a);

	float4 outputRGBA;
	float a = (1.0 - image1.Sample(textureSampler, v_in.uv).r) * inputRGBA.a;
	outputRGBA.rgb = inputRGBA.rgb * a;
	outputRGBA.a = a;
	return outputRGBA;
}

float4 PSKawaseFocalBlur(VertDataOut v_in) : TARGET
{
	float blurIterF = float(blurIter) / float(blurTotal);

	float blurValue = image1.Sample(textureSampler, v_in.uv).r;
	blurValue += image1.Sample(textureSampler, v_in.uv + float2(0.01, 0.01)).r;
	blurValue += image1.Sample(textureSampler, v_in.uv + float2(-0.01, 0.01)).r;
	blurValue += image1.Sample(textureSampler, v_in.uv + float2(0.01, -0.01)).r;
	blurValue += image1.Sample(textureSampler, v_in.uv + float2(-0.01, -0.01)).r;
	blurValue *= 0.2;

	float blurFocusDistance = clamp(abs(blurValue - blurFocusPoint), 0.0, 1.0);
	float blurFocusFactor = clamp(blurFocusDistance - blurFocusDepth, 0.0, 1.0);

	if (blurIterF > blurFocusFactor) {
		return image.Sample(textureSampler, v_in.uv);
	}

	float4 sum = float4(0.0, 0.0, 0.0, 0.0);
	sum += image.Sample(textureSampler, v_in.uv + float2(xOffset, yOffset));
	sum += image.Sample(textureSampler, v_in.uv + float2(-xOffset, yOffset));
	sum += image.Sample(textureSampler, v_in.uv + float2(xOffset, -yOffset));
	sum += image.Sample(textureSampler, v_in.uv + float2(-xOffset, -yOffset));
	sum *= 0.25;
	return sum;
}

technique DrawWithFocalBlur
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSTakeBlur(v_in);
	}
}

technique DrawWithoutBlur
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSAlphaMaskRGBAWithoutBlur(v_in);
	}
}

technique DrawFocalBlur
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSKawaseFocalBlur(v_in);
	}
}

technique Draw
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSDraw(v_in);
	}
}

technique ExtractScaledRGBPlanes
{
	pass
	{
		vertex_shader = VSDefault(v_in);
		pixel_shader = PSExtractScaledRGBPlanes(v_in);
	}
}
)EFFECT";

} // namespace BackgroundRemoval::MPLDDetail
