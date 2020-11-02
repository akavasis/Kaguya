#ifndef RANDOM_HLSLI
#define RANDOM_HLSLI

#include "Constants.hlsli"

uint WangHash(inout uint seed)
{
	seed = uint(seed ^ uint(61)) ^ uint(seed >> uint(16));
	seed *= uint(9);
	seed = seed ^ (seed >> 4);
	seed *= uint(0x27d4eb2d);
	seed = seed ^ (seed >> 15);
	return seed;
}

float RandomFloat01(inout uint seed)
{
	return float(WangHash(seed)) / 4294967296.0;
}

float3 RandomUnitVector(inout uint seed)
{
	float z = RandomFloat01(seed) * 2.0f - 1.0f;
	float a = RandomFloat01(seed) * s_2PI;
	float r = sqrt(1.0f - z * z);
	float x = r * cos(a);
	float y = r * sin(a);
	return float3(x, y, z);
}

float3 RandomInUnitDisk(inout uint seed)
{
	while (true)
	{
		float3 p = float3(RandomFloat01(seed) * 2.0f - 1.0f, RandomFloat01(seed) * 2.0f - 1.0f, 0.0f);
		if (length(p) >= 1.0f)
			continue;
		return p;
	}
}

float3 RandomInUnitSphere(inout uint seed)
{
	while (true)
	{
		float3 p = float3(RandomFloat01(seed) * 2.0f - 1.0f, RandomFloat01(seed) * 2.0f - 1.0f, RandomFloat01(seed) * 2.0f - 1.0f);
		if (length(p) >= 1.0f)
			continue;
		return p;
	}
}

float3 RandomInHemisphere(inout uint seed, in float3 normal)
{
	float3 p = RandomInUnitSphere(seed);
	return dot(p, normal) > 0.0f ? p : -p;
}

float3 PerpendicularVector(in float3 v)
{
	float3 a = abs(v);
	uint xm = ((a.x - a.y) < 0 && (a.x - a.z) < 0) ? 1 : 0;
	uint ym = (a.y - a.z) < 0 ? (1 ^ xm) : 0;
	uint zm = 1 ^ (xm | ym);
	return cross(v, float3(xm, ym, zm));
}

float3 CosHemisphereSample(inout uint seed, in float3 normal)
{
	// Get 2 random numbers to select our sample with
	float2 randVal = float2(RandomFloat01(seed), RandomFloat01(seed));

	// Cosine weighted hemisphere sample from RNG
	float3 bitangent = PerpendicularVector(normal);
	float3 tangent = cross(bitangent, normal);
	float r = sqrt(randVal.x);
	float phi = 2.0f * 3.14159265f * randVal.y;

	// Get our cosine-weighted hemisphere lobe sample direction
	return tangent * (r * cos(phi)) + bitangent * (r * sin(phi)) + normal.xyz * sqrt(1 - randVal.x);
}

// https://stackoverflow.com/a/17479300
// A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
uint Hash(uint x)
{
	x += (x << 10u);
	x ^= (x >> 6u);
	x += (x << 3u);
	x ^= (x >> 11u);
	x += (x << 15u);
	return x;
}

// Compound versions of the hashing algorithm
uint Hash(uint2 v)
{
	return Hash(v.x ^ Hash(v.y));
}
uint Hash(uint3 v)
{
	return Hash(v.x ^ Hash(v.y) ^ Hash(v.z));
}
uint Hash(uint4 v)
{
	return Hash(v.x ^ Hash(v.y) ^ Hash(v.z) ^ Hash(v.w));
}

// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
float FloatConstruct(uint m)
{
	const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
	const uint ieeeOne = 0x3F800000u; // 1.0 in IEEE binary32

	m &= ieeeMantissa; // Keep only mantissa bits (fractional part)
	m |= ieeeOne; // Add fractional part to 1.0

	float f = asfloat(m); // Range [1:2]
	return f - 1.0; // Range [0:1]
}

// Pseudo-random value in half-open range [0:1].
float Random(float x)
{
	return FloatConstruct(Hash(asuint(x)));
}
float Random(float2 v)
{
	return FloatConstruct(Hash(asuint(v)));
}
float Random(float3 v)
{
	return FloatConstruct(Hash(asuint(v)));
}
float Random(float4 v)
{
	return FloatConstruct(Hash(asuint(v)));
}

#endif