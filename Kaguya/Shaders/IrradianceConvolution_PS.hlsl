#include "HLSLCommon.hlsli"

ConstantBuffer<RenderPass> RenderPassConstantsGPU : register(b0);
cbuffer Settings : register(b1)
{
	float DeltaPhi;
	float DeltaTheta;
}

TextureCube CubeMap : register(t0);
SamplerState s_SamplerLinearClamp : register(s0);

struct OutputVertex
{
	float4 positionH : SV_POSITION;
	float3 textureCoord : TEXCOORD;
};
float4 main(OutputVertex inputPixel) : SV_TARGET
{
	float3 normal = normalize(inputPixel.textureCoord);
	float3 up = float3(0.0f, 1.0f, 0.0f);
	float3 right = normalize(cross(up, normal));
	up = normalize(cross(normal, right));
	
	float3 irradiance = float3(0.0f, 0.0f, 0.0f);
	uint numSamples = 0u;
	for (float phi = 0.0f; phi < s_2PI; phi += DeltaPhi)
	{
		for (float theta = 0.0f; theta < s_PIDIV2; theta += DeltaTheta)
		{
			float3 tempVec = cos(phi) * right + sin(phi) * up;
			float3 sampleVector = cos(theta) * normal + sin(theta) * tempVec;
			irradiance += CubeMap.Sample(s_SamplerLinearClamp, sampleVector).rgb * cos(theta) * sin(theta);
			numSamples++;
		}
	}
	return float4((s_PI * irradiance) / float(numSamples), 1.0f);
}