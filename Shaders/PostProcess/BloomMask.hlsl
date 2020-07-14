#include "../HLSLCommon.hlsli"
#include "../StaticSamplers.hlsli"

Texture2D InputMap : register(t0);

struct VSInput
{
	float4 Position : SV_Position;
	float2 Texture : TEXCOORD0;
};
float4 main(VSInput pixel) : SV_TARGET
{
	float4 sceneColor = InputMap.Sample(s_SamplerLinearClamp, pixel.Texture);
	// clamp to avoid artifacts from exceeding fp16 through backbuffer blending of multiple very bright lights
	sceneColor.rgb = min(float3(256 * 256, 256 * 256, 256 * 256), sceneColor.rgb);
	
	half3 linearColor = sceneColor.rgb;
	half luminance = Luminance(linearColor);

	half bloomAmount = saturate(luminance * 0.5);
	return float4(bloomAmount * linearColor, sceneColor.a);
}