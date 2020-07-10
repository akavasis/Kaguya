#pragma once
#include "MathLibrary.h"

struct Transform
{
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Scale;
	DirectX::XMFLOAT4 Orientation;

	Transform();

	void Reset();

	void SetTransform(DirectX::FXMMATRIX M);
	void Translate(float DeltaX, float DeltaY, float DeltaZ);
	void SetScale(float ScaleX, float ScaleY, float ScaleZ);
	// Radians
	void Rotate(float AngleX, float AngleY, float AngleZ);

	DirectX::XMMATRIX Matrix() const;

	DirectX::XMVECTOR Up() const;
	DirectX::XMVECTOR Right() const;
	DirectX::XMVECTOR Forward() const;
};