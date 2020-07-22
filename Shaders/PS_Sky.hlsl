#include "HLSLCommon.hlsli"

// Shader layout define and include
#define ConstantDataType ObjectConstants
#define RenderPassDataType RenderPassConstants
#include "ShaderLayout.hlsli"

struct OutputVertex
{
	float4 positionH : SV_POSITION;
	float3 textureCoord : TEXCOORD;
};
float4 main(OutputVertex inputPixel) : SV_TARGET
{
	TextureCube cubemap = TexCubeTable[RenderPassDataCB.IrradianceCubemapIndex];
	float3 color = cubemap.Sample(SamplerLinearWrap, inputPixel.textureCoord).xyz;
	color *= RenderPassDataCB.Sun.Intensity * 0.5f;
	return float4(color, 1.0f);
}