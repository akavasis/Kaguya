#include "Global.hlsli"

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

// Hit information, aka ray payload
// Note that the payload should be kept as small as possible,
// and that its size must be declared in the corresponding
// D3D12_RAYTRACING_SHADER_CONFIG pipeline subobjet.
struct RayPayload
{
	bool Dummy;
};

enum RayType
{
	RayTypePrimary,
    NumRayTypes
};

RWTexture2D<float4> RenderTargets[NumGBufferTypes] : register(u0, space0);

[shader("raygeneration")]
void RayGeneration()
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 launchDimensions = DispatchRaysDimensions().xy;
	
	const float2 pixel = (float2(launchIndex) + 0.5f) / float2(launchDimensions);
	const float2 ndc = float2(2, -2) * pixel + float2(-1, 1);
	
	// Initialize ray
	RayDesc ray = GenerateCameraRay(ndc, seed);
	// Initialize payload
	RayPayload rayPayload = { false };
	// Trace the ray
	const uint flags = RAY_FLAG_NONE;
	const uint mask = 0xffffffff;
	const uint hitGroupIndex = RayTypePrimary;
	const uint hitGroupIndexMultiplier = NumRayTypes;
	const uint missShaderIndex = RayTypePrimary;
	TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, rayPayload);
}

[shader("miss")]
void Miss(inout RayPayload rayPayload)
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	
	RenderTargets[WorldPosition][launchIndex] = float4(0.0f, 0.0f, 0.0f, 0.0f);
	RenderTargets[WorldNormal][launchIndex] = float4(0.0f, 0.0f, 0.0f, 0.0f);
	RenderTargets[MaterialAlbedo][launchIndex] = TexCubeTable[RenderPassDataCB.RadianceCubemapIndex].SampleLevel(SamplerLinearWrap, WorldRayDirection(), 0.0f);
	RenderTargets[MaterialEmissive][launchIndex] = float4(0.0f, 0.0f, 0.0f, 0.0f);
	RenderTargets[MaterialSpecular][launchIndex] = float4(0.0f, 0.0f, 0.0f, 0.0f);
	RenderTargets[MaterialRefraction][launchIndex] = float4(0.0f, 0.0f, 0.0f, 0.0f);
	RenderTargets[MaterialExtra][launchIndex] = float4(0.0f, 0.0f, 0.0f, 0.0f);
}

[shader("closesthit")]
void ClosestHit(inout RayPayload rayPayload, in HitAttributes attrib)
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	SurfaceInteraction si = GetSurfaceInteraction(attrib);
	
	RenderTargets[WorldPosition][launchIndex] = float4(si.position, 1.0f);
	RenderTargets[WorldNormal][launchIndex] = float4(si.bsdf.normal, 0.0f);
	RenderTargets[MaterialAlbedo][launchIndex] = float4(si.material.Albedo, si.material.SpecularChance);
	RenderTargets[MaterialEmissive][launchIndex] = float4(si.material.Emissive, si.material.Roughness);
	RenderTargets[MaterialSpecular][launchIndex] = float4(si.material.Specular, si.material.Fuzziness);
	RenderTargets[MaterialRefraction][launchIndex] = float4(si.material.Refraction, si.material.IndexOfRefraction);
	RenderTargets[MaterialExtra][launchIndex] = float4(si.material.Model, 0.0f, 0.0f, 0.0f);
}