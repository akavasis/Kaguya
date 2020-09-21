// This file defines global root signature for raytracing shaders
#include "../HLSLCommon.hlsli"
#include "../Random.hlsli"
#include "../BxDF.hlsli"
#include "Common.hlsli"

RaytracingAccelerationStructure SceneBVH                        : register(t0, space0);
StructuredBuffer<Vertex> VertexBuffer                           : register(t1, space0);
StructuredBuffer<uint> IndexBuffer                              : register(t2, space0);
StructuredBuffer<GeometryInfo> GeometryInfoBuffer               : register(t3, space0);
StructuredBuffer<Material> Materials							: register(t4, space0);

#define RenderPassDataType RenderPassConstants
#include "../ShaderLayout.hlsli"

Triangle GetTriangle()
{
	GeometryInfo info = GeometryInfoBuffer[InstanceID()];

	const uint primIndex = PrimitiveIndex();
	const uint idx0 = IndexBuffer[primIndex * 3 + info.IndexOffset + 0];
	const uint idx1 = IndexBuffer[primIndex * 3 + info.IndexOffset + 1];
	const uint idx2 = IndexBuffer[primIndex * 3 + info.IndexOffset + 2];
	
	const Vertex vtx0 = VertexBuffer[idx0 + info.VertexOffset];
	const Vertex vtx1 = VertexBuffer[idx1 + info.VertexOffset];
	const Vertex vtx2 = VertexBuffer[idx2 + info.VertexOffset];
	
	Triangle t = { vtx0, vtx1, vtx2 };
	return t;
}

Vertex GetInterpolatedVertex(in HitAttributes attrib)
{
	float3 barycentrics = float3(1.f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);

	Triangle t = GetTriangle();
	return BarycentricInterpolation(t, barycentrics);
}

struct SurfaceInteraction
{
	bool frontFace;
	float3 position;
	float2 uv;
	BSDF bsdf;
	Material material;
};

SurfaceInteraction GetSurfaceInteraction(in HitAttributes attrib)
{
	SurfaceInteraction si;
	
	GeometryInfo info = GeometryInfoBuffer[InstanceID()];
	Vertex hitSurface = GetInterpolatedVertex(attrib);
	Material material = Materials[info.MaterialIndex];
	
	hitSurface.Tangent = mul(float4(hitSurface.Tangent, 0.0f), info.World).xyz;
	hitSurface.Bitangent = mul(float4(hitSurface.Bitangent, 0.0f), info.World).xyz;
	hitSurface.Normal = mul(float4(hitSurface.Normal, 0.0f), info.World).xyz;
	
	si.frontFace = dot(WorldRayDirection(), hitSurface.Normal) < 0.0f;
	si.position = WorldRayOrigin() + (WorldRayDirection() * RayTCurrent());
	si.uv = hitSurface.Texture;
	si.bsdf.tangent = hitSurface.Tangent;
	si.bsdf.bitangent = hitSurface.Bitangent;
	si.bsdf.normal = hitSurface.Normal;
	//si.bsdf.brdf = InitMicrofacetBRDF(Rd, InitTrowbridgeReitzDistribution(alpha, alpha), InitFresnelDielectric(1.0f, 1.0f)); // Not used yet, still reading Physically Based Rendering
	// Perhaps i should merge these 2 structs into 1 unified Material struct...
	si.material = material;
	
	return si;
}