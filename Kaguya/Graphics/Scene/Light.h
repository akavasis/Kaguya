#pragma once
#include <string>
#include "Math/Transform.h"

using Lumen = float;
using Nit = float;

// Current this is a rect light
class PolygonalLight
{
public:
	PolygonalLight(const char* pName);

	void RenderGui();

	inline bool IsDirty() const { return Dirty; }
	inline void ResetDirtyFlag() { Dirty = false; }

	inline auto GetWidth() const { return Width; }
	inline auto GetHeight() const { return Height; }
	inline auto GetLuminance() const { return Luminance; }
	inline auto GetGpuLightIndex() const { return GpuLightIndex; }

	void SetDimension(float Width, float Height);
	void SetLuminousPower(Lumen LuminousPower);
	void SetGpuLightIndex(size_t GpuLightIndex);

	Transform				Transform;
	DirectX::XMFLOAT3		Color;
private:
	std::string				Name;
	bool					Dirty;
	float					Width;
	float					Height;
	float					Area;

	Lumen					LuminousPower;
	Nit						Luminance;

	size_t					GpuLightIndex;
};