#include "pch.h"
#include "MathLibrary.h"
#include <stdlib.h>

namespace Math
{
	int Rand(int a, int b)
	{
		return a + rand() % ((b - a) + 1);
	}

	float RandF()
	{
		return static_cast<float>(rand()) / static_cast<float>(RAND_MAX);
	}

	float RandF(float min, float max)
	{
		return min + RandF() * (max - min);
	}

	DirectX::XMVECTOR CartesianToSpherical(float x, float y, float z)
	{
		float radius = sqrtf(x * x + y * y + z * z);
		float theta = acosf(z / radius);
		float phi = atanf(y / x);
		return DirectX::XMVectorSet(radius, theta, phi, 1.0f);
	}

	DirectX::XMVECTOR SphericalToCartesian(float radius, float theta, float phi)
	{
		float x = radius * sinf(theta) * cosf(phi);
		float y = radius * sinf(theta) * sinf(phi);
		float z = radius * cosf(theta);
		return DirectX::XMVectorSet(x, y, z, 1.0f);
	}

	DirectX::XMFLOAT4X4 Identity()
	{
		return DirectX::XMFLOAT4X4(
			1.0f, 0.0f, 0.0f, 0.0f,
			0.0f, 1.0f, 0.0f, 0.0f,
			0.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 1.0f);
	}
}