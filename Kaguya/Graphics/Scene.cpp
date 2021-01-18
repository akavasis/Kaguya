#include "pch.h"
#include "Scene.h"

using namespace DirectX;

Material Scene::LoadMaterial(const std::string& Name, std::optional<std::filesystem::path> AlbedoMapPath /*= {}*/,
	std::optional<std::filesystem::path> NormalMapPath /*= {}*/,
	std::optional<std::filesystem::path> RoughnessMapPath /*= {}*/,
	std::optional<std::filesystem::path> MetallicMapPath /*= {}*/,
	std::optional<std::filesystem::path> EmissiveMapPath /*= {}*/)
{
	Material Material(Name);

	auto InitTexture = [&](TextureTypes Type, const std::optional<std::filesystem::path>& Path)
	{
		if (Path)
		{
			Material.Textures[Type].Path = (Application::ExecutableFolderPath / Path.value()).string();
			assert(std::filesystem::exists(Material.Textures[Type].Path));
		}
	};

	InitTexture(TextureTypes::AlbedoIdx, AlbedoMapPath);
	InitTexture(TextureTypes::NormalIdx, NormalMapPath);
	InitTexture(TextureTypes::RoughnessIdx, RoughnessMapPath);
	InitTexture(TextureTypes::MetallicIdx, MetallicMapPath);
	InitTexture(TextureTypes::EmissiveIdx, EmissiveMapPath);

	return Material;
}

Mesh Scene::LoadMeshFromFile(const std::filesystem::path& Path)
{
	return {"Unknown"};
}