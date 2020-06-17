#pragma once
#include <DirectXMath.h>

namespace Math
{
	static constexpr DirectX::XMVECTOR Right = { 1.0f,  0.0f,  0.0f, 1.0f };
	static constexpr DirectX::XMVECTOR Up = { 0.0f,  1.0f,  0.0f, 1.0f };
	static constexpr DirectX::XMVECTOR Forward = { 0.0f,  0.0f,  1.0f, 1.0f };
	static constexpr float Epsilon = 1.192092896e-7f;

	// NOTE: [] = inclusive, () = exclusive

	// Returns random int in [a, b).
	int Rand(int min, int max);

	// Returns random float in [0, 1).
	float RandF();

	// Returns random float in [a, b).
	float RandF(float min, float max);

	// Returns the minimum of a or b
	template<typename T>
	constexpr T Min(const T& a, const T& b)
	{
		return a < b ? a : b;
	}

	// Returns the maximum of a or b
	template<typename T>
	constexpr T Max(const T& a, const T& b)
	{
		return a > b ? a : b;
	}

	// Returns the interpolated value in [v0, v1]
	template<typename T>
	constexpr T Lerp(const T& v0, const T& v1, float t)
	{
		return ((T)1 - t) * v0 + t * v1;
	}

	// Returns the clamped value between [min, max]
	template<typename T>
	constexpr T Clamp(const T& value, const T& min, const T& max)
	{
		return value < min ? min : (value > max ? max : value);
	}

	// Returns the converted cartesian system from spherical
	DirectX::XMVECTOR CartesianToSpherical(float x, float y, float z);
	// Returns the converted spherical system from cartesian
	DirectX::XMVECTOR SphericalToCartesian(float radius, float theta, float phi);

	// Returns an Identity matrix in the type XMFLOAT4X4
	DirectX::XMFLOAT4X4 Identity();

	template<typename T>
	inline T AlignUp(T Size, T Alignment)
	{
		return (T)(((size_t)Size + Alignment - 1) & ~(Alignment - 1));
	}

	template<typename T>
	inline T AlignDown(T Size, T Alignment)
	{
		return (T)((size_t)Size & ~(Alignment - 1));
	}
}