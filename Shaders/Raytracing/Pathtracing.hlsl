#include "Global.hlsli"

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

struct Glossy
{
	float3 Albedo;
	float SpecularChance;
	float Roughness;
	float3 Specular;
	
	void Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out float3 attenuation, out RayDesc scatteredRay)
	{
		float doSpecular = RandomFloat01(rayPayload.Seed) < SpecularChance ? 1.0f : 0.0f;
		
		float3 diffuse = si.bsdf.normal + RandomUnitVector(rayPayload.Seed);
		float3 reflected = reflect(WorldRayDirection(), si.bsdf.normal);
		reflected = normalize(lerp(reflected, diffuse, Roughness * Roughness));
		
		float3 direction = lerp(diffuse, reflected, doSpecular);
		RayDesc ray = { si.position, 0.00001f, direction, 100000.0f };
		
		attenuation = lerp(Albedo, Specular, doSpecular);
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
	float3 Specular;
	float3 Refraction;
	
	void Scatter(in SurfaceInteraction si, inout RayPayload rayPayload, out float3 attenuation, out RayDesc scatteredRay)
	{
		attenuation = float3(1.0f, 1.0f, 1.0f);
		RayDesc ray = { si.position, 0.00001f, float3(0.0f, 0.0f, 0.0f), 100000.0f };
		
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

RWTexture2D<float4> RenderTarget : register(u0, space0);

[shader("raygeneration")]
void RayGeneration()
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 launchDimensions = DispatchRaysDimensions().xy;
	uint seed = uint(launchIndex.x * uint(1973) + launchIndex.y * uint(9277) + uint(RenderPassDataCB.TotalFrameCount) * uint(26699)) | uint(1);
	
	// Calculate subpixel camera jitter for anti aliasing
	const float2 jitter = float2(RandomFloat01(seed), RandomFloat01(seed)) - 0.5f;
	const float2 pixel = (float2(launchIndex) + 0.5f + jitter) / float2(launchDimensions); // Convert from discrete to continuous
	
	const float2 ndc = float2(2, -2) * pixel + float2(-1, 1);
	
	float3 direction = ndc.x * RenderPassDataCB.CameraU + ndc.y * RenderPassDataCB.CameraV + RenderPassDataCB.CameraW;
	
	// Find the focal point for this pixel
	direction /= length(RenderPassDataCB.CameraW); // Make ray have length 1 along the camera's w-axis.
	float3 focalPoint = RenderPassDataCB.EyePosition + RenderPassDataCB.FocalLength * direction; // Select point on ray a distance FocalLength along the w-axis
	
	// Get random numbers (in polar coordinates), convert to random cartesian uv on the lens
	float2 rnd = float2(s_2PI * RandomFloat01(seed), RenderPassDataCB.LensRadius * RandomFloat01(seed));
	float2 uv = float2(cos(rnd.x) * rnd.y, sin(rnd.x) * rnd.y);

	// Use uv coordinate to compute a random origin on the camera lens
	float3 origin = RenderPassDataCB.EyePosition + uv.x * normalize(RenderPassDataCB.CameraU) + uv.y * normalize(RenderPassDataCB.CameraV);
	direction = normalize(focalPoint - origin);
	
	// Initialize ray
	RayDesc ray = { origin, 0.0f, direction, 1e+38f };
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
	//rayPayload.Radiance += TexCubeTable[RenderPassDataCB.RadianceCubemapIndex].SampleLevel(SamplerLinearWrap, WorldRayDirection(), 0.0f).rgb * rayPayload.Throughput;
}

[shader("miss")]
void ShadowMiss(inout ShadowRayPayload rayPayload)
{
	rayPayload.Visibility = 1.0f;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload rayPayload, in HitAttributes attrib)
{
	SurfaceInteraction si = GetSurfaceInteraction(attrib);
	
	float3 attentuation;
	RayDesc ray;
	switch (si.material.Model)
	{
		case LambertianModel:
		{
			Lambertian lambertian = { si.material.Albedo };
			lambertian.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		case GlossyModel:
		{
			Glossy glossy = { si.material.Albedo, si.material.SpecularChance, si.material.Roughness, si.material.Specular };
			glossy.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		case MetalModel:
		{
			Metal metal = { si.material.Albedo, si.material.Fuzziness };
			metal.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		case DielectricModel:
		{
			Dielectric dielectric = { si.material.IndexOfRefraction, si.material.Specular, si.material.Refraction };
			dielectric.Scatter(si, rayPayload, attentuation, ray);
		}
		break;
		
		case DiffuseLightModel:
		{
			DiffuseLight diffuseLight;
			diffuseLight.Scatter(si, rayPayload, attentuation, ray);
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