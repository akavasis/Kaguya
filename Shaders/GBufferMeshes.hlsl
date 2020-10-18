#include "HLSLCommon.hlsli"
#include "GBuffer.hlsli"

//----------------------------------------------------------------------------------------------------
cbuffer RootConstants
{
	uint InstanceID;
};

StructuredBuffer<Vertex>		VertexBuffer		: register(t0, space0);
StructuredBuffer<uint>			IndexBuffer			: register(t1, space0);
StructuredBuffer<GeometryInfo>	GeometryInfoBuffer	: register(t2, space0);
StructuredBuffer<Material>		Materials			: register(t3, space0);

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
	float2		TextureCoord	: TEXCOORD;
	float3x3	TBNMatrix		: TBNBASIS;
};

VSOutput VSMain(uint VertexID : SV_VertexID)
{
	VSOutput OUT;
	{
		GeometryInfo	INFO	= GeometryInfoBuffer[InstanceID];
		uint			Index	= IndexBuffer[INFO.IndexOffset + VertexID];
		Vertex			Vertex	= VertexBuffer[INFO.VertexOffset + Index];
		
		// Transform to world space
		OUT.PositionW			= mul(float4(Vertex.Position, 1.0f), INFO.World).xyz;
		// Transform to homogeneous/clip space
		OUT.PositionH			= mul(float4(OUT.PositionW, 1.0f), RenderPassData.GlobalConstants.ViewProjection);
	
		OUT.TextureCoord		= Vertex.Texture;
		// Transform normal to world space
		// Build orthonormal basis.
		float3 N				= normalize(mul(Vertex.Normal, (float3x3) INFO.World));
	
		OUT.TBNMatrix			= GetTBNMatrix(N);
	}
	return OUT;
}

PSOutput PSMain(VSOutput IN)
{
	PSOutput OUT;
	{
		GeometryInfo	INFO		= GeometryInfoBuffer[InstanceID];
		Material		MATERIAL	= Materials[INFO.MaterialIndex];
		
		GBufferMesh GBufferMesh;
		GBufferMesh.Position		= IN.PositionW;
		GBufferMesh.Normal			= float3(IN.TBNMatrix[2][0], IN.TBNMatrix[2][1], IN.TBNMatrix[2][2]);
		GBufferMesh.Albedo			= float3(MATERIAL.Albedo);
		GBufferMesh.MaterialIndex	= INFO.MaterialIndex;
		
		OUT = GetGBufferMeshPSOutput(GBufferMesh);
	}
	return OUT;
}