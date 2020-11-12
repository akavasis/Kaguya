#ifndef SHADER_LAYOUT_HLSLI
#define SHADER_LAYOUT_HLSLI

/*
	Defines bindless resource layout, contains 5 total root parameters,
	Resources are grouped together based on their usage [Read/Write] within
	their respected descriptor tables
*/

#include "Constants.hlsli"
#include "SharedTypes.hlsli"

struct SystemConstants
{
	Camera Camera, PreviousCamera;
	
	float4 OutputSize;

	uint TotalFrameCount;

	int NumPolygonalLights;
};

struct DummyStructure
{
	int Dummy;
};

#ifndef RenderPassDataType
#define RenderPassDataType DummyStructure
#endif

ConstantBuffer<SystemConstants>			g_SystemConstants			: register(b0, space100);
ConstantBuffer<RenderPassDataType>		g_RenderPassData			: register(b1, space100);

// ShaderResource
Texture2D								g_Texture2DTable[]			: register(t0, space100);
Texture2D<uint4>						g_Texture2DUINT4Table[]		: register(t0, space101);
Texture2DArray							g_Texture2DArrayTable[]		: register(t0, space102);
TextureCube								g_TextureCubeTable[]		: register(t0, space103);
ByteAddressBuffer						g_ByteAddressBufferTable[]	: register(t0, space104);

// UnorderedAccess
RWTexture2D<float4>						g_RWTexture2DTable[]		: register(u0, space100);
RWTexture2DArray<float4>				g_RWTexture2DArrayTable[]	: register(u0, space101);

// Sampler
SamplerState							g_SamplerTable[]			: register(s0, space100);

#endif // SHADER_LAYOUT_HLSLI