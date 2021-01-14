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
		inline static Shader Quad;
	};

	// Mesh Shaders
	struct MS
	{
		inline static Shader GBufferMeshes;
		inline static Shader GBufferLights;
	};

	// Pixel Shaders
	struct PS
	{
		inline static Shader GBufferMeshes;
		inline static Shader GBufferLights;

		inline static Shader PostProcess_Tonemapping;
	};

	// Compute Shaders
	struct CS
	{
		inline static Shader InstanceGeneration;

		inline static Shader SVGF_Reproject;
		inline static Shader SVGF_FilterMoments;
		inline static Shader SVGF_Atrous;

		inline static Shader ShadingComposition;

		inline static Shader Accumulation;

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
	inline static Library Pathtracing;
	inline static Library AmbientOcclusion;
	inline static Library Shading;
	inline static Library Picking;

	static void Register(const ShaderCompiler& ShaderCompiler);
};

struct RootSignatures
{
	inline static RenderResourceHandle Default;

	inline static RenderResourceHandle Skybox;
	inline static RenderResourceHandle GBufferMeshes;
	inline static RenderResourceHandle GBufferLights;

	inline static RenderResourceHandle PostProcess_Tonemapping;
	inline static RenderResourceHandle PostProcess_BloomMask;
	inline static RenderResourceHandle PostProcess_BloomDownsample;
	inline static RenderResourceHandle PostProcess_BloomBlur;
	inline static RenderResourceHandle PostProcess_BloomUpsampleBlurAccumulation;
	inline static RenderResourceHandle PostProcess_BloomComposition;

	inline static RenderResourceHandle InstanceGeneration;

	struct Raytracing
	{
		struct Local
		{
			// Includes a VB and IB root parameter
			inline static RenderResourceHandle Default;
			inline static RenderResourceHandle Picking;
		};

		inline static RenderResourceHandle Global;
	};

	static void Register(RenderDevice* pRenderDevice);
};

struct GraphicsPSOs
{
	inline static RenderResourceHandle GBufferMeshes;
	inline static RenderResourceHandle GBufferLights;

	inline static RenderResourceHandle PostProcess_Tonemapping;

	static void Register(RenderDevice* pRenderDevice);
};

struct ComputePSOs
{
	inline static RenderResourceHandle SVGF_Reproject;
	inline static RenderResourceHandle SVGF_FilterMoments;
	inline static RenderResourceHandle SVGF_Atrous;

	inline static RenderResourceHandle ShadingComposition;

	inline static RenderResourceHandle Accumulation;

	inline static RenderResourceHandle PostProcess_BloomMask;
	inline static RenderResourceHandle PostProcess_BloomDownsample;
	inline static RenderResourceHandle PostProcess_BloomBlur;
	inline static RenderResourceHandle PostProcess_BloomUpsampleBlurAccumulation;
	inline static RenderResourceHandle PostProcess_BloomComposition;

	inline static RenderResourceHandle InstanceGeneration;

	static void Register(RenderDevice* pRenderDevice);
};

struct RaytracingPSOs
{
	inline static RenderResourceHandle Pathtracing;
	inline static RenderResourceHandle AmbientOcclusion;
	inline static RenderResourceHandle Shading;
	inline static RenderResourceHandle Picking;

	static void Register(RenderDevice* pRenderDevice);
};