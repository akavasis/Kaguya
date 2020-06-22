#include "StaticSamplers.hlsli"

struct Setting
{
	float rx;
	float ry;
	float rz;
	float gx;
	float gy;
	float gz;
	float bx;
	float by;
	float bz;
};

ConstantBuffer<Setting> SettingConstantsGPU : register(b0);

Texture2D InputMap : register(t0);

struct VSInput
{
	float4 Position : SV_Position;
	float2 Texture : TEXCOORD0;
};
float4 main(VSInput pixel) : SV_TARGET
{
	float4 sampledInputMapPixel = InputMap.SampleLevel(s_SamplerLinearClamp, pixel.Texture, 0.0f);
	float4 color;
	color.r = dot(sampledInputMapPixel.rgb, float3(SettingConstantsGPU.rx, SettingConstantsGPU.ry, SettingConstantsGPU.rz));
	color.g = dot(sampledInputMapPixel.rgb, float3(SettingConstantsGPU.gx, SettingConstantsGPU.gy, SettingConstantsGPU.gz));
	color.b = dot(sampledInputMapPixel.rgb, float3(SettingConstantsGPU.bx, SettingConstantsGPU.by, SettingConstantsGPU.bz));
	color.a = sampledInputMapPixel.a;
	return color;
}