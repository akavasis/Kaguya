#pragma once
#include "Scene.h"

enum class SampleScene
{
	Random,
	CornellBox,
	CornellBoxLambertianSpheres,
	CornellBoxGlossySpheres,
	PlaneWithTransparentSpheres,

	PlaneWithLights,

	Sponza
};

Scene GenerateScene(SampleScene SampleScene);