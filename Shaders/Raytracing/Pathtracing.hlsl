#include "Global.hlsli"

struct PathtracingData
{
	GlobalConstants GlobalConstants;
	uint OutputIndex;
};

#define RenderPassDataType PathtracingData
#include "../ShaderLayout.hlsli"

// Hit information, aka ray payload
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
struct RayPayload
{
	float3 Radiance;
	float3 Throughput;
	uint Seed;
	uint Depth;
};

enum RayType
{
	RayTypePrimary,
    NumRayTypes
};

struct Lambertian
{
	float3 Albedo;
	
	bool Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out float3 attenuation, out RayDesc scatteredRay)
	{
		float3 direction = si.bsdf.normal + RandomUnitVector(rayPayload.Seed);
		RayDesc ray = { si.position, 0.0001f, direction, 100000.0f };
		
		attenuation = Albedo;
		scatteredRay = ray;
		return true;
	}
};

struct Glossy
{
	float3 Albedo;
	float SpecularChance;
	float Roughness;
	float3 Specular;
	
	bool Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out float3 attenuation, out RayDesc scatteredRay)
	{
		float doSpecular = RandomFloat01(rayPayload.Seed) < SpecularChance ? 1.0f : 0.0f;
		
		float3 diffuse = si.bsdf.normal + RandomUnitVector(rayPayload.Seed);
		float3 reflected = reflect(WorldRayDirection(), si.bsdf.normal);
		reflected = normalize(lerp(reflected, diffuse, Roughness * Roughness));
		
		float3 direction = lerp(diffuse, reflected, doSpecular);
		RayDesc ray = { si.position, 0.0001f, direction, 100000.0f };
		
		attenuation = lerp(Albedo, Specular, doSpecular);
		scatteredRay = ray;
		return true;
	}
};

struct Metal
{
	float3 Albedo;
	float Fuzziness;
	
	bool Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out float3 attenuation, out RayDesc scatteredRay)
	{
		Fuzziness = Fuzziness < 1.0f ? Fuzziness : 1.0f;
		
		float3 reflected = reflect(WorldRayDirection(), si.bsdf.normal) + Fuzziness * RandomInUnitSphere(rayPayload.Seed);
		RayDesc ray = { si.position, 0.0001f, reflected, 100000.0f };
		
		attenuation = Albedo;
		scatteredRay = ray;
		return dot(ray.Direction, si.bsdf.normal) > 0.0f;
	}
};

struct Dielectric
{
	float IndexOfRefraction;
	float3 Specular;
	float3 Refraction;
	
	bool Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out float3 attenuation, out RayDesc scatteredRay)
	{
		attenuation = float3(1.0f, 1.0f, 1.0f);
		RayDesc ray = { si.position, 0.0001f, float3(0.0f, 0.0f, 0.0f), 100000.0f };
		
		si.bsdf.normal = si.frontFace ? si.bsdf.normal : -si.bsdf.normal;
		float etaI_Over_etaT = si.frontFace ? (1.0f / IndexOfRefraction) : IndexOfRefraction;
	
		float cosTheta = min(dot(-WorldRayDirection(), si.bsdf.normal), 1.0f);
		float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
	
		// Total internal reflection
		if ((etaI_Over_etaT * sinTheta) > 1.0f ||
			RandomFloat01(rayPayload.Seed) < Fresnel_Schlick(etaI_Over_etaT, cosTheta))
		{
			float3 reflected = reflect(WorldRayDirection(), si.bsdf.normal);
			ray.Direction = reflected;
			attenuation = Specular;
		}
		else
		{
			float3 refracted = refract(WorldRayDirection(), si.bsdf.normal, etaI_Over_etaT);
			ray.Direction = refracted;
			// do absorption if we are hitting from inside the object
			if (!si.frontFace)
			{
				attenuation *= exp(-Refraction * RayTCurrent());
			}
		}
		
		scatteredRay = ray;
		return true;
	}
};

