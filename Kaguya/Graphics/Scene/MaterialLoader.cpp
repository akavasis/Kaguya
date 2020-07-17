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
	const char* pEmissiveMapPath)
{
	Material material{};

	if (pAlbedoMapPath)
	{
		material.Textures[TextureTypes::Albedo].Path = m_ExecutableFolderPath / pAlbedoMapPath;
		material.Textures[TextureTypes::Albedo].Flag = TextureFlags::Disk;
		assert(std::filesystem::exists(material.Textures[TextureTypes::Albedo].Path));
	}

	if (pNormalMapPath)
	{
		material.Textures[TextureTypes::Normal].Path = m_ExecutableFolderPath / pNormalMapPath;
		material.Textures[TextureTypes::Normal].Flag = TextureFlags::Disk;
		assert(std::filesystem::exists(material.Textures[TextureTypes::Normal].Path));
	}

	if (pRoughnessMapPath)
	{
		material.Textures[TextureTypes::Roughness].Path = m_ExecutableFolderPath / pRoughnessMapPath;
		material.Textures[TextureTypes::Roughness].Flag = TextureFlags::Disk;
		assert(std::filesystem::exists(material.Textures[TextureTypes::Roughness].Path));
	}

	if (pMetallicMapPath)
	{
		material.Textures[TextureTypes::Metallic].Path = m_ExecutableFolderPath / pMetallicMapPath;
		material.Textures[TextureTypes::Metallic].Flag = TextureFlags::Disk;
		assert(std::filesystem::exists(material.Textures[TextureTypes::Metallic].Path));
	}

	if (pEmissiveMapPath)
	{
		material.Textures[TextureTypes::Emissive].Path = m_ExecutableFolderPath / pEmissiveMapPath;
		material.Textures[TextureTypes::Emissive].Flag = TextureFlags::Disk;
		assert(std::filesystem::exists(material.Textures[TextureTypes::Emissive].Path));
	}

	return material;
}