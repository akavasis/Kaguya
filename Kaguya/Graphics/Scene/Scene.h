#pragma once

#include <cstdint>
#include <list>
#include <optional>
#include <filesystem>

#include "Camera.h"
#include "Light.h"
#include "Material.h"
#include "Mesh.h"
#include "MeshInstance.h"

#include "SharedTypes.h"

struct Skybox
{
	std::filesystem::path Path;
};

struct Scene
{
	void LoadFromFile(const std::filesystem::path& Path);

	void AddLight(PolygonalLight Light)
	{
		Lights.push_back(Light);
	}

	size_t AddMaterial(Material Material)
	{
		size_t Index = Materials.size();
		Materials.push_back(Material);
		return Index;
	}

	size_t AddMesh(Mesh Mesh)
	{
		size_t Index = Meshes.size();
		Meshes.push_back(Mesh);
		return Index;
	}

	// These are the ones going to be rendered
	size_t AddMeshInstance(MeshInstance MeshInstance)
	{
		size_t Index = MeshInstances.size();
		MeshInstances.push_back(MeshInstance);
		return Index;
	}

	Skybox						Skybox;
	Camera						Camera, PreviousCamera;

	std::vector<PolygonalLight> Lights;
	std::vector<Material>		Materials;
	std::vector<Mesh>			Meshes;
	std::vector<MeshInstance>	MeshInstances;
};

Material LoadMaterial
(
	const std::string& Name,
	std::optional<std::filesystem::path> AlbedoMapPath,
	std::optional<std::filesystem::path> NormalMapPath,
	std::optional<std::filesystem::path> RoughnessMapPath,
	std::optional<std::filesystem::path> MetallicMapPath,
	std::optional<std::filesystem::path> EmissiveMapPath
);