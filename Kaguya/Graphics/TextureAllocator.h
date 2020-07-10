#pragma once
#include <unordered_map>
#include <filesystem>
#include <string>

#include <DirectXTex.h>

class TextureAllocator
{
public:
	TextureAllocator(std::filesystem::path ExecutableFolderPath);

	std::string LoadFromFile(const char* pPath, bool ForceSRGB, bool GenerateMips);
private:
	friend class GpuTextureAllocator;
	std::filesystem::path m_ExecutableFolderPath;

	struct TextureData
	{
		DirectX::ScratchImage scratchImage;	// image that contains the subresource data to be upload in the texture field
		DirectX::TexMetadata metadata;		// metadata of the scratchImage
		std::size_t mipLevels;				// indicates what mip levels this texture should have
		bool generateMips;					// indicates whether or not we should generate mips for the staged texture

		TextureData() = default;
		~TextureData() = default;

		TextureData(const TextureData&) = delete;
		TextureData& operator=(const TextureData&) = delete;

		TextureData(TextureData&&) noexcept = default;
		TextureData& operator=(TextureData&&) noexcept = default;
	};
	std::unordered_map<std::string, TextureData> m_UnstagedTextures;
};