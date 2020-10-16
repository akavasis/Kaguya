#ifndef __LIGHT_HLSLI__
#define __LIGHT_HLSLI__
#include "SharedDefines.hlsli"

struct PolygonalLight
{
	matrix World;
	float3 Color;
	float Width;
	float Height;
};

#endif