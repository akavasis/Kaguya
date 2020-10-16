#ifndef __LIGHT_H__
#define __LIGHT_H__
#include "SharedDefines.h"

struct PolygonalLight
{
	matrix	World;
	float3	Color;
	float	Width;
	float	Height;
};

#endif