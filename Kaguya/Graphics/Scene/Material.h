#pragma once
#include <string>
#include <filesystem>
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

	float3	Albedo;
	float3	Emissive;
	float3	Specular;
	float3	Refraction;
	float	SpecularChance;
	float	Roughness;
	float	Fuzziness;
	float	IndexOfRefraction;
	uint	Model;

	int		TextureIndices[NumTextureTypes];

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
		Albedo				= { 0.0f, 0.0f, 0.0f };
		Emissive			= { 0.0f, 0.0f, 0.0f };
		Specular			= { 0.0f, 0.0f, 0.0f };
		Refraction			= { 0.0f, 0.0f, 0.0f };
		SpecularChance		= 0.0f;
		Roughness			= 0.0f;
		Fuzziness			= 0.0f;
		IndexOfRefraction	= 1.0f;
		Model				= 0;

		for (int i = 0; i < NumTextureTypes; ++i)
		{
			TextureIndices[i] = -1;
			Textures[i] = {};
		}
		GpuMaterialIndex = 0;
	}

	void RenderGui();
};