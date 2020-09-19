#ifndef __MATERIAL_HLSLI__
#define __MATERIAL_HLSLI__
#include "SharedDefines.hlsli"

enum MaterialModel
{
	LambertianModel,
	GlossyModel,
	MetalModel,
	DielectricModel,
	DiffuseLightModel
};

enum TextureTypes
{
	AlbedoIdx,
	NormalIdx,
	RoughnessIdx,
	MetallicIdx,
	EmissiveIdx,
	NumTextureTypes
};

struct Material
{
	float3	Albedo;
	float3	Emissive;
	float3	Specular;
	float3	Refraction;
	float	SpecularChance;
	float	Roughness;
	float	Fuzziness;
	float	IndexOfRefraction;
	uint	Model;			
	
	int TextureIndices[NumTextureTypes];
};

#endif