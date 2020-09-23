cbuffer Settings : register(b0)
{
	float2 InverseOutputSize;
	float Intensity;
};

Texture2D Input : register(t0);
Texture2D Bloom : register(t1);
RWTexture2D<float4> Output : register(u0);

SamplerState LinearSampler : register(s0);

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
	float2 UV = float2(DTid.xy + 0.5f) * InverseOutputSize;
	
	Output[DTid.xy] = Input[DTid.xy] + Intensity * Bloom.SampleLevel(LinearSampler, UV, 0.0f);
}