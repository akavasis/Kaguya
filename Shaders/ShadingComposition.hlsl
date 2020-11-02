struct ShadingCompositionData
{
	int AnalyticUnshadowed;
	int StochasticUnshadowed;
	int StochasticShadowed;
	
	int RenderTarget;
};
#define RenderPassDataType ShadingCompositionData
#include "ShaderLayout.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	Texture2D			AnalyticUnshadowed		= g_Texture2DTable[g_RenderPassData.AnalyticUnshadowed];
	Texture2D			StochasticUnshadowed	= g_Texture2DTable[g_RenderPassData.StochasticUnshadowed];
	Texture2D			StochasticShadowed		= g_Texture2DTable[g_RenderPassData.StochasticShadowed];
	RWTexture2D<float4> RenderTarget			= g_RWTexture2DTable[g_RenderPassData.RenderTarget];
	
	float3				analytic				= AnalyticUnshadowed[DTid.xy].rgb;
	float3				stochasticUnshadowed	= StochasticUnshadowed[DTid.xy].rgb;
	float3				stochasticShadowed		= StochasticShadowed[DTid.xy].rgb;
	
	float3				shadow					= 0.0.xxx;

    [unroll] 
	for (int i = 0; i < 3; ++i)
	{
		shadow[i] = stochasticUnshadowed[i] < 1e-05 ? 1.0 : stochasticShadowed[i] / stochasticUnshadowed[i];
	}

	RenderTarget[DTid.xy] = float4(analytic * shadow, 1.0f);
}