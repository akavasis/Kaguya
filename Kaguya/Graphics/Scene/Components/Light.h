#pragma once
#include "Component.h"
#include "../../SharedDefines.h"

enum LightType
{
	PointLight,
	QuadLight,
};

struct Light : Component
{
	Light()
	{
		Type = PointLight;
		I = { 1.0f, 1.0f, 1.0f };
		Width = Height = 1.0f;
	}

	LightType Type;

	float3 I;
	float Width, Height;
};