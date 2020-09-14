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
		Properties = {};
	}
};