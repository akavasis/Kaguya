#ifndef __SHARED_TYPES_H__
#define __SHARED_TYPES_H__
#include "SharedDefines.h"
#include "Light.h"
#include "Material.h"

#define MATERIAL_MODEL_LAMBERTIAN 0
#define MATERIAL_MODEL_METAL 1
#define MATERIAL_MODEL_DIELECTRIC 2
#define MATERIAL_MODEL_DIFFUSE_LIGHT 3

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