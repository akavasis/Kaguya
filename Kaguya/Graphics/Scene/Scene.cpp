#include "pch.h"
#include "Scene.h"

Scene::Scene()
{
}

Scene::~Scene()
{
}

void Scene::AddModel(const char* FileName, float Scale)
{
	Models.push_back(std::make_unique<Model>(FileName, Scale));
}

void Scene::AddModel(std::unique_ptr<Model> pModel)
{
	Models.push_back(std::move(pModel));
}

void Scene::AddPointLight(PointLight PointLight)
{
	PointLights.push_back(PointLight);
}

void Scene::AddSpotLight(SpotLight SpotLight)
{
	SpotLights.push_back(SpotLight);
}

void Scene::RenderImGuiWindow()
{
	if (ImGui::Begin("Scene"))
	{
		DirectionalLight.RenderImGuiWindow();
		for (auto& PointLight : PointLights)
		{
			PointLight.RenderImGuiWindow();
		}
		for (auto& SpotLight : SpotLights)
		{
			SpotLight.RenderImGuiWindow();
		}
		for (auto& Model : Models)
		{
			Model->RenderImGuiWindow();
		}
	}
	ImGui::End();
}