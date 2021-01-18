struct RenderPassData
{
	uint NumSamplesPerPixel;
	uint MaxDepth;
	uint NumAccumulatedSamples;

	uint RenderTarget;
};
#define RenderPassDataType RenderPassData
#include "Global.hlsli"

// Local Root Signature
// ====================
StructuredBuffer<Vertex> VertexBuffer : register(t0, space1);
StructuredBuffer<uint> IndexBuffer : register(t1, space1);

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

// Hit information, aka ray payload
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
struct RayPayload
{
	float3 Radiance;
	float3 Throughput;
	float3 Position;
	float3 Direction;
	uint Seed;
	uint Depth;
};

struct ScatterRecord
{
	RayDesc SpecularRay;
	bool IsSpecular;
	float3 Attenuation;
	float Pdf;
};

enum RayType
{
	RayTypePrimary,
    NumRayTypes
};

struct Lambertian
{
	float3 Albedo;
	
	bool Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out ScatterRecord scatterRecord)
	{
		float2 u;
		u[0] = RandomFloat01(rayPayload.Seed);
		u[1] = RandomFloat01(rayPayload.Seed);

		ONB onb = InitONB(si.normal);
		float3 direction = onb.InverseTransform(CosineSampleHemisphere(u));
		
		RayDesc ray = { si.position, 0.0001f, direction, 100000.0f };
		scatterRecord.SpecularRay = ray;
		scatterRecord.IsSpecular = false;
		scatterRecord.Attenuation = Albedo;
		scatterRecord.Pdf = CosineHemispherePdf(dot(onb.normal, ray.Direction));
		
		return true;
	}

	float ScatteringPDF(in SurfaceInteraction si, in RayDesc scatteredRay)
	{
		float cosTheta = dot(si.normal, normalize(scatteredRay.Direction));
		return cosTheta < 0.0f ? 0.0f : cosTheta / s_PI;
	}
	
	float3 Emitted(in SurfaceInteraction si)
	{
		return 0.0.xxx;
	}
};

struct Glossy
{	
	bool Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out ScatterRecord scatterRecord)
	{
		float doSpecular = RandomFloat01(rayPayload.Seed) < SpecularChance ? 1.0f : 0.0f;
		
		float3 diffuse = si.normal + RandomUnitVector(rayPayload.Seed);
		float3 reflected = reflect(WorldRayDirection(), si.normal);
		reflected = normalize(lerp(reflected, diffuse, Roughness * Roughness));
		
		float3 direction = lerp(diffuse, reflected, doSpecular);
		RayDesc ray = { si.position, 0.0001f, direction, 100000.0f };
		
		scatterRecord.SpecularRay = ray;
		scatterRecord.IsSpecular = true;
		scatterRecord.Attenuation = lerp(Albedo, Specular, doSpecular);
		scatterRecord.Pdf = 0.0f;
		
		return true;
	}

	float ScatteringPDF(in SurfaceInteraction si, in RayDesc scatteredRay)
	{
		return 0.0f;
	}
	
	float3 Emitted(in SurfaceInteraction si)
	{
		return 0.0.xxx;
	}
	
	float3 Albedo;
	float SpecularChance;
	float Roughness;
	float3 Specular;
};

struct Metal
{
	bool Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out ScatterRecord scatterRecord)
	{
		Fuzziness = Fuzziness < 1.0f ? Fuzziness : 1.0f;
		
		float3 reflected = reflect(WorldRayDirection(), si.normal) + Fuzziness * RandomInUnitSphere(rayPayload.Seed);
		
		RayDesc ray = { si.position, 0.0001f, normalize(reflected), 100000.0f };
		scatterRecord.SpecularRay = ray;
		scatterRecord.IsSpecular = true;
		scatterRecord.Attenuation = Albedo;
		scatterRecord.Pdf = 0.0f;
		
		return true;
	}

	float ScatteringPDF(in SurfaceInteraction si, in RayDesc scatteredRay)
	{
		return 0.0f;
	}
	
	float3 Emitted(in SurfaceInteraction si)
	{
		return 0.0.xxx;
	}
	
	float3 Albedo;
	float Fuzziness;
};

