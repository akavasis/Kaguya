#ifndef __LIGHT_HLSLI__
#define __LIGHT_HLSLI__
#include "SharedDefines.hlsli"

struct PolygonalLight
{
	float3 	Color;
	matrix 	Transform;
	float	Width;
	float	Height;
};

#endif