#ifndef __SKYBOX_HLSLI__
#define __SKYBOX_HLSLI__

#include "HLSLCommon.hlsli"

#define VS_CB_Register b1
#define PS_CB_Register b0

cbuffer RootConstants : register(VS_CB_Register)
{
	uint InstanceID;
};

StructuredBuffer<Vertex>	VertexBuffer	: register(t0, space0);
StructuredBuffer<uint>		IndexBuffer		: register(t1, space0);
StructuredBuffer<Mesh>		Meshes			: register(t2, space0);

#include "ShaderLayout.hlsli"

struct VSOutput
{
	float4 PositionH	: SV_POSITION;
	float3 TextureCoord : TEXCOORD;
};

VSOutput VSMain(uint VertexID : SV_VertexID)
{
	VSOutput OUT;
	{
		Mesh INFO = Meshes[InstanceID];
	
		uint Index = IndexBuffer[INFO.IndexOffset + VertexID];
		Vertex Vertex = VertexBuffer[INFO.VertexOffset + Index];
		
		// Transform to Clip/Homogenized space
		// Set z = w so that z/w = 1 (i.e., skydome always on far plane).
		OUT.PositionH = mul(float4(Vertex.Position, 0.0f), g_SystemConstants.Camera.ViewProjection).xyww;
		OUT.TextureCoord = Vertex.Position;
	}
	return OUT;
}

#endif