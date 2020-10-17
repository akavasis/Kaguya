#pragma once
#include "Scene.h"

enum class SampleScene
{
	Random,
	CornellBox,
	CornellBoxLambertianSpheres,
	CornellBoxGlossySpheres,
	CornellBoxTransparentSpheres,

	PlaneWithLights
};

Scene GenerateScene(SampleScene SampleScene);