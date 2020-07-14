#ifndef __SHARED_TYPES_H__
#define __SHARED_TYPES_H__
#include "SharedDefines.h"
#include "Light.h"

struct ObjectConstants
{
	matrix World;
	unsigned int MaterialIndex;
	float3 _padding;
};

struct RenderPassConstants
{
	matrix ViewProjection;
	float3 EyePosition;
	float _padding;

	DirectionalLight Sun;
	unsigned int BRDFLUTMapIndex;
	unsigned int SunShadowMapIndex;
	unsigned int IrradianceCubemapIndex;
	unsigned int PrefilteredRadianceCubemapIndex;
};

struct MaterialTextureIndices
{
	unsigned int AlbedoMapIndex;
	unsigned int NormalMapIndex;
	unsigned int RoughnessMapIndex;
	unsigned int MetallicMapIndex;
	unsigned int EmissiveMapIndex;
	float3 _padding;
};
#endif