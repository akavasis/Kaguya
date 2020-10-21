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
	Material Material			= {};
	Material.Albedo				= { 0.0f, 0.0f, 0.0f };
	Material.Emissive			= { 0.0f, 0.0f, 0.0f };
	Material.Specular			= { 0.0f, 0.0f, 0.0f };
	Material.Refraction			= { 0.0f, 0.0f, 0.0f };
	Material.SpecularChance		= 0.0f;
	Material.Roughness			= 0.0f;
	Material.Fuzziness			= 0.0f;
	Material.IndexOfRefraction	= 1.0f;
	Material.Model				= 0;

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