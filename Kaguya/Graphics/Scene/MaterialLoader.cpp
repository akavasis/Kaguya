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
	Material material = {};

	auto InitTexture = [&](TextureTypes Type, const char* pPath)
	{
		if (pPath)
		{
			material.Textures[Type].Path = m_ExecutableFolderPath / pPath;
			material.Textures[Type].Flag = TextureFlags::Disk;
			assert(std::filesystem::exists(material.Textures[Type].Path));
		}
	};

	InitTexture(TextureTypes::Albedo, pAlbedoMapPath);
	InitTexture(TextureTypes::Normal, pNormalMapPath);
	InitTexture(TextureTypes::Roughness, pRoughnessMapPath);
	InitTexture(TextureTypes::Metallic, pMetallicMapPath);
	InitTexture(TextureTypes::Emissive, pEmissiveMapPath);

	material.Properties.Albedo = { 0.0f, 0.0f, 0.0f };
	material.Properties.Roughness = 0.0f;
	material.Properties.Metallic = 0.0f;
	material.Properties.Emissive = { 0.0f, 0.0f, 0.0f };
	material.Properties.IndexOfRefraction = 1.0f;
	material.Properties.ShadingModel = 0;

	return material;
}