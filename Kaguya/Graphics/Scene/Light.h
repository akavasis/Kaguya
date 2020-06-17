#pragma once
#include <DirectXMath.h>

// Following HLSL packing rules
struct DirectionalLight
{
	DirectX::XMFLOAT3 Strength;
	float Intensity;
	DirectX::XMFLOAT3 Direction;
	float _padding;

	DirectionalLight();

	void RenderImGuiWindow();
	void Reset();
};

struct PointLight
{
	DirectX::XMFLOAT3 Strength;
	float Intensity;
	DirectX::XMFLOAT3 Position;
	float Radius;

	PointLight();

	void RenderImGuiWindow();
	void Reset();
};

struct SpotLight
{
	DirectX::XMFLOAT3 Strength;
	float Intensity;
	DirectX::XMFLOAT3 Position;
	float InnerConeAngle;
	DirectX::XMFLOAT3 Direction;
	float OuterConeAngle;
	float Radius;
	DirectX::XMFLOAT3 _padding;

	SpotLight();

	void RenderImGuiWindow();
	void Reset();
};

struct Cascade
{
	DirectX::XMFLOAT4X4 ShadowTransform;
	DirectX::XMFLOAT4 Offset;
	DirectX::XMFLOAT4 Scale;
	float Split;
	DirectX::XMFLOAT3 _padding;
};