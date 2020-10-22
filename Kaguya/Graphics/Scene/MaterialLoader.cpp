#include "pch.h"
#include "MaterialLoader.h"

MaterialLoader::MaterialLoader(std::filesystem::path ExecutableFolderPath)
	: m_ExecutableFolderPath(ExecutableFolderPath)
{
	
}

Material MaterialLoader::LoadMaterial(
	const char* pAlbedoMapPath,
	const char* pNormalMapPath,
	const char* pRoughnessMapPath,
	const char* pMetallicMapPath,
	const char* pEmissiveMapPath) const
{
	Material Material = {};

	auto InitTexture = [&](TextureTypes Type, const char* pPath)
	{
		if (pPath)
		{
			Material.Textures[Type].Path = m_ExecutableFolderPath / pPath;
			Material.Textures[Type].Flag = TextureFlags::Disk;
			assert(std::filesystem::exists(Material.Textures[Type].Path));
		}
	};

	InitTexture(TextureTypes::AlbedoIdx, pAlbedoMapPath);
	InitTexture(TextureTypes::NormalIdx, pNormalMapPath);
	InitTexture(TextureTypes::RoughnessIdx, pRoughnessMapPath);
	InitTexture(TextureTypes::MetallicIdx, pMetallicMapPath);
	InitTexture(TextureTypes::EmissiveIdx, pEmissiveMapPath);

	return Material;
}