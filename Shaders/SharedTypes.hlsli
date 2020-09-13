#ifndef __SHARED_TYPES_HLSLI__
#define __SHARED_TYPES_HLSLI__
#include "SharedDefines.hlsli"
#include "Light.hlsli"

struct ObjectConstants
{
	matrix World;
	unsigned int MaterialIndex;
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
	unsigned int TotalFrameCount;

	DirectionalLight Sun;
	unsigned int SunShadowMapIndex;
	unsigned int BRDFLUTMapIndex;
	unsigned int RadianceCubemapIndex;
	unsigned int IrradianceCubemapIndex;
	unsigned int PrefilteredRadianceCubemapIndex;
	
	unsigned int MaxDepth;
};

struct MaterialTextureProperties
{
	float3 Albedo;
	float Roughness;
	float Metallic;
	float3 Emissive;
	float IndexOfRefraction;
	unsigned int ShadingModel;
};

struct MaterialTextureIndices
{
	int AlbedoMapIndex;
	int NormalMapIndex;
	int RoughnessMapIndex;
	int MetallicMapIndex;
	int EmissiveMapIndex;
	bool IsMasked;
};

struct GeometryInfo
{
	unsigned int VertexOffset;
	unsigned int IndexOffset;
	unsigned int MaterialIndex;
};
#endif