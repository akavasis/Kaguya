#ifndef __LIGHT_H__
#define __LIGHT_H__
#include "SharedDefines.h"

struct PolygonalLight
{
	float3	Color;
	matrix	Transform;
	float	Width;
	float	Height;
};

#endif