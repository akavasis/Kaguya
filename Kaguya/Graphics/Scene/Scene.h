#pragma once
#include <list>
#include "Model.h"
#include "Camera.h"
#include "Light.h"
#include "Skybox.h"

struct Scene
{
	PerspectiveCamera Camera;
	DirectionalLight DirectionalLight;
	std::list<PointLight> PointLights;
	std::list<SpotLight> SpotLights;
	std::list<Model> Models;

	PointLight& AddPointLight(PointLight PointLight);
	SpotLight& AddSpotLight(SpotLight SpotLight);
	Model& AddModel(Model Model);

	void RenderImGuiWindow();
};