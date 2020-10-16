cbuffer Settings : register(b0)
{
	unsigned int AccumulationCount;
};

struct AccumulationData
{
	uint InputIndex;
	uint OutputIndex;
};

// Shader layout define and include
#define RenderPassDataType AccumulationData
#include "../ShaderLayout.hlsli"

[numthreads(16, 16, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	Texture2D Input = Texture2DTable[RenderPassData.InputIndex];
	RWTexture2D<float4> Output = RWTexture2DTable[RenderPassData.OutputIndex];

	float4 current = Input[DTid.xy]; // Current path tracer result
	float4 prev = Output[DTid.xy]; // Previous path tracer output
	
	Output[DTid.xy] = lerp(prev, current, 1.0f / float(AccumulationCount + 1)); // accumulate
}