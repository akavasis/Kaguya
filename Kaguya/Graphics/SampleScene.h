#pragma once
#include "Scene/Scene.h"

enum class SampleScene
{
	CornellBox,
	Hyperion,
};

Scene GenerateScene(SampleScene SampleScene);