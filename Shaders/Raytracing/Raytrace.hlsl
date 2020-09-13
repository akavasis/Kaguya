#include "Global.hlsli"
#include "../Random.hlsli"
#include "../BxDF.hlsli"

Triangle GetTriangle(in uint geometryIndex)
{
	GeometryInfo info = GeometryInfoBuffer[geometryIndex];

	const uint primIndex = PrimitiveIndex();
	const uint idx0 = IndexBuffer[primIndex * 3 + info.IndexOffset + 0];
	const uint idx1 = IndexBuffer[primIndex * 3 + info.IndexOffset + 1];
	const uint idx2 = IndexBuffer[primIndex * 3 + info.IndexOffset + 2];
	
	const Vertex vtx0 = VertexBuffer[idx0 + info.VertexOffset];
	const Vertex vtx1 = VertexBuffer[idx1 + info.VertexOffset];
	const Vertex vtx2 = VertexBuffer[idx2 + info.VertexOffset];
	
	Triangle t;
	t.v0 = vtx0;
	t.v1 = vtx1;
	t.v2 = vtx2;
	return t;
}

Vertex GetBERPedVertex(in HitAttributes attrib, in uint geometryIndex)
{
	float3 barycentrics =
    float3(1.f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);

	Triangle t = GetTriangle(geometryIndex);
	return BERP(t, barycentrics);
}

MaterialTextureIndices GetMaterialIndices(in uint geometryIndex)
{
	GeometryInfo info = GeometryInfoBuffer[geometryIndex];
	return MaterialIndices[info.MaterialIndex];
}

MaterialTextureProperties GetMaterialProperties(in uint geometryIndex)
{
	GeometryInfo info = GeometryInfoBuffer[geometryIndex];
	return MaterialProperties[info.MaterialIndex];
}

struct SurfaceInteraction
{
	float3 position;
	float2 uv;
	BSDF bsdf;
	MaterialTextureIndices materialIndices;
	MaterialTextureProperties materialProperties;
};

SurfaceInteraction GetSurfaceInteraction(in HitAttributes attrib, uint geometryIndex)
{
	SurfaceInteraction si;
	
	Vertex hitSurface = GetBERPedVertex(attrib, geometryIndex);
	MaterialTextureIndices materialIndices = GetMaterialIndices(geometryIndex);
	MaterialTextureProperties materialProperties = GetMaterialProperties(geometryIndex);
	
	// Fetch albedo/diffuse data
	if (materialIndices.AlbedoMapIndex != -1)
	{
		Texture2D albedoMap = Tex2DTable[materialIndices.AlbedoMapIndex];
		float4 albedoMapSample = albedoMap.SampleLevel(SamplerAnisotropicWrap, hitSurface.Texture, 0.0f);
		materialProperties.Albedo = albedoMapSample.rgb;
	}
	
	// Fetch normal data
	if (materialIndices.NormalMapIndex != -1)
	{
		float3x3 tbnMatrix = float3x3(hitSurface.Tangent, hitSurface.Bitangent, hitSurface.Normal);

		Texture2D normalMap = Tex2DTable[materialIndices.NormalMapIndex];
		float4 normalMapSample = normalMap.SampleLevel(SamplerAnisotropicWrap, hitSurface.Texture, 0.0f);
		float3 normalT = normalize(2.0f * normalMapSample.rgb - 1.0f); // Uncompress each component from [0,1] to [-1,1].
		hitSurface.Normal = normalize(mul(normalT, tbnMatrix)); // Transform from tangent space to world space.
	}
	
	// Fetch roughness data
	if (materialIndices.RoughnessMapIndex != -1)
	{
		Texture2D roughnessMap = Tex2DTable[materialIndices.RoughnessMapIndex];
		float4 roughnessMapSample = roughnessMap.SampleLevel(SamplerAnisotropicWrap, hitSurface.Texture, 0.0f);
		materialProperties.Roughness = roughnessMapSample.r;
	}
	
	// Fetch metallic data
	if (materialIndices.MetallicMapIndex != -1)
	{
		Texture2D metallicMap = Tex2DTable[materialIndices.MetallicMapIndex];
		float4 metallicMapSample = metallicMap.SampleLevel(SamplerAnisotropicWrap, hitSurface.Texture, 0.0f);
		materialProperties.Metallic = metallicMapSample.r;
	}
	
	// Fetch emissive data
	if (materialIndices.EmissiveMapIndex != -1)
	{
		Texture2D emissiveMap = Tex2DTable[materialIndices.EmissiveMapIndex];
		float4 emissiveMapSample = emissiveMap.SampleLevel(SamplerAnisotropicWrap, hitSurface.Texture, 0.0f);
		materialProperties.Emissive = emissiveMapSample.rgb;
	}
	
	float3 Rd = lerp(materialProperties.Albedo, 0.0.xxx, materialProperties.Metallic);
	float3 Rs = lerp(0.03f, materialProperties.Albedo, materialProperties.Metallic);
	float alpha = RoughnessToAlphaTrowbridgeReitzDistribution(materialProperties.Roughness);
	
	si.position = WorldRayOrigin() + (WorldRayDirection() * RayTCurrent());
	si.uv = hitSurface.Texture;
	si.bsdf.tangent = hitSurface.Tangent;
	si.bsdf.bitangent = hitSurface.Bitangent;
	si.bsdf.normal = hitSurface.Normal;
	si.bsdf.brdf = InitMicrofacetBRDF(Rd, InitTrowbridgeReitzDistribution(alpha, alpha), InitFresnelDielectric(1.0f, 1.0f));
	si.materialIndices = materialIndices;
	si.materialProperties = materialProperties;
	
	return si;
}

