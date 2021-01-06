#pragma once

#include <string>

#include "SharedDefines.h"
#include "Transform.h"

using Lumen = float;
using Nit = float;

// Current this is a rect light
struct PolygonalLight
{
	PolygonalLight(const std::string& Name);
	void RenderGui();

	// These method should be used to configure attributes of light
	void SetDimension(float Width, float Height);
	void SetLuminousPower(Lumen LuminousPower);

	std::string				Name;
	bool					Dirty;

	Transform				Transform;
	float3					Color;

	float					Width;
	float					Height;
	float					Area;

	Lumen					LuminousPower;
	Nit						Luminance;
};