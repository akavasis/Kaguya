#include <ShaderLayout.hlsli>

#include "GranTurismoOperator.hlsli"

// ACES tone mapping curve fit to go from HDR to LDR
//https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 ACESFilm(float3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0f, 1.0f);
}

float3 LinearTosRGB(float3 u)
{
	return u <= 0.0031308 ? 12.92 * u : 1.055 * pow(u, 1.0 / 2.4) - 0.055;
}

float3 sRGBToLinear(float3 u)
{
	return u <= 0.04045 ? u / 12.92 : pow((u + 0.055) / 1.055, 2.4);
}

cbuffer Settings : register(b0)
{
	uint InputIndex;
	
	float MaximumBrightness;
	float Contrast;
	float LinearSectionStart;
	float LinearSectionLength;
	float BlackTightness_C;
	float BlackTightness_B;
};

// Include Quad VS entry point
#include <Quad.hlsl>

float4 PSMain(VSOutput IN) : SV_TARGET
{
	Texture2D Input = g_Texture2DTable[InputIndex];
	
	uint2 pixel = uint2(IN.Position.xy + 0.5f);
	
	float3 color = Input[pixel].rgb;
	
	GranTurismoOperator GTOperator;
	GTOperator.MaximumBrightness	= MaximumBrightness;
	GTOperator.Contrast				= Contrast;
	GTOperator.LinearSectionStart	= LinearSectionStart;
	GTOperator.LinearSectionLength	= LinearSectionLength;
	GTOperator.BlackTightness_C		= BlackTightness_C;
	GTOperator.BlackTightness_B		= BlackTightness_B;
		
	color = ApplyGranTurismoOperator(color, GTOperator);
	color = LinearTosRGB(color);
	return float4(color, 1.0f);
}