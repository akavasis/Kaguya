#pragma once
#include <filesystem>
#include "SharedTypes.h"

enum TextureTypes
{
	Albedo,
	Normal,
	Roughness,
	Metallic,
	Emissive,
	NumTextureTypes
};

enum class TextureFlags
{
	Unknown,
	Disk,
	Embedded
};

struct Material
{
	struct Texture
	{
		std::filesystem::path Path;
		std::size_t EmbeddedIndex;
		TextureFlags Flag;
	};
	Texture Textures[NumTextureTypes];
	MaterialTextureProperties Properties;
	int TextureIndices[NumTextureTypes];
	bool IsMasked = false;

	Material()
	{
		for (int i = 0; i < NumTextureTypes; ++i)
		{
			Textures[i] = {};
			TextureIndices[i] = -1;
		}
		Properties.Albedo = DirectX::XMFLOAT3(1.0f, 1.0f, 1.0f);
		Properties.Metallic = 0.1f;
		Properties.Emissive = DirectX::XMFLOAT3(0.0f, 0.0f, 0.0f);
		Properties.Roughness = 0.5f;
	}
};