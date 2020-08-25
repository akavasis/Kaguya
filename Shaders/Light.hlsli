#ifndef __LIGHT_HLSLI__
#define __LIGHT_HLSLI__
#include "SharedDefines.hlsli"

#define NUM_CASCADES 4
// Following HLSL packing rules
struct Cascade
{
	row_major matrix ShadowTransform;
	float4 Offset;
	float4 Scale;
	float Split;
	float3 _padding;
};

struct DirectionalLight
{
	float3 Strength;
	float Intensity;
	float3 Direction;
	float Lambda;
	Cascade Cascades[NUM_CASCADES];
};

struct PointLight
{
	float3 Strength;
	float Intensity;
	float3 Position;
	float Radius;
};

struct SpotLight
{
	float3 Strength;
	float Intensity;
	float3 Position;
	float InnerConeAngle;
	float3 Direction;
	float OuterConeAngle;
	float Radius;
	float3 _padding;
};
#endif