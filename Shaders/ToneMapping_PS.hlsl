#include "HLSLCommon.hlsli"
#include "StaticSamplers.hlsli"

cbuffer cbSettings : register(b0)
{
	float Exposure;
	float Gamma;
}

Texture2D InputMap : register(t0);

// From http://filmicworlds.com/blog/filmic-tonemapping-operators/
float3 Uncharted2Tonemap(float3 color)
{
	float A = 0.15f;
	float B = 0.50f;
	float C = 0.10f;
	float D = 0.20f;
	float E = 0.02f;
	float F = 0.30f;
	float W = 11.2f;
	return ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
}

float4 tonemap(float4 color)
{
	float3 outcol = Uncharted2Tonemap(color.rgb * Exposure);
	outcol = outcol * (1.0f / Uncharted2Tonemap(float3(11.2f, 11.2f, 11.2f)));
	float invGamma = 1.0f / Gamma;
	return float4(pow(outcol, float3(invGamma, invGamma, invGamma)), color.a);
}

#define MANUAL_SRGB 1

float4 SRGBtoLINEAR(float4 srgbIn)
{
#ifdef MANUAL_SRGB
#ifdef SRGB_FAST_APPROXIMATION
	float3 linOut = pow(srgbIn.xyz,float3(2.2));
#else //SRGB_FAST_APPROXIMATION
	float3 bLess = step(float3(0.04045f, 0.04045f, 0.04045f), srgbIn.xyz);
	float3 linOut = lerp(srgbIn.xyz / float3(12.92f, 12.92f, 12.92f), pow((srgbIn.xyz + float3(0.055f, 0.055f, 0.055f)) / float3(1.055f, 1.055f, 1.055f), float3(2.4f, 2.4f, 2.4f)), bLess);
#endif //SRGB_FAST_APPROXIMATION
	return float4(linOut, srgbIn.w);;
#else //MANUAL_SRGB
	return srgbIn;
#endif //MANUAL_SRGB
}

struct VSInput
{
	float4 Position : SV_Position;
	float2 Texture : TEXCOORD0;
};
float4 main(VSInput pixel) : SV_TARGET
{
	float4 sampledInputMapPixel = InputMap.SampleLevel(s_SamplerLinearClamp, pixel.Texture, 0.0f);
	
	return SRGBtoLINEAR(tonemap(sampledInputMapPixel));
}