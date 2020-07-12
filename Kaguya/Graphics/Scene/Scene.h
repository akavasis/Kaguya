#pragma once
#include <list>
#include "Skybox.h"
#include "Camera.h"
#include "Light.h"
#include "Model.h"

struct Scene
{
	Skybox Skybox;
	PerspectiveCamera Camera;
	DirectionalLight DirectionalLight;
	std::list<PointLight> PointLights;
	std::list<SpotLight> SpotLights;
	std::list<Model> Models;

	PointLight& AddPointLight(PointLight&& PointLight);
	SpotLight& AddSpotLight(SpotLight&& SpotLight);
	Model& AddModel(Model&& Model);

	void RenderImGuiWindow();
};