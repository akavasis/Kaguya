#ifndef __GBUFFER_HLSLI__
#define __GBUFFER_HLSLI__

#include "HLSLCommon.hlsli"

static const uint GBufferTypeMesh = 0;
static const uint GBufferTypeLight = 1;

struct GBuffer
{
	Texture2D			Position;
	Texture2D			Normal;
	Texture2D			Albedo;
	Texture2D<uint4>	TypeAndIndex;
	Texture2D			DepthStencil;
};

struct GBufferMesh
{
	float3	Position;
	float3	Normal;
	float3	Albedo;
	uint	MaterialIndex;
};

struct GBufferLight
{
	uint LightIndex;
};

struct PSOutput
{
	float4	Position		: SV_TARGET0;
	float4	Normal			: SV_TARGET1;
	float4	Albedo			: SV_TARGET2;
	uint	TypeAndIndex	: SV_TARGET3; // If Type is Mesh, Index is MaterialIndex, If Type if Light, Index is LightIndex
};

float3 EncodeNormal(float3 n)
{
	return n * 0.5f + 0.5f;
}

float3 DecodeNormal(float3 n)
{
	return n * 2.0f - 1.0f;
}

PSOutput GetGBufferMeshPSOutput(GBufferMesh mesh)
{
	PSOutput OUT;
	{
		OUT.Position		= float4(mesh.Position, 1.0f);
		OUT.Normal			= float4(EncodeNormal(mesh.Normal), 0.0f);
		OUT.Albedo			= float4(mesh.Albedo, 0.0f);
		OUT.TypeAndIndex	= (GBufferTypeMesh << 4) | (mesh.MaterialIndex & 0x0000000F);
	}
	return OUT;
}

PSOutput GetGBufferLightPSOutput(GBufferLight light)
{
	PSOutput OUT;
	{
		OUT.Position		= (float4) 0.0f;
		OUT.Normal			= (float4) 0.0f;
		OUT.Albedo			= (float4) 0.0f;
		OUT.TypeAndIndex	= (GBufferTypeLight << 4) | (light.LightIndex & 0x0000000F);
	}
	return OUT;
}

uint GetGBufferType(GBuffer gbuffer, uint2 uv)
{
	uint type = gbuffer.TypeAndIndex.Load(uint3(uv, 0)).x;
	return type >> 4;
}

GBufferMesh GetGBufferMesh(GBuffer gbuffer, uint2 uv)
{
	GBufferMesh OUT;
	{
		uint3 location = uint3(uv, 0);
		
		OUT.Position = gbuffer.Position.Load(location).xyz;
		OUT.Normal = DecodeNormal(gbuffer.Normal.Load(location).xyz);
		OUT.Albedo = gbuffer.Albedo.Load(location).rgb;
		OUT.MaterialIndex = gbuffer.TypeAndIndex.Load(location).x & 0x0000000F;
	}
	return OUT;
}

GBufferLight GetGBufferLight(GBuffer gbuffer, uint2 uv)
{
	GBufferLight OUT;
	{
		uint3 location = uint3(uv, 0);
		
		OUT.LightIndex = gbuffer.TypeAndIndex.Load(location).x & 0x0000000F;
	}
	return OUT;
}

#endif