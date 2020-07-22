#include "HLSLCommon.hlsli"

// Shader layout define and include
#define ConstantDataType ObjectConstants
#define RenderPassDataType RenderPassConstants
#include "ShaderLayout.hlsli"

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
	float3 textureCoord : TEXCOORD;
};
OutputVertex main(InputVertex inputVertex)
{
	OutputVertex output;
	
	// Transform to Clip/Homogenized space
	// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
	output.positionH.xyz = inputVertex.positionL;
	output.positionH = mul(float4(inputVertex.positionL, 0.0f), RenderPassDataCB.ViewProjection).xyww;
	output.textureCoord = inputVertex.positionL;
	return output;
}