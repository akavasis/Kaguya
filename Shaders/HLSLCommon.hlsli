#ifndef HLSL_COMMON_HLSLI
#define HLSL_COMMON_HLSLI

#include "Vertex.hlsli"
#include "SharedTypes.hlsli"
#include "Constants.hlsli"
#include "Sampling.hlsli"
#include "Collision.hlsli"

float3 CartesianToSpherical(float x, float y, float z)
{
	float radius = sqrt(x * x + y * y + z * z);
	float theta = acos(z / radius);
	float phi = atan(y / x);
	return float3(radius, theta, phi);
}

float3 SphericalToCartesian(float Radius, float Theta, float Phi)
{
	float x = Radius * sin(Theta) * cos(Phi);
	float y = Radius * sin(Theta) * sin(Phi);
	float z = Radius * cos(Theta);
	return float3(x, y, z);
}

void CoordinateSystem(float3 V1, out float3 V2, out float3 V3)
{
	if (abs(V1.x) > abs(V1.y))
		V2 = float3(-V1.z, 0, V1.x) / sqrt(V1.x * V1.x + V1.z * V1.z);
	else
		V2 = float3(0, V1.z, -V1.y) / sqrt(V1.y * V1.y + V1.z * V1.z);
	V3 = normalize(cross(V1, V2));
}

float3x3 GetTBNMatrix(float3 Normal)
{
	float3 tangent, bitangent;
	CoordinateSystem(Normal, tangent, bitangent);
	return float3x3(tangent, bitangent, Normal);
}

float RGBToCIELuminance(float3 RGB)
{
	return dot(RGB, float3(0.212671f, 0.715160f, 0.072169f));
}

float NDCDepthToViewDepth(float NDCDepth, Camera Camera)
{
	return Camera.Projection[3][2] / (NDCDepth - Camera.Projection[2][2]);
}

float3 NDCDepthToViewPosition(float NDCDepth, float2 ScreenSpaceUV, Camera Camera)
{
	ScreenSpaceUV.y = 1.0f - ScreenSpaceUV.y; // Flip due to DirectX convention

	float z = NDCDepth;
	float2 xy = ScreenSpaceUV * 2.0f - 1.0f;

	float4 clipSpacePosition = float4(xy, z, 1.0f);
	float4 viewSpacePosition = mul(clipSpacePosition, Camera.InvProjection);

    // Perspective division
	viewSpacePosition /= viewSpacePosition.w;

	return viewSpacePosition.xyz;
}

// this is supposed to get the world position from the depth buffer
float3 NDCDepthToWorldPosition(float NDCDepth, float2 ScreenSpaceUV, Camera Camera)
{
	float4 viewSpacePosition = float4(NDCDepthToViewPosition(NDCDepth, ScreenSpaceUV, Camera), 1.0f);
	float4 worldSpacePosition = mul(viewSpacePosition, Camera.InvView);
	return worldSpacePosition.xyz;
}

#endif