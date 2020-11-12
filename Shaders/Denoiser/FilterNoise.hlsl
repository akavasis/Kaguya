cbuffer RootConstants : register(b0)
{
	uint SourceIndex;
	uint RenderTargetIndex;
};
#include "../ShaderLayout.hlsli"

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
	Texture2D Source = g_Texture2DTable[SourceIndex];
	
	float result = 0.0f;
	const int R = 1;
	for (int2 delta = int2(-R, -R); delta.y <= R; ++delta.y)
	{
		for (delta.x = -R; delta.x <= R; ++delta.x)
		{
			result += Source[DTid.xy + delta].r;
		}
	}
	float x = 2.0f * float(R) + 1.0f;
	result *= (1.0f / (x * x));
	
	RWTexture2D<float4> RenderTarget = g_RWTexture2DTable[RenderTargetIndex];
	RenderTarget[DTid.xy] = result.xxxx;
}