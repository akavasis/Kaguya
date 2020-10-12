#pragma once
#include "Scene.h"

enum class SampleScene
{
	Random,
	CornellBox,
	CornellBoxLambertianSpheres,
	CornellBoxGlossySpheres,
	CornellBoxTransparentSpheres,
};

Scene GenerateScene(SampleScene SampleScene);