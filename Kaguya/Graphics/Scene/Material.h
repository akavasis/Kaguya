#pragma once
#include <filesystem>

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
		std::size_t EmbeddedIdx;
		TextureFlags Flag;
	};
	Texture Textures[NumTextureTypes];
};