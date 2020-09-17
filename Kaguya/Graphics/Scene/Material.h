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

#define MATERIAL_MODEL_LAMBERTIAN 0
#define MATERIAL_MODEL_GLOSSY 1
#define MATERIAL_MODEL_METAL 2
#define MATERIAL_MODEL_DIELECTRIC 3
#define MATERIAL_MODEL_DIFFUSE_LIGHT 4

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