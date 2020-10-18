#pragma once
#include "SharedDefines.h"
#include "Math/Transform.h"

struct PolygonalLight
{
	Transform	Transform;
	float3		Color;
	float		Width;
	float		Height;

	size_t		GpuLightIndex;

	void RenderGui();
};