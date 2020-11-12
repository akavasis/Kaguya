cbuffer RootConstants : register(b0)
{
	uint Source0Index; // S_N
	uint Source1Index; // U_N
	uint RenderTargetIndex;
};
#include "../ShaderLayout.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	Texture2D Source0 = g_Texture2DTable[Source0Index];
	Texture2D Source1 = g_Texture2DTable[Source1Index];
	RWTexture2D<float4> RenderTarget = g_RWTexture2DTable[RenderTargetIndex];
    
	float3 S_N = Source0[DTid.xy].rgb;
	float3 U_N = Source1[DTid.xy].rgb;
	
	float3 resultRatio;
	for (int i = 0; i < 3; ++i)
	{
        // Threshold degenerate values
		resultRatio[i] = (U_N[i] < 0.0001f) ? 1.0f : (S_N[i] / U_N[i]);
	}
	
	RenderTarget[DTid.xy] = float4(resultRatio, 1.0f);
}