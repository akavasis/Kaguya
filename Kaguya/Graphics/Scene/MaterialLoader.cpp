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
	Material material = {};

	material.Albedo = { 0.0f, 0.0f, 0.0f };
	material.Emissive = { 0.0f, 0.0f, 0.0f };
	material.SpecularChance = 0.0f;
	material.SpecularRoughness = 0.0f;
	material.SpecularColor = { 0.0f, 0.0f, 0.0f };
	material.Fuzziness = 0.0f;
	material.IndexOfRefraction = 1.0f;
	material.RefractionRoughness = 0.0f;
	material.RefractionColor = { 0.0f, 0.0f, 0.0f };
	material.Model = 0;

	auto InitTexture = [&](TextureTypes Type, const char* pPath)
	{
		if (pPath)
		{
			material.Textures[Type].Path = m_ExecutableFolderPath / pPath;
			material.Textures[Type].Flag = TextureFlags::Disk;
			assert(std::filesystem::exists(material.Textures[Type].Path));
		}
	};

	InitTexture(TextureTypes::AlbedoIdx, pAlbedoMapPath);
	InitTexture(TextureTypes::NormalIdx, pNormalMapPath);
	InitTexture(TextureTypes::RoughnessIdx, pRoughnessMapPath);
	InitTexture(TextureTypes::MetallicIdx, pMetallicMapPath);
	InitTexture(TextureTypes::EmissiveIdx, pEmissiveMapPath);

	return material;
}