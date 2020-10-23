#include "HLSLCommon.hlsli"
#include "GBuffer.hlsli"

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
	uint LightIndex;
};

StructuredBuffer<PolygonalLight> Lights : register(t0, space0);

#include "ShaderLayout.hlsli"

struct VSOutput
{
	float4		PositionH		: SV_POSITION;
	float3		PositionW		: POSITION;
	float		Depth			: DEPTH;
	float2		Dimension		: DIMENSION;
};

VSOutput VSMain(uint VertexID : SV_VertexID)
{
	VSOutput OUT;
	{
		uint LightVertexIndex = VertexID;
		LightVertexIndex = LightVertexIndex == 3 ? 2 : LightVertexIndex;
		LightVertexIndex = LightVertexIndex == 4 ? 3 : LightVertexIndex;
		LightVertexIndex = LightVertexIndex == 5 ? 0 : LightVertexIndex;
		
		PolygonalLight Light = Lights[LightIndex];
		
		float2 localSpacePosition;
		GetLightVertexWS(Light, LightVertexIndex, OUT.PositionW, localSpacePosition);
		
		// Transform to homogeneous/clip space
		OUT.PositionH = mul(float4(OUT.PositionW, 1.0f), SystemConstants.ViewProjection);
		
		OUT.Depth = OUT.PositionH.z;
		OUT.Dimension = float2(Light.Width, Light.Height);
	}
	return OUT;
}

PSOutput PSMain(VSOutput IN)
{
	PSOutput OUT;
	{
		GBufferLight GBufferLight;
		GBufferLight.LightIndex = LightIndex;
		
		OUT = GetGBufferLightPSOutput(GBufferLight);
	}
	return OUT;
}