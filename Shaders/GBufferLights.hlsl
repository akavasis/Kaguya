#include "HLSLCommon.hlsli"

// This creates a quad facing the positive z
void GetLightVertexWS(PolygonalLight light, uint vertexId, out float3 worldSpaceCoord, out float2 localSpaceCoord)
{
	float halfWidth = light.Width * 0.5f;
	float halfHeight = light.Height * 0.5f;

    // Get billboard points at the origin
	float dx = (vertexId == 0 || vertexId == 1) ? halfWidth : -halfWidth;
	float dy = (vertexId == 0 || vertexId == 3) ? -halfHeight : halfHeight;

	localSpaceCoord = float2(dx, dy);
	float3 lightPoint = float3(localSpaceCoord, 0.0f);

    // Rotate around origin
	lightPoint = mul(lightPoint, (float3x3) light.World);

    // Move points to light's location
	worldSpaceCoord = lightPoint + float3(light.World[3][0], light.World[3][1], light.World[3][2]);
}

//----------------------------------------------------------------------------------------------------
cbuffer RootConstants
{
	uint LightID;
};

StructuredBuffer<PolygonalLight> Lights : register(t0, space0);

struct GBufferData
{
	GlobalConstants GlobalConstants;
};

#define RenderPassDataType GBufferData
#include "ShaderLayout.hlsli"

struct VSOutput
{
	float4		PositionH		: SV_POSITION;
	float3		PositionW		: POSITION;
	float		Depth			: DEPTH;
	float2		Dimension		: DIMENSION;
};

struct PSOutput
{
	float4 Position		: SV_TARGET0;
	float4 Normal		: SV_TARGET1;
	float4 Albedo		: SV_TARGET2;
	float4 Emissive		: SV_TARGET3;
	float4 Specular		: SV_TARGET4;
	float4 Refraction	: SV_TARGET5;
	float4 Extra		: SV_TARGET6;
};

VSOutput VSMain(uint VertexID : SV_VertexID)
{
	VSOutput OUT;
	{
		uint LightVertexIndex = VertexID;
		LightVertexIndex = LightVertexIndex == 3 ? 2 : LightVertexIndex;
		LightVertexIndex = LightVertexIndex == 4 ? 3 : LightVertexIndex;
		LightVertexIndex = LightVertexIndex == 5 ? 0 : LightVertexIndex;
		
		PolygonalLight Light = Lights[LightID];
		
		float2 localSpacePosition;
		GetLightVertexWS(Light, LightVertexIndex, OUT.PositionW, localSpacePosition);
		
		// Transform to homogeneous/clip space
		OUT.PositionH = mul(float4(OUT.PositionW, 1.0f), RenderPassData.GlobalConstants.ViewProjection);
		
		OUT.Depth = OUT.PositionH.z;
		OUT.Dimension = float2(Light.Width, Light.Height);
	}
	return OUT;
}

PSOutput PSMain(VSOutput IN)
{
	PSOutput OUT;
	{
		OUT.Position				= float4(IN.PositionW, 1.0f);
	}
	return OUT;
}