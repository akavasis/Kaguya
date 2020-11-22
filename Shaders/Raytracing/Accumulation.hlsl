struct RenderPassData
{
	uint AccumulationCount;
	
	uint Input;
	uint RenderTarget;
};
#define RenderPassDataType RenderPassData
#include <ShaderLayout.hlsli>

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	Texture2D			Input	= g_Texture2DTable[g_RenderPassData.Input];
	RWTexture2D<float4> RenderTarget = g_RWTexture2DTable[g_RenderPassData.RenderTarget];

	float4 current = Input[DTid.xy]; // Current path tracer result
	float4 prev = RenderTarget[DTid.xy]; // Previous path tracer output

	RenderTarget[DTid.xy] = lerp(prev, current, 1.0f / float(g_RenderPassData.AccumulationCount + 1)); // accumulate
}