#include "HLSLCommon.hlsli"

ConstantBuffer<Object> ObjectConstantsGPU : register(b0);
ConstantBuffer<Material> MaterialConstantsGPU : register(b1);
ConstantBuffer<RenderPass> RenderPassConstantsGPU : register(b2);

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
	float2 textureCoord : TEXCOORD;
	float3 normalW : NORMAL;
	float3x3 tbnMatrix : TBNBASIS;
	float depth : DEPTH;
};
OutputVertex main(InputVertex inputVertex)
{
	OutputVertex output;
	
	// Build orthonormal basis.
	float3 N = normalize(mul(inputVertex.normalL, (float3x3) ObjectConstantsGPU.World));
	float3 T = normalize(mul(inputVertex.tangentL, (float3x3) ObjectConstantsGPU.World));
	float3 B = normalize(cross(N, T));
	
	// Transform to world space
	output.positionW = mul(float4(inputVertex.positionL, 1.0f), ObjectConstantsGPU.World).xyz;
	// Transform to homogeneous/clip space
	output.positionH = mul(float4(output.positionW, 1.0f), RenderPassConstantsGPU.ViewProjection);
	output.textureCoord = inputVertex.textureCoord;
	// Transform normal and tangent to world space
	output.normalW = N;
	output.tbnMatrix = float3x3(T, B, N);
	output.depth = output.positionH.w;
	
	return output;
}