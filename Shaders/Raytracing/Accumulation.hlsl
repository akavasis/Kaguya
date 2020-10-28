cbuffer Settings : register(b0)
{
	unsigned int AccumulationCount;
};

struct AccumulationData
{
	uint InputIndex;
	uint RenderTarget;
};
#define RenderPassDataType AccumulationData
#include "../ShaderLayout.hlsli"

[numthreads(16, 16, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	Texture2D Input = g_Texture2DTable[g_RenderPassData.InputIndex];
	RWTexture2D<float4> Output = g_RWTexture2DTable[g_RenderPassData.RenderTarget];

	float4 current = Input[DTid.xy]; // Current path tracer result
	float4 prev = Output[DTid.xy]; // Previous path tracer output
	
	Output[DTid.xy] = lerp(prev, current, 1.0f / float(AccumulationCount + 1)); // accumulate
}