#include "StaticSamplers.hlsli"
#include "DescriptorTables.hlsli"

#include "HLSLCommon.hlsli"

ConstantBuffer<RenderPassConstants> RenderPassConstantsGPU : register(b0);

TextureCube CubeMap : register(t0);

struct OutputVertex
{
	float4 positionH : SV_POSITION;
	float3 textureCoord : TEXCOORD;
};
float4 main(OutputVertex inputPixel) : SV_TARGET
{
	float3 color = CubeMap.Sample(s_SamplerLinearWrap, inputPixel.textureCoord).xyz;
	color *= RenderPassConstantsGPU.Sun.Intensity * 0.5f;
	return float4(color, 1.0f);
}