cbuffer AccumulationSetting : register(b0)
{
	unsigned int AccumulationCount;
};

RWTexture2D<float4> Input : register(u0);
RWTexture2D<float4> Output : register(u1);

[numthreads(16, 16, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	float4 current = Input[DTid.xy]; // Current path tracer result
	float4 prev = Output[DTid.xy]; // Previous path tracer output
	float4 color = lerp(prev, current, 1.0f / float(AccumulationCount + 1)); // accumulate
	
	Output[DTid.xy] = color;
}