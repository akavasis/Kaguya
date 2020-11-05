#ifndef GBUFFER_HLSLI
#define GBUFFER_HLSLI

#include "HLSLCommon.hlsli"

static const uint GBufferTypeMesh	= 1 << 0;
static const uint GBufferTypeLight	= 1 << 1;

struct GBuffer
{
	Texture2D			Albedo;
	Texture2D			Normal;
	Texture2D<uint4>	TypeAndIndex;
	Texture2D			Depth;
};

struct GBufferMesh
{
	float3	Albedo;
	float3	Normal;
	float	Roughness;
	float	Metallic;
	uint	MaterialIndex;
};

struct GBufferLight
{
	uint LightIndex;
};

// Multiple Render Targets
struct MRT
{
	float4	Albedo				: SV_TARGET0;
	float4	Normal				: SV_TARGET1;
	uint	TypeAndIndex		: SV_TARGET2; // If Type is Mesh, Index is MaterialIndex, If Type if Light, Index is LightIndex
	float4	SVGF_LinearZ		: SV_TARGET3;
	float4	SVGF_MotionVector	: SV_TARGET4;
	float4	SVGF_Compact		: SV_TARGET5;
};

float3 EncodeNormal(float3 n)
{
	return n * 0.5f + 0.5f;
}

float3 DecodeNormal(float3 n)
{
	return n * 2.0f - 1.0f;
}

uint GetGBufferType(GBuffer gbuffer, uint2 uv)
{
	uint type = gbuffer.TypeAndIndex[uv].x;
	return type >> 4;
}

GBufferMesh GetGBufferMesh(GBuffer gbuffer, uint2 uv)
{
	GBufferMesh OUT;
	{
		float4	NormalRoughness	= gbuffer.Normal[uv];
		float4	AlbedoMetallic	= gbuffer.Albedo[uv];
		
		OUT.Normal				= DecodeNormal(NormalRoughness.xyz);
		OUT.Albedo				= AlbedoMetallic.rgb;
		OUT.Roughness			= NormalRoughness.a;
		OUT.Metallic			= AlbedoMetallic.a;
		OUT.MaterialIndex		= gbuffer.TypeAndIndex[uv].x & 0x0000000F;
	}
	return OUT;
}

GBufferLight GetGBufferLight(GBuffer gbuffer, uint2 uv)
{
	GBufferLight OUT;
	{
		OUT.LightIndex = gbuffer.TypeAndIndex[uv].x & 0x0000000F;
	}
	return OUT;
}

#endif