void PathTrace(in RayDesc ray, inout RayPayload payload)
{
	if (payload.Depth <= RenderPassDataCB.MaxDepth)
	{
		// Trace the ray
		const uint flags = RAY_FLAG_NONE;
		const uint mask = 0xffffffff;
		const uint hitGroupIndex = RayTypePrimary;
		const uint hitGroupIndexMultiplier = NumRayTypes;
		const uint missShaderIndex = RayTypePrimary;
		TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, payload);
	}
}

struct HitInfo
{
	uint GeometryIndex;
};
ConstantBuffer<HitInfo> HitGroupCB : register(b0, space0);

[shader("raygeneration")]
void RayGen()
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 launchDimensions = DispatchRaysDimensions().xy;
	const float2 pixel = (float2(launchIndex) + 0.5f) / float2(launchDimensions); // Convert from discrete to continuous
	float2 ndcCoord = pixel * 2.0f - 1.0f; // Uncompress each component from [0,1] to [-1,1].
	ndcCoord.y *= -1.0f;
	
	uint seed = uint(launchIndex.x * uint(1973) + launchIndex.y * uint(9277) + uint(RenderPassDataCB.TotalFrameCount) * uint(26699)) | uint(1);
  
	float3 origin = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), RenderPassDataCB.InvView).xyz;
	float4 target = mul(float4(ndcCoord, 1.0f, 1.0f), RenderPassDataCB.InvProjection);
	float3 direction = mul(float4(target.xyz, 0.0f), RenderPassDataCB.InvView).xyz;
	
	RayDesc ray = { origin, 0.001f, direction, 100000.0f };
	RayPayload payload = { float3(0.0f, 0.0f, 0.0f), float3(1.0f, 1.0f, 1.0f), seed, 0 };
	
	PathTrace(ray, payload);

	RenderTarget[launchIndex] = float4(payload.Radiance, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload payload)
{
	payload.Radiance = TexCubeTable[RenderPassDataCB.IrradianceCubemapIndex].SampleLevel(SamplerLinearWrap, WorldRayDirection(), 0.0f).rgb;
}

[shader("miss")]
void ShadowMiss(inout ShadowRayPayload payload)
{
	payload.Visibility = 1.0f;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in HitAttributes attrib)
{
	SurfaceInteraction si = GetSurfaceInteraction(attrib, HitGroupCB.GeometryIndex);
	
	payload.Radiance += si.materialProperties.Emissive * payload.Throughput;
	payload.Throughput *= si.materialProperties.Albedo;
	payload.Depth++;
	
	float3 position = si.position + si.bsdf.normal * 0.01f;
	float3 direction = normalize(si.bsdf.normal + RandomUnitVector(payload.Seed));
	RayDesc ray = { position, 0.001f, direction, 100000.0f };
	PathTrace(ray, payload);
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowRayPayload payload, in HitAttributes attrib)
{
	payload.Visibility = 0.0f;
}