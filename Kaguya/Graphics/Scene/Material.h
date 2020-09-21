#ifndef __MATERIAL_H__
#define __MATERIAL_H__
#include "SharedDefines.h"

#ifdef __cplusplus
#include <filesystem>

enum class TextureFlags
{
	Unknown,
	Disk,
	Embedded
};
#endif

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

#ifdef __cplusplus
	struct Texture
	{
		std::filesystem::path Path;
		std::size_t EmbeddedIndex;
		TextureFlags Flag;
	};
	Texture Textures[NumTextureTypes];
	size_t GpuMaterialIndex;

	Material()
	{
		for (int i = 0; i < NumTextureTypes; ++i)
		{
			Textures[i] = {};
			TextureIndices[i] = -1;
		}
	}
#endif
};
#endif