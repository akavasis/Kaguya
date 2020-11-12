struct ShadingCompositionData
{
	int AnalyticUnshadowed; // U
	int WeightedShadow;		// W_N

	int RenderTarget;
};
#define RenderPassDataType ShadingCompositionData
#include "ShaderLayout.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	Texture2D			AnalyticUnshadowed	= g_Texture2DTable[g_RenderPassData.AnalyticUnshadowed];
	Texture2D			WeightedShadow		= g_Texture2DTable[g_RenderPassData.WeightedShadow];
	RWTexture2D<float4> RenderTarget		= g_RWTexture2DTable[g_RenderPassData.RenderTarget];
	
	float3				analytic			= AnalyticUnshadowed[DTid.xy].rgb;
	float3				weightedShadow		= WeightedShadow[DTid.xy].rgb;
	
	RenderTarget[DTid.xy] = float4(analytic * weightedShadow, 1.0f);
}