#pragma once
#include "../../../SharedDefines.h"

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
	Material();

	float3 baseColor = { 1.0f, 1.0f, 1.0f };
	float metallic = 0.0f;
	float subsurface = 0.0f;
	float specular = 0.5f;
	float roughness = 0.5f;
	float specularTint = 0.0f;
	float anisotropic = 0.0f;
	float sheen = 0.0f;
	float sheenTint = 0.5f;
	float clearcoat = 0.0f;
	float clearcoatGloss = 1.0f;

	int TextureIndices[NumTextureTypes];
	int TextureChannel[NumTextureTypes];

	struct Texture
	{
		std::string				Path;
	};
	Texture Textures[NumTextureTypes];
};