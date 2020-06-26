#ifndef LIGHT_HLSLI
#define LIGHT_HLSLI

#define MAX_POINT_LIGHT 5
#define MAX_SPOT_LIGHT 5

struct DirectionalLight
{
	float3 Strength;
	float Intensity;
	float3 Direction;
	float _padding1;
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

struct Cascade
{
	row_major matrix ShadowTransform;
	float4 Offset;
	float4 Scale;
	float Split;
	float3 _padding;
};
#endif