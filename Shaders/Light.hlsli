#ifndef LIGHT_HLSLI
#define LIGHT_HLSLI

struct PolygonalLight
{
	matrix	World;
	float3	Color;
	float	Luminance;
	float	Width;
	float	Height;
};

#endif