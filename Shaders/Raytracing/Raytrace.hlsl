#include "Global.hlsli"

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
	const float2 pixel = float2(launchIndex) / float2(launchDimensions);
	float2 ndcCoord = pixel * 2.0f - 1.0f; // Uncompress each component from [0,1] to [-1,1].
	ndcCoord.y *= -1.0f;
  
  	// Initialize the ray payload
	RayPayload payload;
	payload.colorAndDistance = float4(0.0f, 0.0f, 0.0f, 0.0f);
    
  	// Transform from screen space to projection space
	RayDesc ray;
	ray.Origin = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), RenderPassDataCB.InvView).xyz;
	float4 target = mul(float4(ndcCoord, 1.0f, 1.0f), RenderPassDataCB.InvProjection);
	ray.Direction = mul(float4(target.xyz, 0.0f), RenderPassDataCB.InvView).xyz;
	ray.TMin = 0.001f;
	ray.TMax = 100000.0f;
    
  	// Trace the ray
	const uint flags = RAY_FLAG_NONE;
	const uint mask = 0xffffffff;
	const uint hitGroupIndex = RayTypePrimary;
	const uint hitGroupIndexMultiplier = NumRayTypes;
	const uint missShaderIndex = RayTypePrimary;
	TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, payload);

	RenderTarget[launchIndex] = float4(payload.colorAndDistance.rgb, 1.0f);
}

// This shader will be executed when no geometry is hit, and will write a constant color in the payload. 
// Note that this shader takes the payload as a inout parameter. It will be provided to the shader automatically by DXR.
[shader("miss")]
void Miss(inout RayPayload payload)
{
	uint2 launchIndex = DispatchRaysIndex().xy;
	float2 dims = float2(DispatchRaysDimensions().xy);

	float ramp = launchIndex.y / dims.y;
	payload.colorAndDistance = float4(0.0f, 0.2f, 0.7f - 0.3f * ramp, -1.0f);
}

// It will be executed upon hitting the geometry (our triangle). 
// As the miss shader, it takes the ray payload payload as a inout parameter. 
// It also has a second parameter defining the intersection attributes as provided by the intersection shader, 
// ie. the barycentric coordinates. This shader simply writes a constant color to the payload, as well as the distance from the origin, 
// provided by the built-in RayCurrentT() function.
[shader("closesthit")]
void ClosestHit(inout RayPayload payload, in HitAttributes attrib)
{
	float3 barycentrics =
    float3(1.f - attrib.barycentrics.x - attrib.barycentrics.y, attrib.barycentrics.x, attrib.barycentrics.y);

	const float3 A = float3(1, 0, 0);
	const float3 B = float3(0, 1, 0);
	const float3 C = float3(0, 0, 1);

	float3 hitColor = float3(0.7, 0.7, 0.7);
	switch (InstanceID())
	{
		case 0:
			hitColor = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
			break;
		case 1:
			hitColor = B * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
			break;
		case 2:
			hitColor = C * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
			break;
	}

	// Initialize the ray payload
	ShadowRayPayload shadowRayPayload;
	shadowRayPayload.visibility = 1.0f;

	// Shoot a shadow ray
	const float3 incomingRayOriginWS = WorldRayOrigin();
    const float3 incomingRayDirWS = WorldRayDirection();

	RayDesc ray;
	ray.Origin = incomingRayOriginWS + incomingRayDirWS * RayTCurrent();
	ray.Direction = -RenderPassDataCB.Sun.Direction;
	ray.TMin = 0.001f;
	ray.TMax = 100000.0f;

	const uint flags = RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH;
	const uint mask = 0xffffffff;
	const uint hitGroupIndex = RayTypeShadow;
	const uint hitGroupIndexMultiplier = NumRayTypes;
	const uint missShaderIndex = RayTypeShadow;
	TraceRay(SceneBVH, flags, mask, hitGroupIndex, hitGroupIndexMultiplier, missShaderIndex, ray, shadowRayPayload);

	hitColor *= shadowRayPayload.visibility;
	payload.colorAndDistance = float4(hitColor, RayTCurrent());
}

[shader("miss")]
void ShadowMiss(inout ShadowRayPayload payload)
{
	payload.visibility = 1.0f;
}

[shader("closesthit")]
void ShadowClosestHit(inout ShadowRayPayload payload, in HitAttributes attrib)
{
	payload.visibility = 0.0f;
}