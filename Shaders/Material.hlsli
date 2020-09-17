#ifndef __MATERIAL_HLSLI__
#define __MATERIAL_HLSLI__
#include "SharedDefines.hlsli"

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
	// Note: diffuse chance is 1.0f - (specularChance+refractionChance)
	float3	Albedo;					// the color used for diffuse lighting
	float3	Emissive;				// how much the surface glows
	float	SpecularChance;			// percentage chance of doing a specular reflection
	float	SpecularRoughness;		// how rough the specular reflections are
	float3	SpecularColor;			// the color tint of specular reflections
	float	IndexOfRefraction;		// index of refraction. used by fresnel and refraction.
	float	RefractionChance;		// percent chance of doing a refractive transmission
	float	RefractionRoughness;	// how rough the refractive transmissions are
	float3	RefractionColor;		// absorption for beer's law
	uint	Model;					// describes the material model
	
	int TextureIndices[NumTextureTypes];
};

#endif