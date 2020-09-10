#ifndef __RANDOM_HLSLI__
#define __RANDOM_HLSLI__
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

#endif