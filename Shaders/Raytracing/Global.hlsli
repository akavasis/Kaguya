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

	int2 MousePosition;

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

RaytracingAccelerationStructure		g_Scene				: register(t0, space0);
StructuredBuffer<Material>			g_Materials			: register(t1, space0);

SamplerState						SamplerLinearWrap	: register(s0, space0);
SamplerState						SamplerLinearClamp	: register(s1, space0);

#include <DescriptorTable.hlsli>

#endif // GLOBAL_HLSLI