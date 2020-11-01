#ifndef LIGHT_HLSLI
#define LIGHT_HLSLI

struct PolygonalLight
{
	float3	Position;
	float4	Orientation;
	matrix	World;
	float3	Color;
	float	Luminance;
	float	Width;
	float	Height;
};

#endif