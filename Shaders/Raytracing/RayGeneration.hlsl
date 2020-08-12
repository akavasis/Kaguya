#include "../HLSLCommon.hlsli"
#include "Common.hlsl"

RaytracingAccelerationStructure SceneBVH : register(t0, space0);
RWTexture2D<float4> RenderTarget : register(u0, space0);
ConstantBuffer<RenderPassConstants> RenderPassDataCB : register(b0, space0);

[shader("raygeneration")] 
void RayGen()
{
  uint2 launchIndex = DispatchRaysIndex().xy;
  uint2 launchDimensions = DispatchRaysDimensions().xy;

  // Initialize the ray payload
	RayPayload payload;
	payload.colorAndDistance = float4(0.0f, 0.0f, 0.0f, 0.0f);

  float2 idx = float2(launchIndex);
	float2 dims = float2(launchDimensions);
	float2 d = (idx / dims) * 2.0f - 1.0f;
    
  // Transform from screen space to world space
	RayDesc ray;
	ray.Origin = mul(float4(0.0f, 0.0f, 0.0f, 1.0f), RenderPassDataCB.InvView).xyz;
	float4 target = mul(float4(d.x, -d.y, 1.0f, 1.0f), RenderPassDataCB.InvProjection);
	ray.Direction = mul(float4(target.xyz, 0.0f), RenderPassDataCB.InvView).xyz;
	ray.TMin = 0.001f;
	ray.TMax = 100000.0f;
    
  // Trace the ray
	TraceRay(SceneBVH, RAY_FLAG_NONE, 0xFF, 0, 0, 0, ray, payload);

	RenderTarget[launchIndex] = float4(payload.colorAndDistance.rgb, 1.f);
}