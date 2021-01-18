#pragma once
#include "RenderDevice.h"

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
	static constexpr UINT64 MAX_LIGHT_SUPPORTED = 1000;
	static constexpr UINT64 MAX_MATERIAL_SUPPORTED = 1000;
	static constexpr UINT64 MAX_MESH_INSTANCE_SUPPORTED = 1000;

	static Material LoadMaterial(const std::string& Name,
		std::optional<std::filesystem::path> AlbedoMapPath = {},
		std::optional<std::filesystem::path> NormalMapPath = {},
		std::optional<std::filesystem::path> RoughnessMapPath = {},
		std::optional<std::filesystem::path> MetallicMapPath = {},
		std::optional<std::filesystem::path> EmissiveMapPath = {});
	static Mesh LoadMeshFromFile(const std::filesystem::path& Path);

	void AddLight(PolygonalLight Light)
	{
		assert(Lights.size() < MAX_LIGHT_SUPPORTED);

		Lights.push_back(Light);
	}

	[[nodiscard]] size_t AddMaterial(Material Material)
	{
		assert(Materials.size() < MAX_MATERIAL_SUPPORTED);

		size_t Index = Materials.size();
		Materials.push_back(Material);
		return Index;
	}

	[[nodiscard]] size_t AddMesh(Mesh Mesh)
	{
		size_t Index = Meshes.size();
		Meshes.push_back(Mesh);
		return Index;
	}

	// These are the ones going to be rendered
	size_t AddMeshInstance(MeshInstance MeshInstance)
	{
		assert(MeshInstances.size() < MAX_MESH_INSTANCE_SUPPORTED);

		size_t Index = MeshInstances.size();
		MeshInstances.push_back(MeshInstance);
		return Index;
	}

	bool Empty()
	{
		return MeshInstances.size() != 0;
	}

	Skybox						Skybox;
	Camera						Camera, PreviousCamera;

	std::vector<PolygonalLight> Lights;
	std::vector<Material>		Materials;
	std::vector<Mesh>			Meshes;
	std::vector<MeshInstance>	MeshInstances;

	Resource m_LightTable;
	Resource m_MaterialTable;
	Resource m_InstanceDescsBuffer;
	Resource m_MeshTable;
};