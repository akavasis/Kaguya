#pragma once
#include <DirectXMath.h>
#include <DirectXCollision.h>
#include <cmath>
#include <limits>

namespace Math
{
	static constexpr DirectX::XMVECTOR Right = { 1.0f,  0.0f,  0.0f, 0.0f };
	static constexpr DirectX::XMVECTOR Up = { 0.0f,  1.0f,  0.0f, 0.0f };
	static constexpr DirectX::XMVECTOR Forward = { 0.0f,  0.0f,  1.0f, 0.0f };
	static constexpr float Epsilon = 1.192092896e-7f;
	static constexpr float Infinity = std::numeric_limits<float>::infinity();

	// NOTE: [] = inclusive, () = exclusive

	// Returns random int in [a, b].
	int Rand(int min, int max);

	// Returns random float in [0, 1).
	float RandF();

	// Returns random float in [a, b).
	float RandF(float min, float max);

	// Returns the minimum of a or b
	template<typename T>
	constexpr inline T Min(T a, T b)
	{
		return a < b ? a : b;
	}

	// Returns the maximum of a or b
	template<typename T>
	constexpr inline T Max(T a, T b)
	{
		return a > b ? a : b;
	}

	// Returns the interpolated value in [v0, v1]
	template<typename T>
	constexpr inline T Lerp(T v0, T v1, float t)
	{
		return ((T)1 - t) * v0 + t * v1;
	}

	// Returns the clamped value between [min, max]
	template<typename T>
	constexpr inline T Clamp(T value, T min, T max)
	{
		return value < min ? min : (value > max ? max : value);
	}

	// Returns the converted cartesian system from spherical
	DirectX::XMVECTOR CartesianToSpherical(float x, float y, float z);
	// Returns the converted spherical system from cartesian
	DirectX::XMVECTOR SphericalToCartesian(float radius, float theta, float phi);

	DirectX::XMVECTOR RandomVector();
	DirectX::XMVECTOR RandomVector(float Min, float Max);
	DirectX::XMVECTOR RandomUnitVector();
	DirectX::XMVECTOR RandomInUnitDisk();
	DirectX::XMVECTOR RandomInUnitSphere();
	DirectX::XMVECTOR RandomInHemisphere(DirectX::FXMVECTOR Normal);

	float SchlickApproximation(float cosineTheta, float indexOfRefraction);

	// Returns an Identity matrix in the type XMFLOAT4X4
	DirectX::XMFLOAT4X4 Identity();
	DirectX::XMVECTOR QuaternionToEulerAngles(DirectX::CXMVECTOR Q);

	template<typename T>
	constexpr inline T AlignUp(T Size, T Alignment)
	{
		return (T)(((size_t)Size + (size_t)Alignment - 1) & ~((size_t)Alignment - 1));
	}

	template<typename T>
	constexpr inline T AlignDown(T Size, T Alignment)
	{
		return (T)((size_t)Size & ~((size_t)Alignment - 1));
	}

	template <typename T>
	constexpr inline T RoundUpAndDivide(T Value, size_t Alignment)
	{
		return (T)((Value + Alignment - 1) / Alignment);
	}
}

// Returns radians
constexpr inline float operator"" _Deg(long double Degrees)
{
	return DirectX::XMConvertToRadians(static_cast<float>(Degrees));
}

// Returns degrees
constexpr inline float operator"" _Rad(long double Radians)
{
	return DirectX::XMConvertToDegrees(static_cast<float>(Radians));
}

constexpr inline std::size_t operator"" _KiB(std::size_t X)
{
	return X * 1024;
}

constexpr inline std::size_t operator"" _MiB(std::size_t X)
{
	return X * 1024 * 1024;
}

constexpr inline std::size_t operator"" _GiB(std::size_t X)
{
	return X * 1024 * 1024 * 1024;
}

inline std::size_t ToKiB(std::size_t Byte)
{
	return Byte / 1024;
}

inline std::size_t ToMiB(std::size_t Byte)
{
	return Byte / 1024 / 1024;
}

inline std::size_t ToGiB(std::size_t Byte)
{
	return Byte / 1024 / 1024 / 1024;
}