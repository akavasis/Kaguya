#include "Global.hlsli"

// HitGroup Local Root Signature
// ====================
cbuffer RootConstants : register(b0, space1)
{
	uint MaterialIndex;
};

StructuredBuffer<Vertex> VertexBuffer : register(t0, space1);
StructuredBuffer<uint> IndexBuffer : register(t1, space1);

SurfaceInteraction GetSurfaceInteraction(in BuiltInTriangleIntersectionAttributes attrib)
{
	// Fetch indices
	unsigned int primIndex = PrimitiveIndex();
	unsigned int idx0 = IndexBuffer[primIndex * 3 + 0];
	unsigned int idx1 = IndexBuffer[primIndex * 3 + 1];
	unsigned int idx2 = IndexBuffer[primIndex * 3 + 2];
	
	// Fetch vertices
	Vertex vtx0 = VertexBuffer[idx0];
	Vertex vtx1 = VertexBuffer[idx1];
	Vertex vtx2 = VertexBuffer[idx2];
	
	float3 p0 = vtx0.Position, p1 = vtx1.Position, p2 = vtx2.Position;
	// Compute 2 edges of the triangle
	float3 e0 = p1 - p0;
	float3 e1 = p2 - p0;
	float3 n = normalize(cross(e0, e1));
	
	float3 barycentrics = float3(1.f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);
	Vertex vertex = BarycentricInterpolation(vtx0, vtx1, vtx2, barycentrics);
	
	SurfaceInteraction si;
	si.p = WorldRayOrigin() + (WorldRayDirection() * RayTCurrent());
	si.wo = -WorldRayDirection();
	si.n = n;
	si.uv = vertex.TextureCoordinate;
	
	// Compute geometry basis and shading basis
	si.GeometryFrame = InitFrame(n);
	si.ShadingFrame = InitFrame(normalize(vertex.Normal));
	
	// Update BSDF's internal data
	si.BSDF.Ng = si.GeometryFrame.n;
	si.BSDF.ShadingFrame = si.ShadingFrame;
	
	Material material = g_Materials[MaterialIndex];
	si.BSDF.BxDF.baseColor = material.baseColor;
	si.BSDF.BxDF.metallic = material.metallic;
	si.BSDF.BxDF.subsurface = material.subsurface;
	si.BSDF.BxDF.specular = material.specular;
	si.BSDF.BxDF.roughness = material.roughness;
	si.BSDF.BxDF.specularTint = material.specularTint;
	si.BSDF.BxDF.anisotropic = material.anisotropic;
	si.BSDF.BxDF.sheen = material.sheen;
	si.BSDF.BxDF.sheenTint = material.sheenTint;
	si.BSDF.BxDF.clearcoat = material.clearcoat;
	si.BSDF.BxDF.clearcoatGloss = material.clearcoatGloss;
	
	
	si.Material = material;
	
	return si;
}

struct RayPayload
{
	float3 L;
	float3 beta;
	float3 Position;
	float3 Direction;
	uint Seed;
	uint Depth;
};

struct ShadowRayPayload
{
	float Visibility; // 1.0f for fully lit, 0.0f for fully shadowed
};

enum RayType
{
	RayTypePrimary,
    NumRayTypes
};

float TraceShadowRay(RayDesc Ray)
{
	ShadowRayPayload RayPayload = { 0.0f };
	
	// Trace the ray
	const uint RayFlags = RAY_FLAG_FORCE_OPAQUE
				| RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH
				| RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
	const uint InstanceInclusionMask = 0xffffffff;
	const uint RayContributionToHitGroupIndex = RayTypePrimary;
	const uint MultiplierForGeometryContributionToHitGroupIndex = NumRayTypes;
	const uint MissShaderIndex = 1;
	TraceRay(g_Scene,
			RayFlags,
			InstanceInclusionMask,
			RayContributionToHitGroupIndex,
			MultiplierForGeometryContributionToHitGroupIndex,
			MissShaderIndex,
			Ray,
			RayPayload);

	return RayPayload.Visibility;
}

