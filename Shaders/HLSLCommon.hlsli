#ifndef __HLSL_COMMON_HLSLI__
#define __HLSL_COMMON_HLSLI__

#include "Vertex.hlsli"
#include "Material.hlsli"
#include "Light.hlsli"
#include "SharedTypes.hlsli"
#include "Constants.hlsli"

float3 CartesianToSpherical(float x, float y, float z)
{
	float radius = sqrt(x * x + y * y + z * z);
	float theta = acos(z / radius);
	float phi = atan(y / x);
	return float3(radius, theta, phi);
}

float3 SphericalToCartesian(float radius, float theta, float phi)
{
	float x = radius * sin(theta) * cos(phi);
	float y = radius * sin(theta) * sin(phi);
	float z = radius * cos(theta);
	return float3(x, y, z);
}

#endif