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
	float3	Albedo;					// The color used for diffuse lighting
	float3	Emissive;				// How much the surface glows
	float	SpecularChance;			// Percentage chance of doing a specular reflection
	float	SpecularRoughness;		// How rough the specular reflections are
	float3	SpecularColor;			// The color tint of specular reflections
	float	Fuzziness;
	float	IndexOfRefraction;		// Index of refraction. used by fresnel and refraction.
	float	RefractionRoughness;	// How rough the refractive transmissions are
	float3	RefractionColor;		// Absorption for beer's law
	uint	Model;					// Describes the material model

	int TextureIndices[NumTextureTypes];

#ifdef __cplusplus
	struct Texture
	{
		std::filesystem::path Path;
		std::size_t EmbeddedIndex;
		TextureFlags Flag;
	};
	Texture Textures[NumTextureTypes];

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