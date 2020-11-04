#pragma once
#include <cstdint>
#include <list>
#include "Skybox.h"
#include "Camera.h"
#include "Light.h"
#include "Material.h"
#include "Model.h"
#include "ModelInstance.h"
#include "MaterialLoader.h"
#include "ModelLoader.h"

#include "SharedTypes.h"

struct Scene
{
	using LightList			= std::list<PolygonalLight>;
	using MaterialList		= std::list<Material>;
	using ModelList			= std::list<Model>;
	using ModelInstanceList = std::list<ModelInstance>;

	Skybox				Skybox;
	PerspectiveCamera	Camera, PreviousCamera;
	LightList			Lights;
	MaterialList		Materials;
	ModelList			Models;
	ModelInstanceList	ModelInstances;

	PolygonalLight&	AddLight();
	Material&		AddMaterial(Material&& Material);
	Model&			AddModel(Model&& Model);
	ModelInstance&	AddModelInstance(ModelInstance&& ModelInstance);
};

uint32_t CullModels(const Camera* pCamera, const Scene::ModelInstanceList& ModelInstances, std::vector<const ModelInstance*>& Indices);
uint32_t CullModelsOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const Scene::ModelInstanceList& ModelInstances, std::vector<const ModelInstance*>& Indices);
uint32_t CullMeshes(const Camera* pCamera, const std::vector<MeshInstance>& MeshInstances, std::vector<uint32_t>& Indices);
uint32_t CullMeshesOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const std::vector<MeshInstance>& MeshInstances, std::vector<uint32_t>& Indices);