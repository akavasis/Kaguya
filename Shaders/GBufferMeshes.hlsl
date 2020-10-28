#include "HLSLCommon.hlsli"
#include "GBuffer.hlsli"

//----------------------------------------------------------------------------------------------------
cbuffer RootConstants
{
	uint InstanceID;
};

StructuredBuffer<Vertex> VertexBuffer : register(t0, space0);
StructuredBuffer<uint> IndexBuffer : register(t1, space0);
StructuredBuffer<Mesh> Meshes : register(t2, space0);
StructuredBuffer<Material> Materials : register(t3, space0);

#include "ShaderLayout.hlsli"

SamplerState AnisotropicClamp : register(s0, space0);

struct VSOutput
{
	float4 PositionH : SV_POSITION;
	float3 PositionW : POSITION;
	float2 TextureCoord : TEXCOORD;
	float3x3 TBNMatrix : TBNBASIS;
};

VSOutput VSMain(uint VertexID : SV_VertexID)
{
	VSOutput OUT;
	{
		Mesh MESH = Meshes[InstanceID];
		uint Index = IndexBuffer[MESH.IndexOffset + VertexID];
		Vertex Vertex = VertexBuffer[MESH.VertexOffset + Index];
		
		// Transform to world space
		OUT.PositionW = mul(float4(Vertex.Position, 1.0f), MESH.World).xyz;
		// Transform to homogeneous/clip space
		OUT.PositionH = mul(float4(OUT.PositionW, 1.0f), g_SystemConstants.Camera.ViewProjection);
	
		OUT.TextureCoord = Vertex.Texture;
		// Transform normal to world space
		// Build orthonormal basis.
		float3 N = normalize(mul(Vertex.Normal, (float3x3) MESH.World));
	
		OUT.TBNMatrix = GetTBNMatrix(N);
	}
	return OUT;
}

float3 GetAlbedoMap(VSOutput IN, Material Material)
{
	int Index = Material.TextureIndices[AlbedoIdx];
	Texture2D AlbedoMap = g_Texture2DTable[Index];
	return AlbedoMap.Sample(AnisotropicClamp, IN.TextureCoord).rgb;
	return Material.Albedo;
}

float3 GetNormalMap(VSOutput IN, Material Material)
{
	int Index = Material.TextureIndices[NormalIdx];
	Texture2D NormalMap = g_Texture2DTable[Index];
	float3 Normal = NormalMap.Sample(AnisotropicClamp, IN.TextureCoord).xyz;
	Normal = Normal * 2.0f - 1.0f;
	return normalize(mul(Normal, IN.TBNMatrix));
	return float3(IN.TBNMatrix[2][0], IN.TBNMatrix[2][1], IN.TBNMatrix[2][2]);
}

float GetRoughnessMap(VSOutput IN, Material Material)
{
	int Index = Material.TextureIndices[RoughnessIdx];
	Texture2D RoughnessMap = g_Texture2DTable[Index];
	return RoughnessMap.Sample(AnisotropicClamp, IN.TextureCoord).r;
}

float GetMetallicMap(VSOutput IN, Material Material)
{
	int Index = Material.TextureIndices[MetallicIdx];
	Texture2D MetallicMap = g_Texture2DTable[Index];
	return MetallicMap.Sample(AnisotropicClamp, IN.TextureCoord).r;
}

PSOutput PSMain(VSOutput IN)
{
	Mesh mesh = Meshes[InstanceID];
	Material material = Materials[mesh.MaterialIndex];
	
	GBufferMesh gbufferMesh = (GBufferMesh) 0;
	gbufferMesh.Position = IN.PositionW;
	gbufferMesh.Normal = GetNormalMap(IN, material);
	gbufferMesh.Albedo = GetAlbedoMap(IN, material);
	gbufferMesh.Roughness = GetRoughnessMap(IN, material);
	gbufferMesh.Metallic = GetMetallicMap(IN, material);
	gbufferMesh.MaterialIndex = mesh.MaterialIndex;

	return PackMeshForPSOutput(gbufferMesh);
}