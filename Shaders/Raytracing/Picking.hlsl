struct RenderPassData
{
    uint2 MousePosition;
};
#define RenderPassDataType RenderPassData
#include "Global.hlsli"

struct RayPayload
{
    int InstanceID;
};

enum RayType
{
	RayTypePrimary,
    NumRayTypes
};

// Local Root Signature
// ====================
RWStructuredBuffer<int> PickingResult : register(u0, space0);

[shader("raygeneration")]
void RayGeneration()
{
    uint2 pixelIdx = g_RenderPassData.MousePosition;
    float2 uv = (pixelIdx + float2(0.5f, 0.5f)) / g_SystemConstants.OutputSize.xy;

    RayDesc ray = 
    { 
        g_SystemConstants.Camera.Position.xyz, 
        g_SystemConstants.Camera.NearZ, 
        GenerateWorldCameraRayDirection(uv, g_SystemConstants.Camera), 
        g_SystemConstants.Camera.FarZ 
    };

    RayPayload rayPayload = { -1 };

    const uint flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
	const uint mask = 0xffffffff;
	const uint hitGroupIndex = RayTypePrimary;
	const uint hitGroupIndexMultiplier = NumRayTypes;
	const uint missShaderIndex = RayTypePrimary;
	TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, rayPayload);

    PickingResult[0] = rayPayload.InstanceID;
}

[shader("miss")]
void Miss(inout RayPayload rayPayload)
{

}

[shader("closesthit")]
void ClosestHit(inout RayPayload rayPayload, in HitAttributes attrib)
{
    rayPayload.InstanceID = InstanceID();
}