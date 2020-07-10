#include "pch.h"
#include "TextureAllocator.h"

TextureAllocator::TextureAllocator(std::filesystem::path ExecutableFolderPath)
	: m_ExecutableFolderPath(ExecutableFolderPath)
{
}

std::string TextureAllocator::LoadFromFile(const char* pPath, bool ForceSRGB, bool GenerateMips)
{
	if (!pPath)
		return "";

	std::filesystem::path filePath = m_ExecutableFolderPath / pPath;
	std::string fileName = filePath.filename().generic_string();
	if (!std::filesystem::exists(filePath))
	{
		CORE_ERROR("File: {} Not found");
		assert(false);
		return "";
	}
	auto iter = m_UnstagedTextures.find(fileName);
	if (iter != m_UnstagedTextures.end())
	{
		return fileName;
	}

	DirectX::TexMetadata metadata;
	DirectX::ScratchImage scratchImage;
	if (filePath.extension() == ".dds")
	{
		ThrowCOMIfFailed(DirectX::LoadFromDDSFile(filePath.c_str(), DirectX::DDS_FLAGS::DDS_FLAGS_FORCE_RGB, &metadata, scratchImage));
	}
	else if (filePath.extension() == ".tga")
	{
		ThrowCOMIfFailed(DirectX::LoadFromTGAFile(filePath.c_str(), &metadata, scratchImage));
	}
	else if (filePath.extension() == ".hdr")
	{
		ThrowCOMIfFailed(DirectX::LoadFromHDRFile(filePath.c_str(), &metadata, scratchImage));
	}
	else
	{
		ThrowCOMIfFailed(DirectX::LoadFromWICFile(filePath.c_str(), DirectX::WIC_FLAGS::WIC_FLAGS_FORCE_RGB, &metadata, scratchImage));
	}

	bool generateMips = (metadata.mipLevels > 1) ? false : GenerateMips;
	size_t mipLevels = generateMips ? static_cast<size_t>(std::floor(std::log2(std::max(metadata.width, metadata.height)))) + 1 : 1;

	if (ForceSRGB)
	{
		metadata.format = DirectX::MakeSRGB(metadata.format);
		if (!DirectX::IsSRGB(metadata.format))
		{
			CORE_WARN("Failed to convert to SRGB format");
		}
	}

	TextureData textureData;
	textureData.scratchImage = std::move(scratchImage);
	textureData.metadata = metadata;
	textureData.mipLevels = mipLevels;
	textureData.generateMips = generateMips;
	m_UnstagedTextures[fileName] = std::move(textureData);
	return fileName;
}