struct Dielectric
{
	bool Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out ScatterRecord scatterRecord)
	{
		si.normal = si.frontFace ? si.normal : -si.normal;
		float etaI_Over_etaT = si.frontFace ? (1.0f / IndexOfRefraction) : IndexOfRefraction;
	
		float cosTheta = min(dot(-WorldRayDirection(), si.normal), 1.0f);
		float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
		
		// Total internal reflection
		bool cannotRefract = (etaI_Over_etaT * sinTheta) > 1.0f;
		float3 direction;
	
		if (cannotRefract ||
			RandomFloat01(rayPayload.Seed) < F_Schlick(etaI_Over_etaT, cosTheta))
		{
			direction = reflect(WorldRayDirection(), si.normal);
		}
		else
		{
			direction = refract(WorldRayDirection(), si.normal, etaI_Over_etaT);
		}
		
		RayDesc ray = { si.position, 0.0001f, direction, 100000.0f };
		scatterRecord.SpecularRay = ray;
		scatterRecord.IsSpecular = true;
		scatterRecord.Attenuation = float3(1.0f, 1.0f, 1.0f);
		scatterRecord.Pdf = 0.0f;
		
		return true;
	}

	float ScatteringPDF(in SurfaceInteraction si, in RayDesc scatteredRay)
	{
		return 0.0f;
	}
	
	float3 Emitted(in SurfaceInteraction si)
	{
		return 0.0.xxx;
	}
	
	float IndexOfRefraction;
	float3 Specular;
	float3 Refraction;
};

struct DiffuseLight
{
	bool Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out ScatterRecord scatterRecord)
	{
		// Need to provide initialized value here or DXC complains
		RayDesc ray = { float3(0.0f, 0.0f, 0.0f), 0.0f, float3(0.0f, 0.0f, 0.0f), 0.0f };
		scatterRecord.SpecularRay = ray;
		scatterRecord.IsSpecular = false;
		scatterRecord.Attenuation = float3(1.0f, 1.0f, 1.0f);
		scatterRecord.Pdf = 0.0f;
		
		return false;
	}

	float ScatteringPDF(in SurfaceInteraction si, in RayDesc scatteredRay)
	{
		return 0.0f;
	}
	
	float3 Emitted(in SurfaceInteraction si)
	{
		return Emissive;
	}
	
	float3 Emissive;
};

struct CosinePdf
{
	ONB onb;

	float Value(in float3 direction)
	{
		return CosineHemispherePdf(dot(onb.normal, direction));
	}

	float3 Generate(inout uint seed)
	{
		float2 u;
		u[0] = RandomFloat01(seed);
		u[1] = RandomFloat01(seed);

		return onb.InverseTransform(CosineSampleHemisphere(u));
	}
};

CosinePdf InitCosinePdf(in float3 w)
{
	CosinePdf cosinePdf;
	cosinePdf.onb = InitONB(w);
	return cosinePdf;
}

void TerminateRay(inout RayPayload rayPayload)
{
	rayPayload.Depth = g_RenderPassData.MaxDepth;
}

float3 PathTrace(RayDesc Ray, inout uint Seed)
{
	RayPayload RayPayload = { float3(0.0f, 0.0f, 0.0f), float3(1.0f, 1.0f, 1.0f), float3(0.0f, 0.0f, 0.0f), float3(0.0f, 0.0f, 0.0f), Seed, 0 };
	
	while (RayPayload.Depth < g_RenderPassData.MaxDepth)
	{
		// Trace the ray
		const uint flags = RAY_FLAG_NONE;
		const uint mask = 0xffffffff;
		const uint hitGroupIndex = RayTypePrimary;
		const uint hitGroupIndexMultiplier = NumRayTypes;
		const uint missShaderIndex = RayTypePrimary;
		TraceRay(Scene, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, Ray, RayPayload);
		
		Ray.Origin = RayPayload.Position;
		Ray.TMin = 0.0001f; // Avoid self intersection
		Ray.Direction = RayPayload.Direction;
		
		RayPayload.Depth++;
	}

	return RayPayload.Radiance;
}

