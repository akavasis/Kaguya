#pragma once
#include "Scene.h"

enum class SampleScene
{
	CornellBox,

	PlaneWithLights,
};

Scene GenerateScene(SampleScene SampleScene);