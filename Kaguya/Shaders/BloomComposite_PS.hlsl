#include "StaticSamplers.hlsli"

struct Setting
{
	float Exposure;
	float Intensity;
};

ConstantBuffer<Setting> SettingConstantsGPU : register(b0);

Texture2D InputMap : register(t0);
Texture2D BloomMaskMap : register(t1);

struct VSInput
{
	float4 Position : SV_Position;
	float2 Texture : TEXCOORD0;
};

float4 main(VSInput pixel) : SV_TARGET
{
	float4 sampledInputMapPixel = InputMap.Sample(s_SamplerPointClamp, pixel.Texture);
	float3 sampledBloomMaskMapPixel = BloomMaskMap.Sample(s_SamplerLinearClamp, pixel.Texture) * SettingConstantsGPU.Intensity * exp(SettingConstantsGPU.Exposure);
	return float4(sampledInputMapPixel.rgb + sampledBloomMaskMapPixel.rgb, sampledInputMapPixel.a);
}