#pragma once
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
	using PointLightList = std::list<PointLight>;
	using SpotLightList = std::list<SpotLight>;
	using MaterialList = std::list<Material>;
	using ModelList = std::list<Model>;
	using ModelInstanceList = std::list<ModelInstance>;

	Skybox Skybox;
	PerspectiveCamera Camera;
	DirectionalLight Sun;
	PointLightList PointLights;
	SpotLightList SpotLights;
	MaterialList Materials;
	ModelList Models;
	ModelInstanceList ModelInstances;

	PointLight& AddPointLight(PointLight&& PointLight);
	SpotLight& AddSpotLight(SpotLight&& SpotLight);
	Material& AddMaterial(Material&& Material);
	Model& AddModel(Model&& Model);
	ModelInstance& AddModelInstance(ModelInstance&& ModelInstance);
};

UINT CullModels(const Camera* pCamera, const Scene::ModelInstanceList& ModelInstances, std::vector<const ModelInstance*>& Indices);
UINT CullModelsOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const Scene::ModelInstanceList& ModelInstances, std::vector<const ModelInstance*>& Indices);
UINT CullMeshes(const Camera* pCamera, const std::vector<MeshInstance>& MeshInstances, std::vector<UINT>& Indices);
UINT CullMeshesOrthographic(const OrthographicCamera& Camera, bool IgnoreNearZ, const std::vector<MeshInstance>& MeshInstances, std::vector<UINT>& Indices);