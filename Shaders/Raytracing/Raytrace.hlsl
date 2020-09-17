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
	
	Triangle t = { vtx0, vtx1, vtx2 };
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
	bool frontFace;
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
	
	//float3 Rd = lerp(materialProperties.Albedo, 0.0.xxx, materialProperties.Metallic);
	//float3 Rs = lerp(0.03f, materialProperties.Albedo, materialProperties.Metallic);
	//float alpha = RoughnessToAlphaTrowbridgeReitzDistribution(materialProperties.Roughness);
	
	si.frontFace = dot(WorldRayDirection(), hitSurface.Normal) < 0.0f;
	si.position = WorldRayOrigin() + (WorldRayDirection() * RayTCurrent());
	si.uv = hitSurface.Texture;
	si.bsdf.tangent = hitSurface.Tangent;
	si.bsdf.bitangent = hitSurface.Bitangent;
	si.bsdf.normal = hitSurface.Normal;
	//si.bsdf.brdf = InitMicrofacetBRDF(Rd, InitTrowbridgeReitzDistribution(alpha, alpha), InitFresnelDielectric(1.0f, 1.0f)); // Not used yet, still reading Physically Based Rendering
	// Perhaps i should merge these 2 structs into 1 unified Material struct...
	si.materialIndices = materialIndices;
	si.materialProperties = materialProperties;
	
	return si;
}

struct Lambertian
{
	float3 Albedo;
	
	void Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out float3 attenuation, out RayDesc scatteredRay)
	{
		float3 direction = si.bsdf.normal + RandomUnitVector(rayPayload.Seed);
		RayDesc ray = { si.position, 0.00001f, direction, 100000.0f };
		
		attenuation = Albedo;
		scatteredRay = ray;
	}
};

struct Metal
{
	float3 Albedo;
	float Fuzziness;
	
	void Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out float3 attenuation, out RayDesc scatteredRay)
	{
		Fuzziness = Fuzziness < 1.0f ? Fuzziness : 1.0f;
		
		float3 reflected = reflect(WorldRayDirection(), si.bsdf.normal) + Fuzziness * RandomInUnitSphere(rayPayload.Seed);
		RayDesc ray = { si.position, 0.00001f, reflected, 100000.0f };
		
		attenuation = Albedo;
		scatteredRay = ray;
	}
};

struct Dielectric
{
	float IndexOfRefraction;
	
	void Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out float3 attenuation, out RayDesc scatteredRay)
	{
		attenuation = float3(1.0f, 1.0f, 1.0f);
		RayDesc ray = { si.position, 0.00001f, float3(0.0f, 0.0f, 0.0f), 100000.0f };
		
		float etaI_Over_etaT = si.frontFace ? (1.0f / IndexOfRefraction) : IndexOfRefraction;
	
		float cosTheta = min(dot(-WorldRayDirection(), si.bsdf.normal), 1.0f);
		float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
	
		// Total internal reflection
		if ((etaI_Over_etaT * sinTheta) > 1.0f ||
			RandomFloat01(rayPayload.Seed) < Fresnel_Schlick(etaI_Over_etaT, cosTheta))
		{
			float3 reflected = reflect(WorldRayDirection(), si.bsdf.normal);
			ray.Direction = reflected;
			attenuation = si.materialProperties.SpecularColor;
		}
		else
		{
			float3 refracted = refract(WorldRayDirection(), si.bsdf.normal, etaI_Over_etaT);
			ray.Direction = refracted;
		}
		
		// do absorption if we are hitting from inside the object
		if (!si.frontFace)
		{
			attenuation *= exp(-si.materialProperties.RefractionColor * RayTCurrent());
		}
		
		scatteredRay = ray;
	}
};

