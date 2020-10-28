#pragma once
#include <string>
#include "SharedDefines.h"

enum class TextureFlags
{
	Unknown,
	Disk,
	Embedded
};

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
	std::string Name;
	bool		Dirty;

	float3		Albedo;
	float3		Emissive;
	float3		Specular;
	float3		Refraction;
	float		SpecularChance;
	float		Roughness;
	float		Metallic;
	float		Fuzziness;
	float		IndexOfRefraction;
	uint		Model;

	int			TextureIndices[NumTextureTypes];

	struct Texture
	{
		std::string				Path;
		std::size_t				EmbeddedIndex;
		TextureFlags			Flag;
	};
	Texture Textures[NumTextureTypes];
	size_t GpuMaterialIndex;

	Material();
	void RenderGui();
};