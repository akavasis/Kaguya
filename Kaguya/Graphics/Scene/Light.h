#pragma once
#include <string>
#include "SharedDefines.h"
#include "Math/Transform.h"

struct PolygonalLight
{
	std::string Name;
	Transform	Transform;
	float3		Color;
	float		Intensity;
	float		Width;
	float		Height;

	size_t		GpuLightIndex;

	void RenderGui();
};