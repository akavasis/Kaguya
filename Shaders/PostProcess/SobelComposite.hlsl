#include "../StaticSamplers.hlsli"

Texture2D InputMap : register(t0);
Texture2D SobelEdgeMap : register(t1);

struct VSInput
{
	float4 Position : SV_Position;
	float2 Texture : TEXCOORD0;
};

float4 main(VSInput pixel) : SV_TARGET
{
	float4 sampledInputMapPixel = InputMap.SampleLevel(s_SamplerPointClamp, pixel.Texture, 0.0f);
	float4 sampledSobelEdgeMapPixel = SobelEdgeMap.SampleLevel(s_SamplerPointClamp, pixel.Texture, 0.0f);
	return sampledInputMapPixel * sampledSobelEdgeMapPixel;
}