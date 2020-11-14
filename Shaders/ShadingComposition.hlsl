struct ShadingCompositionData
{
	// Input Textures
	uint AnalyticUnshadowed;	// U
	uint Source0Index;			// S_N
	uint Source1Index;			// U_N

	// Output Textures
	uint RenderTarget;
};
#define RenderPassDataType ShadingCompositionData
#include "ShaderLayout.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 Gid : SV_GroupID, uint3 GTid : SV_GroupThreadID, uint3 DTid : SV_DispatchThreadID)
{
	Texture2D			AnalyticUnshadowed	= g_Texture2DTable[g_RenderPassData.AnalyticUnshadowed];
	Texture2D			Source0				= g_Texture2DTable[g_RenderPassData.Source0Index];
	Texture2D			Source1				= g_Texture2DTable[g_RenderPassData.Source1Index];
	RWTexture2D<float4> RenderTarget		= g_RWTexture2DTable[g_RenderPassData.RenderTarget];
	
	float3				analytic			= AnalyticUnshadowed[DTid.xy].rgb;
	float3				S_N					= Source0[DTid.xy].rgb;
	float3				U_N					= Source1[DTid.xy].rgb;
	
	float3 weightedShadow;
	for (int i = 0; i < 3; ++i)
	{
        // Threshold degenerate values
		weightedShadow[i] = (U_N[i] < 0.0001f) ? 1.0f : (S_N[i] / U_N[i]);
	}
	
	RenderTarget[DTid.xy] = float4(analytic * weightedShadow, 1.0f);
}