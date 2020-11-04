#include "HLSLCommon.hlsli"
#include "GBuffer.hlsli"

//----------------------------------------------------------------------------------------------------
cbuffer RootConstants
{
	uint LightIndex;
};

StructuredBuffer<PolygonalLight> Lights : register(t0, space0);

#include "ShaderLayout.hlsli"

struct VSOutput
{
	float4 PositionH : SV_POSITION;
	float3 PositionW : POSITION;
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
		
		float halfWidth = Light.Width * 0.5f;
		float halfHeight = Light.Height * 0.5f;

		// Get billboard points at the origin
		float dx = (LightVertexIndex == 0 || LightVertexIndex == 1) ? halfWidth : -halfWidth;
		float dy = (LightVertexIndex == 0 || LightVertexIndex == 3) ? -halfHeight : halfHeight;

		float2 localSpacePosition = float2(dx, dy);
		float3 lightPoint = float3(localSpacePosition, 0.0f);

		// Rotate around origin
		lightPoint = mul(lightPoint, (float3x3) Light.World);

		// Move points to light's location
		OUT.PositionW = lightPoint + float3(Light.World[3][0], Light.World[3][1], Light.World[3][2]);
		
		// Transform to homogeneous/clip space
		OUT.PositionH = mul(float4(OUT.PositionW, 1.0f), g_SystemConstants.Camera.ViewProjection);
	}
	return OUT;
}

MRT PSMain(VSOutput IN)
{
	GBufferLight light = (GBufferLight) 0;
	light.LightIndex = LightIndex;
	
	MRT OUT;
	OUT.Position			= 0.0.xxxx;
	OUT.Normal				= 0.0.xxxx;
	OUT.Albedo				= 0.0.xxxx;
	OUT.TypeAndIndex		= (GBufferTypeLight << 4) | (light.LightIndex & 0x0000000F);
	OUT.SVGF_LinearZ		= 0.0.xxxx;
	OUT.SVGF_MotionVector	= 0.0.xxxx;
	OUT.SVGF_Compact		= 0.0.xxxx;
	return OUT;
}