#ifndef __HLSL_COMMON_HLSLI__
#define __HLSL_COMMON_HLSLI__

#include "Vertex.hlsli"
#include "Material.hlsli"
#include "Light.hlsli"
#include "SharedTypes.hlsli"
#include "Constants.hlsli"
#include "Sampling.hlsli"

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

void CoordinateSystem(float3 v1, out float3 v2, out float3 v3)
{
	if (abs(v1.x) > abs(v1.y))
		v2 = float3(-v1.z, 0, v1.x) / sqrt(v1.x * v1.x + v1.z * v1.z);
	else
		v2 = float3(0, v1.z, -v1.y) / sqrt(v1.y * v1.y + v1.z * v1.z);
	v3 = normalize(cross(v1, v2));
}

float3x3 GetTBNMatrix(float3 normal)
{
	float3 tangent, bitangent;
	CoordinateSystem(normal, tangent, bitangent);
	return float3x3(tangent, bitangent, normal);
}

#endif