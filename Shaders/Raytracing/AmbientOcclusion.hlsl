struct AmbientOcclusionData
{
	float AORadius;
	int NumAORaysPerPixel;

	int InputWorldPositionIndex;
	int InputWorldNormalIndex;
	int OutputIndex;
};
#define RenderPassDataType AmbientOcclusionData
#include "Global.hlsli"

struct RayPayload
{
	float Visibility;
};

enum RayType
{
	RayTypePrimary,
    NumRayTypes
};

float TraceAmbientOcclusionRay(float3 origin, float tMin, float3 direction, float tMax)
{
	RayPayload rayPayload = { 0.0f };
	RayDesc ray = { origin, tMin, direction, tMax };
	
	const uint flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH | RAY_FLAG_SKIP_CLOSEST_HIT_SHADER;
	const uint mask = 0xffffffff;
	const uint hitGroupIndex = RayTypePrimary;
	const uint hitGroupIndexMultiplier = NumRayTypes;
	const uint missShaderIndex = RayTypePrimary;
	TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, rayPayload);

	return rayPayload.Visibility;
}

[shader("raygeneration")]
void RayGeneration()
{
	const uint2 launchIndex = DispatchRaysIndex().xy;
	const uint2 launchDimensions = DispatchRaysDimensions().xy;
	uint seed = uint(launchIndex.x * uint(1973) + launchIndex.y * uint(9277) + uint(SystemConstants.TotalFrameCount) * uint(26699)) | uint(1);
	
	Texture2D WorldPosition = Texture2DTable[RenderPassData.InputWorldPositionIndex];
	Texture2D WorldNormal = Texture2DTable[RenderPassData.InputWorldNormalIndex];

	float4 worldPosition = WorldPosition[launchIndex];
	float4 worldNormal = WorldNormal[launchIndex];
	
	float occlusionSum = 0.0f;
	
	if (worldPosition.z != 0.0f)
	{
		for (uint i = 0; i < RenderPassData.NumAORaysPerPixel; ++i)
		{
			float3 worldDirection = CosHemisphereSample(seed, worldNormal.xyz);
			occlusionSum += TraceAmbientOcclusionRay(worldPosition.xyz, 0.001f, worldDirection.xyz, RenderPassData.AORadius);
		}
	}
	
	occlusionSum /= RenderPassData.NumAORaysPerPixel;
	
	RWTexture2D<float4> RenderTarget = RWTexture2DTable[RenderPassData.OutputIndex];
	RenderTarget[launchIndex] = float4(occlusionSum, occlusionSum, occlusionSum, 1.0f);
}

[shader("miss")]
void Miss(inout RayPayload rayPayload)
{
	rayPayload.Visibility = 1.0f;
}

[shader("closesthit")]
void ClosestHit(inout RayPayload rayPayload, in HitAttributes attrib)
{
}