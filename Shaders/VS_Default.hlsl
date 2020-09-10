#include "HLSLCommon.hlsli"

#define ConstantDataType ObjectConstants
#define RenderPassDataType RenderPassConstants

#include "ShaderLayout.hlsli"

struct InputVertex
{
	float3 positionL : POSITION;
	float2 textureCoord : TEXCOORD;
	float3 normalL : NORMAL;
	float3 tangentL : TANGENT;
	float3 bitangentL : BITANGENT;
};
struct OutputVertex
{
	float4 positionH : SV_POSITION;
	float3 positionW : POSITION;
	float2 textureCoord : TEXCOORD;
	float3 normalW : NORMAL;
	float3 tangentW : TANGENT;
	float3 bitangentW : BITANGENT;
	float3x3 tbnMatrix : TBNBASIS;
	float depth : DEPTH;
};
OutputVertex main(InputVertex inputVertex)
{
	OutputVertex output;
	
	// Transform to world space
	output.positionW = mul(float4(inputVertex.positionL, 1.0f), ConstantDataCB.World).xyz;
	// Transform to homogeneous/clip space
	output.positionH = mul(float4(output.positionW, 1.0f), RenderPassDataCB.ViewProjection);
	
#if !RENDER_SHADOWS
	output.textureCoord = inputVertex.textureCoord;
	// Transform normal and tangent to world space
	// Build orthonormal basis.
	float3 N = normalize(mul(inputVertex.normalL, (float3x3) ConstantDataCB.World));
	float3 T = normalize(mul(inputVertex.tangentL, (float3x3) ConstantDataCB.World));
	float3 B = normalize(mul(inputVertex.bitangentL, (float3x3) ConstantDataCB.World));
	
	output.normalW = N;
	output.tangentW = T;
	output.bitangentW = B;
	output.tbnMatrix = float3x3(T, B, N);
	output.depth = output.positionH.w;
#endif
	
	return output;
}