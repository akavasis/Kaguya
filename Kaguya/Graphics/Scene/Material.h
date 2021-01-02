#pragma once
#include <string>
#include "SharedDefines.h"

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

#define TEXTURE_CHANNEL_RED		0
#define TEXTURE_CHANNEL_GREEN	1
#define TEXTURE_CHANNEL_BLUE	2
#define TEXTURE_CHANNEL_ALPHA	3

struct Material
{
	Material(const std::string& Name);
	void RenderGui();

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
	int			Model;
	bool		UseAttributes; // If this is true, then the attributes above will be used rather than actual textures

	int			TextureIndices[NumTextureTypes];
	int			TextureChannel[NumTextureTypes];

	struct Texture
	{
		std::string				Path;
	};
	Texture Textures[NumTextureTypes];
};