float3 EstimateDirect(SurfaceInteraction si, Light light, float2 XiLight)
{
	float3 Ld = float3(0.0f, 0.0f, 0.0f);
	
	// Sample light source with multiple importance sampling
	float3 wi;
	float lightPdf = 0.0f, scatteringPdf = 0.0f;
	VisibilityTester visibilityTester;
	
	float3 Li = SampleLi(light, si, XiLight, wi, lightPdf, visibilityTester);
	if (lightPdf > 0.0f && any(Li))
	{
		// Compute BSDF's value for light sample
		// Evaluate BSDF for light sampling strategy
		float3 f = si.BSDF.f(si.wo, wi) * abs(dot(wi, si.ShadingFrame.n));
		scatteringPdf = si.BSDF.Pdf(si.wo, wi);

		float visibility = TraceShadowRay(visibilityTester.I0.SpawnRayTo(visibilityTester.I1));
		
		// Add light's contribution to reflected radiance
		// Only handling point light for now
		Ld += f * Li * visibility / lightPdf;
	}

	return Ld;
}

float3 UniformSampleOneLight(SurfaceInteraction si, inout uint seed)
{
	if (g_SystemConstants.NumLights == 0)
	{
		return float3(0.0f, 0.0f, 0.0f);
	}
	
	int numLights = g_SystemConstants.NumLights;

	int lightIndex = min(RandomFloat01(seed) * numLights, numLights-1);
	float lightPdf = 1.0f / float(numLights);

	Light light = g_Lights[lightIndex];

	float2 Xi = float2(RandomFloat01(seed), RandomFloat01(seed));

	return EstimateDirect(si, light, Xi) / lightPdf;
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
		const uint RayFlags = RAY_FLAG_NONE;
		const uint InstanceInclusionMask = 0xffffffff;
		const uint RayContributionToHitGroupIndex = RayTypePrimary;
		const uint MultiplierForGeometryContributionToHitGroupIndex = NumRayTypes;
		const uint MissShaderIndex = RayTypePrimary;
		TraceRay(g_Scene,
			RayFlags,
			InstanceInclusionMask, 
			RayContributionToHitGroupIndex, 
			MultiplierForGeometryContributionToHitGroupIndex, 
			MissShaderIndex, 
			Ray, 
			RayPayload);
		
		Ray.Origin = RayPayload.Position;
		Ray.TMin = 0.0001f; // Avoid self intersection
		Ray.Direction = RayPayload.Direction;
		
		RayPayload.Depth++;
	}

	return RayPayload.L;
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
		RayDesc ray = g_SystemConstants.Camera.GenerateCameraRay(ndc, seed);
	
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
void Miss(inout RayPayload rayPayload : SV_RayPayload)
{
	TerminateRay(rayPayload);
}

[shader("miss")]
void ShadowMiss(inout ShadowRayPayload rayPayload : SV_RayPayload)
{
	// If we miss all geometry, then the light is visible
	rayPayload.Visibility = 1.0f;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload rayPayload, in BuiltInTriangleIntersectionAttributes attrib)
{
	SurfaceInteraction si = GetSurfaceInteraction(attrib);
	
	rayPayload.L += rayPayload.beta * UniformSampleOneLight(si, rayPayload.Seed);
	
	// Sample BSDF to get new path direction
	float3 wo = -rayPayload.Direction;
	BSDFSample bsdfSample = (BSDFSample)0;
	bool success = si.BSDF.Samplef(wo, float2(RandomFloat01(rayPayload.Seed), RandomFloat01(rayPayload.Seed)), bsdfSample);
	if (!success)
	{
		TerminateRay(rayPayload);
		return;
	}
	
	rayPayload.beta *= bsdfSample.f * dot(bsdfSample.wi, si.ShadingFrame.n) / bsdfSample.pdf;
	
	// Spawn new ray
	rayPayload.Position = WorldRayOrigin();
	rayPayload.Direction = normalize(bsdfSample.wi);
	
	const float rrThreshold = 1.0f;
	float3 rr = rayPayload.beta;
	float rrMaxComponentValue = max(rr.x, max(rr.y, rr.z));
	if (rrMaxComponentValue < rrThreshold && rayPayload.Depth > 3)
	{
		float q = max(0.05f, 1.0f - rrMaxComponentValue);
		if (RandomFloat01(rayPayload.Seed) < q)
		{
			TerminateRay(rayPayload);
		}
		rayPayload.beta /= 1.0f - q;
	}
}