struct DiffuseLight
{
	bool Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out float3 attenuation, out RayDesc scatteredRay)
	{
		// Need to provide initialized value here or DXC complains
		RayDesc ray = { float3(0.0f, 0.0f, 0.0f), 0.0f, float3(0.0f, 0.0f, 0.0f), 0.0f };

		attenuation = float3(1.0f, 1.0f, 1.0f);
		scatteredRay = ray;
		return false;
	}
};

[shader("raygeneration")]
void RayGeneration()
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 launchDimensions = DispatchRaysDimensions().xy;
	uint seed = uint(launchIndex.x * uint(1973) + launchIndex.y * uint(9277) + uint(RenderPassData.GlobalConstants.TotalFrameCount) * uint(26699)) | uint(1);
	
	float3 color = float3(0.0f, 0.0f, 0.0f);
	for (int sample = 0; sample < RenderPassData.GlobalConstants.NumSamplesPerPixel; ++sample)
	{
		// Calculate subpixel camera jitter for anti aliasing
		const float2 jitter = float2(RandomFloat01(seed), RandomFloat01(seed)) - 0.5f;
		const float2 pixel = (float2(launchIndex) + jitter) / float2(launchDimensions); // Convert from discrete to continuous

		const float2 ndc = float2(2, -2) * pixel + float2(-1, 1);

		// Initialize ray
		RayDesc ray = GenerateCameraRay(ndc, seed, RenderPassData.GlobalConstants);
		// Initialize payload
		RayPayload rayPayload = { float3(0.0f, 0.0f, 0.0f), float3(1.0f, 1.0f, 1.0f), seed, 0 };
		// Trace the ray
		const uint flags = RAY_FLAG_NONE;
		const uint mask = 0xffffffff;
		const uint hitGroupIndex = RayTypePrimary;
		const uint hitGroupIndexMultiplier = NumRayTypes;
		const uint missShaderIndex = RayTypePrimary;
		TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, rayPayload);
	
		color += rayPayload.Radiance;
	}
	float scale = 1.0f / (float)RenderPassData.GlobalConstants.NumSamplesPerPixel;
	color *= scale;

	RWTexture2D<float4> RenderTarget = RWTexture2DTable[RenderPassData.OutputIndex];
	RenderTarget[launchIndex] = float4(color, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload rayPayload)
{
	rayPayload.Radiance += TextureCubeTable[RenderPassData.GlobalConstants.SkyboxIndex].SampleLevel(SamplerLinearWrap, WorldRayDirection(), 0.0f).rgb * rayPayload.Throughput;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload rayPayload, in HitAttributes attrib)
{
	SurfaceInteraction si = GetSurfaceInteraction(attrib);
	
	float3 attentuation;
	RayDesc ray;
	bool shouldScatter;
	switch (si.material.Model)
	{
		case LambertianModel:
		{
			Lambertian lambertian = { si.material.Albedo };
			shouldScatter = lambertian.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		case GlossyModel:
		{
			Glossy glossy = { si.material.Albedo, si.material.SpecularChance, si.material.Roughness, si.material.Specular };
			shouldScatter = glossy.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		case MetalModel:
		{
			Metal metal = { si.material.Albedo, si.material.Fuzziness };
			shouldScatter = metal.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		case DielectricModel:
		{
			Dielectric dielectric = { si.material.IndexOfRefraction, si.material.Specular, si.material.Refraction };
			shouldScatter = dielectric.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		case DiffuseLightModel:
		{
			DiffuseLight diffuseLight;
			shouldScatter = diffuseLight.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		default:
		{
			rayPayload.Radiance = float3(5000.0f, 0.0f, 0.0f);
			return;
		}
		break;
	}
	
	rayPayload.Radiance += si.material.Emissive * rayPayload.Throughput;
	rayPayload.Throughput *= attentuation;
	
	// Path trace
	if (shouldScatter &&
		rayPayload.Depth + 1 < RenderPassData.GlobalConstants.MaxDepth)
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