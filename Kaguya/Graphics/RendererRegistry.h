#pragma once
#include "RenderDevice.h"

// Gives a float2 barycentric, the last element can be found via 1 - b.x - b.y since barycentrics must add up to 1
static constexpr size_t SizeOfBuiltInTriangleIntersectionAttributes = 2 * sizeof(float);
static constexpr size_t SizeOfHLSLBooleanType = sizeof(int);

struct Shaders
{
	// Vertex Shaders
	struct VS
	{
		inline static Shader FullScreenTriangle;
	};

	// Mesh Shaders
	struct MS
	{
	};

	// Pixel Shaders
	struct PS
	{
		inline static Shader ToneMap;
	};

	// Compute Shaders
	struct CS
	{
		inline static Shader InstanceGeneration;

		inline static Shader PostProcess_BloomMask;
		inline static Shader PostProcess_BloomDownsample;
		inline static Shader PostProcess_BloomBlur;
		inline static Shader PostProcess_BloomUpsampleBlurAccumulation;
		inline static Shader PostProcess_BloomComposition;
	};

	static void Register(const ShaderCompiler& ShaderCompiler);
};

struct Libraries
{
	inline static Library PathTrace;
	inline static Library Picking;

	static void Register(const ShaderCompiler& ShaderCompiler);
};