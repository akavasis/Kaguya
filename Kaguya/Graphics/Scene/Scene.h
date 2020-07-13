#pragma once
#include <list>
#include "Skybox.h"
#include "Camera.h"
#include "Light.h"
#include "Model.h"

struct Scene
{
	using PointLightList = std::list<PointLight>;
	using SpotLightList = std::list<SpotLight>;
	using ModelList = std::list<Model>;

	Skybox Skybox;
	PerspectiveCamera Camera;
	DirectionalLight DirectionalLight;
	PointLightList PointLights;
	SpotLightList SpotLights;
	ModelList Models;

	PointLight& AddPointLight(PointLight&& PointLight);
	SpotLight& AddSpotLight(SpotLight&& SpotLight);
	Model& AddModel(Model&& Model);

	void RenderImGuiWindow();
};