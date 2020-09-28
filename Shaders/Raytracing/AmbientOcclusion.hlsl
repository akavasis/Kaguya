#include "Global.hlsli"

struct AORayPayload
{
	float Visibility;
};

enum RayType
{
	RayTypePrimary,
    NumRayTypes
};

static const uint NumAORays = 10;
static const float AORadius = 1.0f;

RWTexture2D<float4> RenderTarget : register(u0, space0);
Texture2D<float4> WorldPosition : register(t0, space5);
Texture2D<float4> WorldNormal : register(t1, space5);

float TraceAmbientOcclusionRay(float3 origin, float tMin, float3 direction, float tMax)
{
	AORayPayload aoRayPayload = { 0.0f };
	RayDesc ray = { origin, tMin, direction, tMax };
	
	const uint flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
	const uint mask = 0xffffffff;
	const uint hitGroupIndex = RayTypePrimary;
	const uint hitGroupIndexMultiplier = NumRayTypes;
	const uint missShaderIndex = RayTypePrimary;
	TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, aoRayPayload);

	return aoRayPayload.Visibility;
}

[shader("raygeneration")]
void RayGeneration()
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 launchDimensions = DispatchRaysDimensions().xy;
	uint seed = uint(launchIndex.x * uint(1973) + launchIndex.y * uint(9277) + uint(RenderPassDataCB.TotalFrameCount) * uint(26699)) | uint(1);
	
	float4 worldPosition = WorldPosition[launchIndex];
	float4 worldNormal = WorldNormal[launchIndex];
	
	float occlusionSum = 0.0f;
	
	if (worldPosition.z != 0.0f)
	{
		for (uint i = 0; i < NumAORays; ++i)
		{
			float3 worldDirection = CosHemisphereSample(seed, worldNormal.xyz);
			occlusionSum += TraceAmbientOcclusionRay(worldPosition.xyz, 0.001f, worldDirection.xyz, AORadius);
		}
	}
	
	occlusionSum /= NumAORays;
	
	float access = 1.0f - occlusionSum;
	RenderTarget[launchIndex] = float4(access, access, access, 1.0f);
}

[shader("miss")]
void Miss(inout AORayPayload rayPayload)
{
	rayPayload.Visibility = 1.0f;
}