[shader("raygeneration")]
void RayGeneration()
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 launchDimensions = DispatchRaysDimensions().xy;
	uint seed = uint(launchIndex.x * uint(1973) + launchIndex.y * uint(9277) + uint(g_SystemConstants.TotalFrameCount) * uint(26699)) | uint(1);
	
	float3 color = float3(0.0f, 0.0f, 0.0f);
	for (int sample = 0; sample < g_RenderPassData.NumSamplesPerPixel; ++sample)
	{
		// Calculate subpixel camera jitter for anti aliasing
		const float2 jitter = float2(RandomFloat01(seed), RandomFloat01(seed)) - 0.5f;
		const float2 pixel = (float2(launchIndex) + jitter) / float2(launchDimensions);

		const float2 ndc = float2(2, -2) * pixel + float2(-1, 1);

		// Initialize ray
		RayDesc ray = GenerateCameraRay(ndc, seed);
	
		color += PathTrace(ray, seed);
	}
	color *= 1.0f / float(g_RenderPassData.NumSamplesPerPixel);

	RWTexture2D<float4> RenderTarget = g_RWTexture2DTable[g_RenderPassData.RenderTarget];
	
	float3 finalColor;
	if (g_RenderPassData.NumAccumulatedSamples > 0)
	{
		float3 prev = RenderTarget[launchIndex].rgb;
		finalColor = lerp(prev, color, 1.0f / float(g_RenderPassData.NumAccumulatedSamples)); // accumulate
	}
	else
	{
		finalColor = color;
	}
	
	RenderTarget[launchIndex] = float4(finalColor, 1);
}

[shader("miss")]
void Miss(inout RayPayload rayPayload)
{
	float3 d = WorldRayDirection();

	//float3 sky = g_TextureCubeTable[g_SystemConstants.Skybox].SampleLevel(SamplerLinearWrap, d, 0.0f).rgb;
	//float3 sunDir = normalize(float3(.8, .55, 1.));
	//float3 sun = pow(abs(dot(d, sunDir)), 5000);
	
	float t = 0.5f * (d.y + 1.0f);
	float3 c = lerp(float3(1.0, 1.0, 1.0), float3(0.5, 0.7, 1.0), t);
	
	//rayPayload.Radiance += (sky + sun) * rayPayload.Throughput;
	rayPayload.Radiance += c * rayPayload.Throughput;
	TerminateRay(rayPayload);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload rayPayload, in HitAttributes attrib)
{
	SurfaceInteraction si = GetSurfaceInteraction(attrib);
	
	ScatterRecord scatterRecord;
	float3 emitted;
	float scatteringPDF;
	bool shouldScatter;
	switch (si.material.Model)
	{
		case LambertianModel:
		{
			Lambertian lambertian = { si.material.Albedo };
			shouldScatter = lambertian.Scatter(si, rayPayload, scatterRecord);
			emitted = lambertian.Emitted(si);
			scatteringPDF = lambertian.ScatteringPDF(si, scatterRecord.SpecularRay);
		}
		break;
		
		case GlossyModel:
		{
			Glossy glossy = { si.material.Albedo, si.material.SpecularChance, si.material.Roughness, si.material.Specular };
			shouldScatter = glossy.Scatter(si, rayPayload, scatterRecord);
			emitted = glossy.Emitted(si);
			scatteringPDF = glossy.ScatteringPDF(si, scatterRecord.SpecularRay);
		}
		break;
		
		case MetalModel:
		{
			Metal metal = { si.material.Albedo, si.material.Fuzziness };
			shouldScatter = metal.Scatter(si, rayPayload, scatterRecord);
			emitted = metal.Emitted(si);
			scatteringPDF = metal.ScatteringPDF(si, scatterRecord.SpecularRay);
		}
		break;
		
		case DielectricModel:
		{
			Dielectric dielectric = { si.material.IndexOfRefraction, si.material.Specular, si.material.Refraction };
			shouldScatter = dielectric.Scatter(si, rayPayload, scatterRecord);
			emitted = dielectric.Emitted(si);
			scatteringPDF = dielectric.ScatteringPDF(si, scatterRecord.SpecularRay);
		}
		break;
		
		case DiffuseLightModel:
		{
			DiffuseLight diffuseLight = { si.material.Emissive };
			shouldScatter = diffuseLight.Scatter(si, rayPayload, scatterRecord);
			emitted = diffuseLight.Emitted(si);
			scatteringPDF = diffuseLight.ScatteringPDF(si, scatterRecord.SpecularRay);
		}
		break;
	}
	
	rayPayload.Radiance += emitted * rayPayload.Throughput;
	rayPayload.Throughput *= scatterRecord.Attenuation;
	
	if (shouldScatter)
	{	
		rayPayload.Position = scatterRecord.SpecularRay.Origin;
		rayPayload.Direction = scatterRecord.SpecularRay.Direction;
	}
	else
	{
		TerminateRay(rayPayload);
	}
}