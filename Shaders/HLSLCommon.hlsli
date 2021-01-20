#ifndef HLSL_COMMON_HLSLI
#define HLSL_COMMON_HLSLI

#include <Vertex.hlsli>
#include <SharedTypes.hlsli>
#include <Math.hlsli>
#include <Sampling.hlsli>
#include <Random.hlsli>
#include <BxDF.hlsli>

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
	float3 Tangent, Bitangent;
	CoordinateSystem(Normal, Tangent, Bitangent);
	return float3x3(Tangent, Bitangent, Normal);
}

// Returns a relative luminance of an input linear RGB color in the Rec. 709 color space
float RGBToCIELuminance(float3 RGB)
{
	return dot(RGB, float3(0.212671f, 0.715160f, 0.072169f));
}

float NDCDepthToViewDepth(float NDCDepth, Camera Camera)
{
	return Camera.Projection[3][2] / (NDCDepth - Camera.Projection[2][2]);
}

// Reconstructs view position from depth buffer
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

// Reconstructs world position from depth buffer
float3 NDCDepthToWorldPosition(float NDCDepth, float2 ScreenSpaceUV, Camera Camera)
{
	float4 viewSpacePosition = float4(NDCDepthToViewPosition(NDCDepth, ScreenSpaceUV, Camera), 1.0f);
	float4 worldSpacePosition = mul(viewSpacePosition, Camera.InvView);
	return worldSpacePosition.xyz;
}

float3 GenerateWorldCameraRayDirection(float2 ScreenSpaceUV, Camera Camera)
{
	return normalize(NDCDepthToWorldPosition(1.0f, ScreenSpaceUV, Camera) - Camera.Position.xyz);
}

float3 EncodeNormal(float3 n)
{
	return n * 0.5f + 0.5f;
}

float3 DecodeNormal(float3 n)
{
	return n * 2.0f - 1.0f;
}

//----------------------------------------------------------------------------------------------------

// Modified from [pbrt]
uint Halton2Inverse(uint Index, uint Digits)
{
	Index = (Index << 16) | (Index >> 16);
	Index = ((Index & 0x00ff00ff) << 8) | ((Index & 0xff00ff00) >> 8);
	Index = ((Index & 0x0f0f0f0f) << 4) | ((Index & 0xf0f0f0f0) >> 4);
	Index = ((Index & 0x33333333) << 2) | ((Index & 0xcccccccc) >> 2);
	Index = ((Index & 0x55555555) << 1) | ((Index & 0xaaaaaaaa) >> 1);
	return Index >> (32 - Digits);
}

// Modified from [pbrt]
uint Halton3Inverse(uint Index, uint Digits)
{
	uint result = 0;
	for (uint d = 0; d < Digits; ++d)
	{
		result = result * 3 + Index % 3;
		Index /= 3;
	}
	return result;
}

uint HaltonIndex(uint x, uint y, uint i, uint Increment)
{
	return ((Halton2Inverse(x % 256, 8) * 76545 +
      Halton3Inverse(y % 256, 6) * 110080) % Increment) + i * 186624;
}

struct HaltonState
{
	uint Dimension;
	uint SequenceIndex;
};

HaltonState InitHaltonState(int x, int y,
	int Path, int NumPaths,
	int FrameId,
	int Loop, uint Increment)
{
	HaltonState HaltonState;
	HaltonState.Dimension = 2;
	HaltonState.SequenceIndex = HaltonIndex(x, y, (FrameId * NumPaths + Path) % (Loop * NumPaths), Increment);
	return HaltonState;
}

float HaltonSample(uint Dimension, uint SampleIndex)
{
	static float PrimeNumbers[32] =
	{
		2, 3, 5, 7, 11, 13, 17, 19, 23, 29,
        31, 37, 41, 43, 47, 53, 59, 61, 67, 71,
        73, 79, 83, 89, 97, 101, 103, 107, 109, 113,
        127, 131
	};
	
	int base = Dimension < 32 ? PrimeNumbers[Dimension] : 2;

	// Compute the radical inverse.
	float a = 0;
	float invBase = 1.0f / float(base);
  
	for (float mult = invBase; SampleIndex != 0; SampleIndex /= base, mult *= invBase)
	{
		a += float(SampleIndex % base) * mult;
	}
  
	return a;
}

float HaltonNext(inout HaltonState HaltonState)
{
	return HaltonSample(HaltonState.Dimension++, HaltonState.SequenceIndex);
}

//
struct Triangle
{
    Vertex v0;
    Vertex v1;
    Vertex v2;
};

float BarycentricInterpolation(in float v0, in float v1, in float v2, in float3 barycentric)
{
    return v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
}

float2 BarycentricInterpolation(in float2 v0, in float2 v1, in float2 v2, in float3 barycentric)
{
    return v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
}

float3 BarycentricInterpolation(in float3 v0, in float3 v1, in float3 v2, in float3 barycentric)
{
    return v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
}

float4 BarycentricInterpolation(in float4 v0, in float4 v1, in float4 v2, in float3 barycentric)
{
    return v0 * barycentric.x + v1 * barycentric.y + v2 * barycentric.z;
}

Vertex BarycentricInterpolation(in Vertex v0, in Vertex v1, in Vertex v2, in float3 barycentric)
{
    Vertex vertex;
    vertex.Position     = BarycentricInterpolation(v0.Position, v1.Position, v2.Position, barycentric);
    vertex.Texture      = BarycentricInterpolation(v0.Texture, v1.Texture, v2.Texture, barycentric);
    vertex.Normal       = BarycentricInterpolation(v0.Normal, v1.Normal, v2.Normal, barycentric);

    return vertex;
}

Vertex BarycentricInterpolation(in Triangle t, in float3 barycentric)
{
	return BarycentricInterpolation(t.v0, t.v1, t.v2, barycentric);
}

#endif // HLSL_COMMON_HLSLI