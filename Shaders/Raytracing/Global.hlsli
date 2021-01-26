#ifndef GLOBAL_HLSLI
#define GLOBAL_HLSLI

// This file defines global root signature for raytracing shaders
#include <HLSLCommon.hlsli>

#define RAYTRACING_INSTANCEMASK_ALL 	(0xff)
#define RAYTRACING_INSTANCEMASK_OPAQUE 	(1 << 0)
#define RAYTRACING_INSTANCEMASK_LIGHT	(1 << 1)

struct SystemConstants
{
	Camera Camera;
	
	// x, y = Resolution
	// z, w = 1 / Resolution
	float4 Resolution;

	uint TotalFrameCount;
};

struct RenderPassData
{
	uint NumSamplesPerPixel;
	uint MaxDepth;
	uint NumAccumulatedSamples;

	uint RenderTarget;
};

ConstantBuffer<SystemConstants> 	g_SystemConstants 	: register(b0, space0);
ConstantBuffer<RenderPassData> 		g_RenderPassData	: register(b1, space0);

RaytracingAccelerationStructure		Scene				: register(t0, space0);
StructuredBuffer<Material>			Materials			: register(t1, space0);

SamplerState						SamplerLinearWrap	: register(s0, space0);
SamplerState						SamplerLinearClamp	: register(s1, space0);

#include <DescriptorTable.hlsli>

RayDesc GenerateCameraRay(in float2 ndc, inout uint seed)
{
	float3 direction = ndc.x * g_SystemConstants.Camera.U.xyz + ndc.y * g_SystemConstants.Camera.V.xyz + g_SystemConstants.Camera.W.xyz;
	
	// Find the focal point for this pixel
	direction /= length(g_SystemConstants.Camera.W); // Make ray have length 1 along the camera's w-axis.
	float3 focalPoint = g_SystemConstants.Camera.Position.xyz + g_SystemConstants.Camera.FocalLength * direction; // Select point on ray a distance FocalLength along the w-axis
	
	// Get random numbers (in polar coordinates), convert to random cartesian uv on the lens
	float2 rnd = float2(s_2PI * RandomFloat01(seed), g_SystemConstants.Camera.RelativeAperture * RandomFloat01(seed));
	float2 uv = float2(cos(rnd.x) * rnd.y, sin(rnd.x) * rnd.y);

	// Use uv coordinate to compute a random origin on the camera lens
	float3 origin = g_SystemConstants.Camera.Position.xyz + uv.x * normalize(g_SystemConstants.Camera.U.xyz) + uv.y * normalize(g_SystemConstants.Camera.V.xyz);
	direction = normalize(focalPoint - origin);
	
	RayDesc ray = { origin, 0.0f, direction, 1e+38f };
	return ray;
}

#endif // GLOBAL_HLSLI