#pragma once
#include "Component.h"
#include "../../SharedDefines.h"
#include "../../Asset/Image.h"
#include "../../Asset/AssetCache.h"

enum TextureTypes
{
	AlbedoIdx,
	NormalIdx,
	RoughnessIdx,
	MetallicIdx,
	NumTextureTypes
};

#define TEXTURE_CHANNEL_RED		0
#define TEXTURE_CHANNEL_GREEN	1
#define TEXTURE_CHANNEL_BLUE	2
#define TEXTURE_CHANNEL_ALPHA	3

enum BSDFTypes
{
	Lambertian,
	Mirror,
	Glass,
	Disney,
	NumBSDFTypes
};

struct Material
{
	Material()
	{
		for (int i = 0; i < NumTextureTypes; ++i)
		{
			TextureIndices[i] = -1;
			TextureChannel[i] = 0;

			TextureKeys[i] = 0;
		}
	}

	int BSDFType = BSDFTypes::Disney;
	float3 baseColor = { 1, 1, 1 };
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

	// Used by Glass BxDF
	float3 T = { 1, 1, 1 };
	float etaA = 1.000277f; // air
	float etaB = 1.5046f; // glass

	int TextureIndices[NumTextureTypes];
	int TextureChannel[NumTextureTypes];

	UINT64 TextureKeys[NumTextureTypes];
	AssetHandle<Asset::Image> Textures[NumTextureTypes];
};

struct MeshRenderer : Component
{
	struct MeshFilter* pMeshFilter = nullptr;
	Material Material;
};