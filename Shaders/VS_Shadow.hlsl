#include "HLSLCommon.hlsli"

ConstantBuffer<ObjectConstants> ObjectConstantsGPU : register(b0);
ConstantBuffer<RenderPassConstants> RenderPassConstantsGPU : register(b1);

struct InputVertex
{
	float3 positionL : POSITION;
	float2 textureCoord : TEXCOORD;
	float3 normalL : NORMAL;
	float3 tangentL : TANGENT;
};
struct OutputVertex
{
	float4 positionH : SV_POSITION;
	float3 positionW : POSITION;
};
OutputVertex main(InputVertex inputVertex)
{
	OutputVertex output;
	// Transform to world space
	output.positionW = mul(float4(inputVertex.positionL, 1.0f), ObjectConstantsGPU.World).xyz;
	// Transform to homogeneous/clip space
	output.positionH = mul(float4(output.positionW, 1.0f), RenderPassConstantsGPU.ViewProjection);
	
	return output;
}