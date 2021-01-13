#pragma once
#include "Scene.h"

enum class SampleScene
{
	CornellBox,
	Hyperion,
};

Scene GenerateScene(SampleScene SampleScene);