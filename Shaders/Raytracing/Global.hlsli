#ifndef GLOBAL_HLSLI
#define GLOBAL_HLSLI

// This file defines global root signature for raytracing shaders
#include <HLSLCommon.hlsli>
#include <Random.hlsli>
#include <Sampling.hlsli>
#include <BxDF.hlsli>
#include "Common.hlsli"

#define RAYTRACING_INSTANCEMASK_ALL 	(0xff)
#define RAYTRACING_INSTANCEMASK_OPAQUE 	(1 << 0)
#define RAYTRACING_INSTANCEMASK_LIGHT	(1 << 1)

RaytracingAccelerationStructure		SceneBVH			: register(t0, space0);
StructuredBuffer<Mesh>				Meshes				: register(t1, space0);
StructuredBuffer<PolygonalLight>	Lights				: register(t2, space0);
StructuredBuffer<Material>			Materials			: register(t3, space0);

SamplerState						SamplerLinearWrap	: register(s0, space0);
SamplerState						SamplerLinearClamp	: register(s1, space0);

#include <ShaderLayout.hlsli>

// Local Root Signature
// ====================
StructuredBuffer<Vertex>			VertexBuffer		: register(t0, space1);
StructuredBuffer<uint>				IndexBuffer			: register(t1, space1);

Triangle GetTriangle()
{
	Mesh mesh = Meshes[InstanceID()];

	const uint primIndex = PrimitiveIndex();
	const uint idx0 = IndexBuffer[primIndex * 3 + mesh.IndexOffset + 0];
	const uint idx1 = IndexBuffer[primIndex * 3 + mesh.IndexOffset + 1];
	const uint idx2 = IndexBuffer[primIndex * 3 + mesh.IndexOffset + 2];
	
	const Vertex vtx0 = VertexBuffer[idx0 + mesh.VertexOffset];
	const Vertex vtx1 = VertexBuffer[idx1 + mesh.VertexOffset];
	const Vertex vtx2 = VertexBuffer[idx2 + mesh.VertexOffset];
	
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
	float3 tangent;
	float3 bitangent;
	float3 normal;
	Material material;
};

SurfaceInteraction GetSurfaceInteraction(in HitAttributes attrib)
{
	SurfaceInteraction si;
	
	Mesh mesh = Meshes[InstanceID()];
	Vertex vertex = GetInterpolatedVertex(attrib);
	Material material = Materials[mesh.MaterialIndex];
	
	vertex.Normal = normalize(mul(vertex.Normal, (float3x3) mesh.World));
	ONB onb = InitONB(vertex.Normal);
	
	si.frontFace = dot(WorldRayDirection(), vertex.Normal) < 0.0f;
	si.position = WorldRayOrigin() + (WorldRayDirection() * RayTCurrent());
	si.uv = vertex.Texture;
	si.tangent = onb.tangent;
	si.bitangent = onb.bitangent;
	si.normal = onb.normal;
	si.material = material;
	
	return si;
}

RayDesc GenerateCameraRay(in float2 ndc, inout uint seed)
{
	float3 direction = ndc.x * g_SystemConstants.Camera.U.xyz + ndc.y * g_SystemConstants.Camera.V.xyz + g_SystemConstants.Camera.W.xyz;
	
	// Find the focal point for this pixel
	direction /= length(g_SystemConstants.Camera.W); // Make ray have length 1 along the camera's w-axis.
	float3 focalPoint = g_SystemConstants.Camera.Position.xyz + g_SystemConstants.Camera.FocalLength * direction; // Select point on ray a distance FocalLength along the w-axis
	
	// Get random numbers (in polar coordinates), convert to random cartesian uv on the lens
	float2 rnd = float2(s_2PI * RandomFloat01(seed), g_SystemConstants.Camera.RelativeAperture * RandomFloat01(seed));
	float2 uv = float2(cos(rnd.x) * rnd.y, sin(rnd.x) * rnd.y);

	// Use uv coordinate to compute a random origin on the camera lens
	float3 origin = g_SystemConstants.Camera.Position.xyz + uv.x * normalize(g_SystemConstants.Camera.U.xyz) + uv.y * normalize(g_SystemConstants.Camera.V.xyz);
	direction = normalize(focalPoint - origin);
	
	RayDesc ray = { origin, 0.0f, direction, 1e+38f };
	return ray;
}

#endif