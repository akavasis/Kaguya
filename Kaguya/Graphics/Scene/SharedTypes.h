#ifndef __SHARED_TYPES_H__
#define __SHARED_TYPES_H__
#include "SharedDefines.h"
#include "Light.h"
#include "Material.h"

struct ObjectConstants
{
	matrix World;
	uint MaterialIndex;
	float3 _padding;
};

struct RenderPassConstants
{
	matrix View;
	matrix Projection;
	matrix InvView;
	matrix InvProjection;
	matrix ViewProjection;
	float3 EyePosition;
	uint TotalFrameCount;

	DirectionalLight Sun;
	uint SunShadowMapIndex;
	uint BRDFLUTMapIndex;
	uint RadianceCubemapIndex;
	uint IrradianceCubemapIndex;
	uint PrefilteredRadianceCubemapIndex;

	uint MaxDepth;
};

struct GeometryInfo
{
	uint VertexOffset;
	uint IndexOffset;
	uint MaterialIndex;
	matrix World;
};
#endif