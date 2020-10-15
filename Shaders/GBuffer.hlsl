#include "HLSLCommon.hlsli"

enum GBufferTypes
{
	WorldPosition,
	WorldNormal,
	MaterialAlbedo,
	MaterialEmissive,
	MaterialSpecular,
	MaterialRefraction,
	MaterialExtra,
	NumGBufferTypes
};

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

struct VSInput
{
	float3 PositionL	: POSITION;
	float2 TextureCoord : TEXCOORD;
	float3 NormalL		: NORMAL;
	float3 TangentL		: TANGENT;
	float3 BitangentL	: BITANGENT;
};

struct VSOutput
{
	float4		PositionH		: SV_POSITION;
	float3		PositionW		: POSITION;
	float2		TextureCoord	: TEXCOORD;
	float3		NormalW			: NORMAL;
	float3		TangentW		: TANGENT;
	float3		BitangentW		: BITANGENT;
	float3x3	TBNMatrix		: TBNBASIS;
	float		Depth			: DEPTH;
};

struct PSOutput
{
	float4 Position		: SV_TARGET0;
	float4 Normal		: SV_TARGET1;
	float4 Albedo		: SV_TARGET2;
	float4 Emissive		: SV_TARGET3;
	float4 Specular		: SV_TARGET4;
	float4 Refraction	: SV_TARGET5;
	float4 Extra		: SV_TARGET7;
};

VSOutput VSMain(uint VertexID : SV_VertexID)
{
	VSOutput OUT;
	{
		GeometryInfo INFO = GeometryInfoBuffer[InstanceID];
	
		uint Index = IndexBuffer[INFO.IndexOffset + VertexID];
		Vertex Vertex = VertexBuffer[INFO.VertexOffset + Index];
		
		// Transform to world space
		OUT.PositionW = mul(float4(Vertex.Position, 1.0f), INFO.World).xyz;
		// Transform to homogeneous/clip space
		OUT.PositionH = mul(float4(OUT.PositionW, 1.0f), RenderPassData.GlobalConstants.ViewProjection);
	
		OUT.TextureCoord = Vertex.Texture;
		// Transform normal and tangent to world space
		// Build orthonormal basis.
		float3 N = normalize(mul(Vertex.Normal, (float3x3) INFO.World));
		float3 T = normalize(mul(Vertex.Tangent, (float3x3) INFO.World));
		float3 B = normalize(mul(Vertex.Bitangent, (float3x3) INFO.World));
	
		OUT.NormalW = N;
		OUT.TangentW = T;
		OUT.BitangentW = B;
		OUT.TBNMatrix = float3x3(T, B, N);
		OUT.Depth = OUT.PositionH.w;
	}
	return OUT;
}

PSOutput PSMain(VSOutput IN)
{
	PSOutput OUT;
	{
		GeometryInfo	INFO		= GeometryInfoBuffer[InstanceID];
		Material		MATERIAL	= Materials[INFO.MaterialIndex];
	
		OUT.Position				= float4(IN.PositionW, 1.0f);
		OUT.Normal					= float4(IN.NormalW, 0.0f);
		OUT.Albedo					= float4(MATERIAL.Albedo, MATERIAL.SpecularChance);
		OUT.Emissive				= float4(MATERIAL.Emissive, MATERIAL.Roughness);
		OUT.Specular				= float4(MATERIAL.Specular, MATERIAL.Fuzziness);
		OUT.Refraction				= float4(MATERIAL.Refraction, MATERIAL.IndexOfRefraction);
		OUT.Extra					= float4(MATERIAL.Model, 0.0f, 0.0f, 0.0f);
	}
	return OUT;
}