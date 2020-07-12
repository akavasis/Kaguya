#include "pch.h"
#include "Scene.h"

PointLight& Scene::AddPointLight(PointLight&& PointLight)
{
	PointLights.push_back(std::move(PointLight));
	return PointLights.back();
}

SpotLight& Scene::AddSpotLight(SpotLight&& SpotLight)
{
	SpotLights.push_back(std::move(SpotLight));
	return SpotLights.back();
}

Model& Scene::AddModel(Model&& Model)
{
	Models.push_back(std::move(Model));
	return Models.back();
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
	}
	ImGui::End();
}