struct DiffuseLight
{
	void Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out float3 attenuation, out RayDesc scatteredRay)
	{
		si.bsdf.normal = si.frontFace ? si.bsdf.normal : -si.bsdf.normal;
		
		float3 direction = si.bsdf.normal + RandomUnitVector(rayPayload.Seed);
		RayDesc ray = { si.position, 0.00001f, direction, 100000.0f };
		
		attenuation = float3(0.0f, 0.0f, 0.0f);
		scatteredRay = ray;
	}
};

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
	uint seed = uint(launchIndex.x * uint(1973) + launchIndex.y * uint(9277) + uint(RenderPassDataCB.TotalFrameCount) * uint(26699)) | uint(1);
	
	// calculate subpixel camera jitter for anti aliasing
	const float2 jitter = float2(RandomFloat01(seed), RandomFloat01(seed)) - 0.5f;
	const float2 pixel = (float2(launchIndex) + 0.5f + jitter) / float2(launchDimensions); // Convert from discrete to continuous
	
	float2 ndcCoord = pixel * 2.0f - 1.0f; // Uncompress each component from [0,1] to [-1,1].
	ndcCoord.y *= -1.0f; // Flip y
	
	float3 origin = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), RenderPassDataCB.InvView).xyz;
	float4 target = mul(float4(ndcCoord, 1.0f, 1.0f), RenderPassDataCB.InvProjection);
	float3 direction = mul(float4(target.xyz, 0.0f), RenderPassDataCB.InvView).xyz;
	
	// Initialize ray
	RayDesc ray = { origin, 0.00001f, direction, 100000.0f };
	// Initialize payload
	RayPayload rayPayload = { float3(0.0f, 0.0f, 0.0f), float3(1.0f, 1.0f, 1.0f), seed, 0 };
	// Trace the ray
	const uint flags = RAY_FLAG_NONE;
	const uint mask = 0xffffffff;
	const uint hitGroupIndex = RayTypePrimary;
	const uint hitGroupIndexMultiplier = NumRayTypes;
	const uint missShaderIndex = RayTypePrimary;
	TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, rayPayload);
	
	RenderTarget[launchIndex] = float4(rayPayload.Radiance, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload rayPayload)
{
	rayPayload.Radiance += TexCubeTable[RenderPassDataCB.RadianceCubemapIndex].SampleLevel(SamplerLinearWrap, WorldRayDirection(), 0.0f).rgb * rayPayload.Throughput;
}

[shader("miss")]
void ShadowMiss(inout ShadowRayPayload rayPayload)
{
	rayPayload.Visibility = 1.0f;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload rayPayload, in HitAttributes attrib)
{
	SurfaceInteraction si = GetSurfaceInteraction(attrib, HitGroupCB.GeometryIndex);
	
	float3 attentuation;
	RayDesc ray;
	switch (si.materialProperties.Model)
	{
		case MATERIAL_MODEL_LAMBERTIAN:
		{
			Lambertian lambertian = { si.materialProperties.Albedo };
			lambertian.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		case MATERIAL_MODEL_METAL:
		{
			Metal metal = { si.materialProperties.Albedo, 0.0f };
			metal.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		case MATERIAL_MODEL_DIELECTRIC:
		{
			Dielectric dielectric = { si.materialProperties.IndexOfRefraction };
			dielectric.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		case MATERIAL_MODEL_DIFFUSE_LIGHT:
		{
			DiffuseLight diffuseLight;
			diffuseLight.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		default:
		{
			rayPayload.Radiance = float3(0.0f, 0.0f, 0.0f);
			return;
		}
		break;
	}
	
	rayPayload.Radiance += si.materialProperties.Emissive * rayPayload.Throughput;
	rayPayload.Throughput *= attentuation;
	
	// Path trace
	if (rayPayload.Depth + 1 < RenderPassDataCB.MaxDepth)
	{
		rayPayload.Depth = rayPayload.Depth + 1;
		
		const uint flags = RAY_FLAG_NONE;
		const uint mask = 0xffffffff;
		const uint hitGroupIndex = RayTypePrimary;
		const uint hitGroupIndexMultiplier = NumRayTypes;
		const uint missShaderIndex = RayTypePrimary;
		TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, rayPayload);
	}
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowRayPayload rayPayload, in HitAttributes attrib)
{
	rayPayload.Visibility = 0.0f;
}