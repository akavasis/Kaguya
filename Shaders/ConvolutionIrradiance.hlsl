#include "HLSLCommon.hlsli"

static float DeltaPhi	= s_2PI / 180.0f;
static float DeltaTheta = s_PIDIV2 / 64.0f;

cbuffer Settings : register(b0)
{
	int CubemapIndex;
}

SamplerState SamplerLinearClamp : register(s0);

#include "Skybox.hlsli"

float4 PSMain(VSOutput IN) : SV_TARGET
{
	TextureCube Cubemap = TextureCubeTable[CubemapIndex];

	float3 normal = normalize(IN.TextureCoord);
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
			irradiance += Cubemap.Sample(SamplerLinearClamp, sampleVector).rgb * cos(theta) * sin(theta);
			numSamples++;
		}
	}
	return float4((s_PI * irradiance) / float(numSamples), 1.0f);
}