#pragma once
#include "Scene.h"

enum class SampleScene
{
	Random,
	CornellBox,
	CornellBoxLambertianSpheres,
	CornellBoxGlossySpheres,
	PlaneWithTransparentSpheres,

	PlaneWithLights
};

Scene GenerateScene(SampleScene SampleScene);