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

struct RaytraceGBufferData
{
	GlobalConstants GlobalConstants;
	int OutputWorldPositionIndex;
	int OutputWorldNormalIndex;
	int OutputMaterialAlbedoIndex;
	int OutputMaterialEmissiveIndex;
	int OutputMaterialSpecularIndex;
	int OutputMaterialRefractionIndex;
	int OutputMaterialExtraIndex;
};

#define RenderPassDataType RaytraceGBufferData
#include "../ShaderLayout.hlsli"

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

[shader("raygeneration")]
void RayGeneration()
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 launchDimensions = DispatchRaysDimensions().xy;
	uint seed = uint(launchIndex.x * uint(1973) + launchIndex.y * uint(9277) + uint(RenderPassData.GlobalConstants.TotalFrameCount) * uint(26699)) | uint(1);
	
	const float2 pixel = (float2(launchIndex) + 0.5f) / float2(launchDimensions);
	const float2 ndc = float2(2, -2) * pixel + float2(-1, 1);
	
	// Initialize ray
	RayDesc ray = GenerateCameraRay(ndc, seed, RenderPassData.GlobalConstants);
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

	RWTexture2D<float4> RenderTargets[NumGBufferTypes];
	RenderTargets[WorldPosition] = RWTexture2DTable[RenderPassData.OutputWorldPositionIndex];
	RenderTargets[WorldNormal] = RWTexture2DTable[RenderPassData.OutputWorldNormalIndex];
	RenderTargets[MaterialAlbedo] = RWTexture2DTable[RenderPassData.OutputMaterialAlbedoIndex];
	RenderTargets[MaterialEmissive] = RWTexture2DTable[RenderPassData.OutputMaterialEmissiveIndex];
	RenderTargets[MaterialSpecular] = RWTexture2DTable[RenderPassData.OutputMaterialSpecularIndex];
	RenderTargets[MaterialRefraction] = RWTexture2DTable[RenderPassData.OutputMaterialRefractionIndex];
	RenderTargets[MaterialExtra] = RWTexture2DTable[RenderPassData.OutputMaterialExtraIndex];
	
	RenderTargets[WorldPosition][launchIndex] = float4(0.0f, 0.0f, 0.0f, 0.0f);
	RenderTargets[WorldNormal][launchIndex] = float4(0.0f, 0.0f, 0.0f, 0.0f);
	RenderTargets[MaterialAlbedo][launchIndex] = float4(0.0f, 0.0f, 0.0f, 0.0f);
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
	
	RWTexture2D<float4> RenderTargets[NumGBufferTypes];
	RenderTargets[WorldPosition] = RWTexture2DTable[RenderPassData.OutputWorldPositionIndex];
	RenderTargets[WorldNormal] = RWTexture2DTable[RenderPassData.OutputWorldNormalIndex];
	RenderTargets[MaterialAlbedo] = RWTexture2DTable[RenderPassData.OutputMaterialAlbedoIndex];
	RenderTargets[MaterialEmissive] = RWTexture2DTable[RenderPassData.OutputMaterialEmissiveIndex];
	RenderTargets[MaterialSpecular] = RWTexture2DTable[RenderPassData.OutputMaterialSpecularIndex];
	RenderTargets[MaterialRefraction] = RWTexture2DTable[RenderPassData.OutputMaterialRefractionIndex];
	RenderTargets[MaterialExtra] = RWTexture2DTable[RenderPassData.OutputMaterialExtraIndex];

	RenderTargets[WorldPosition][launchIndex] = float4(si.position, 1.0f);
	RenderTargets[WorldNormal][launchIndex] = float4(si.bsdf.normal, 0.0f);
	RenderTargets[MaterialAlbedo][launchIndex] = float4(si.material.Albedo, si.material.SpecularChance);
	RenderTargets[MaterialEmissive][launchIndex] = float4(si.material.Emissive, si.material.Roughness);
	RenderTargets[MaterialSpecular][launchIndex] = float4(si.material.Specular, si.material.Fuzziness);
	RenderTargets[MaterialRefraction][launchIndex] = float4(si.material.Refraction, si.material.IndexOfRefraction);
	RenderTargets[MaterialExtra][launchIndex] = float4(si.material.Model, 0.0f, 0.0f, 0.0f);
}