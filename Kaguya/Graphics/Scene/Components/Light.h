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
		I = { 50.0f, 50.0f, 50.0f };
		Width = Height = 1.0f;
	}

	LightType Type;

	float3 I;
	float Width, Height;
};