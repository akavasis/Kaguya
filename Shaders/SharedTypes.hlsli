#ifndef __SHARED_TYPES_HLSLI__
#define __SHARED_TYPES_HLSLI__

#include "SharedDefines.hlsli"
#include "Light.hlsli"
#include "Material.hlsli"

struct GeometryInfo
{
	uint VertexOffset;
	uint IndexOffset;
	uint MaterialIndex;
	matrix World;
};

#endif