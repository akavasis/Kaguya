#include "HLSLCommon.hlsli"

// Radical inverse based on http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html
float RadicalInverse_VDC(uint Bits)
{
	Bits = (Bits << 16u) | (Bits >> 16u);
	Bits = ((Bits & 0x55555555u) << 1u) | ((Bits & 0xAAAAAAAAu) >> 1u);
	Bits = ((Bits & 0x33333333u) << 2u) | ((Bits & 0xCCCCCCCCu) >> 2u);
	Bits = ((Bits & 0x0F0F0F0Fu) << 4u) | ((Bits & 0xF0F0F0F0u) >> 4u);
	Bits = ((Bits & 0x00FF00FFu) << 8u) | ((Bits & 0xFF00FF00u) >> 8u);
	return float(Bits) * 2.3283064365386963e-10; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
	return float2(float(i) / float(N), RadicalInverse_VDC(i));
}

// Based on http://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_slides.pdf
float3 ImportanceSampleGGX(float2 Xi, float3 N, float Roughness)
{
	float a = Roughness * Roughness;
	
	float phi = 2.0 * s_PI * Xi.x;
	float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
	float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
	
    // from spherical coordinates to cartesian coordinates
	float3 H = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
	
    // from tangent-space vector to world-space sample vector
	float3 up = abs(N.z) < 0.999 ? float3(0.0, 0.0, 1.0) : float3(1.0, 0.0, 0.0);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = normalize(cross(N, tangent));
	
	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float NDF_TrowbridgeReitzGGX(in float cosLh, in float Roughness)
{
	float alpha = Roughness * Roughness;
	float alphaSq = alpha * alpha;
	
	float denominator = (cosLh * cosLh) * (alphaSq - 1.0f) + 1.0f;
	return alphaSq / (s_PI * denominator * denominator);
}

static uint NumSamples = 32;

cbuffer Settings : register(b0)
{
	float Roughness;
	int CubemapIndex;
}

SamplerState SamplerLinearClamp : register(s0);

#include "Skybox.hlsl"

float4 PSMain(VSOutput IN) : SV_TARGET
{
	TextureCube Cubemap = TextureCubeTable[CubemapIndex];

	float3 N = normalize(IN.TextureCoord);
	float3 R = N;
	float3 V = R;
	float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);
	float totalWeight = 0.0f;
	
	uint width, height, numMips;
	Cubemap.GetDimensions(0, width, height, numMips);
	float dimension = float(width);
	for (uint i = 0u; i < NumSamples; i++)
	{
		float2 Xi = Hammersley(i, NumSamples);
		float3 H = ImportanceSampleGGX(Xi, N, Roughness);
		float3 L = normalize(2.0f * dot(V, H) * H - V);
		float NdotL = max(0.0f, dot(N, L));
		if (NdotL > 0.0f)
		{
			// Filtering based on https://placeholderart.wordpress.com/2015/07/28/implementation-notes-runtime-environment-map-filtering-for-image-based-lighting/
			float dotNH = saturate(dot(N, H));
			float dotVH = saturate(dot(V, H));

			// Probability Distribution Function
			float pdf = NDF_TrowbridgeReitzGGX(dotNH, Roughness) * dotNH / (4.0f * dotVH) + 0.0001;
			// Solid angle of current smple
			float omegaS = 1.0 / (float(NumSamples) * pdf);
			// Solid angle of 1 pixel across all cube faces
			float omegaP = 4.0 * s_PI / (6.0f * dimension * dimension);
			// Biased (+1.0) mip level for better result
			float mipLevel = Roughness == 0.0 ? 0.0 : max(0.5 * log2(omegaS / omegaP) + 1.0, 0.0f);
			prefilteredColor += Cubemap.SampleLevel(SamplerLinearClamp, L, mipLevel).rgb * NdotL;
			totalWeight += NdotL;
		}
	}
	return float4(prefilteredColor / totalWeight, 1.0);
}