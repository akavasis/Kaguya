#pragma once
#include "Scene.h"

enum class SampleScene
{
	CornellBox,
	Hyperion,

	PlaneWithLights,
};

Scene GenerateScene(SampleScene SampleScene);