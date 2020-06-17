#pragma once
#include <memory>
#include "Model.h"
#include "Camera.h"
#include "Light.h"

struct Scene
{
	Scene();
	~Scene();

	std::vector<std::unique_ptr<Model>> Models;
	DirectionalLight DirectionalLight;
	std::vector<PointLight> PointLights;
	std::vector<SpotLight> SpotLights;

	void AddModel(const char* FileName, float Scale);
	void AddModel(std::unique_ptr<Model> pModel);

	void AddPointLight(PointLight PointLight);
	void AddSpotLight(SpotLight SpotLight);

	void RenderImGuiWindow();
};