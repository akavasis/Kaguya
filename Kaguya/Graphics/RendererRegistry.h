#pragma once
#include "RenderDevice.h"

static constexpr size_t SizeOfBuiltInTriangleIntersectionAttributes = 2 * sizeof(float);
static constexpr size_t SizeOfHLSLBooleanType = sizeof(int);

#define ENUM_TO_LSTR(Enum) L#Enum

enum CubemapConvolution
{
	Irradiance,
	Prefilter,
	NumCubemapConvolutions
};

struct Resolutions
{
	static constexpr UINT64 Irradiance = 64;
	static constexpr UINT64 Prefilter = 512;
};

struct ConvolutionIrradianceSettings
{
	int CubemapIndex;
};

struct ConvolutionPrefilterSettings
{
	float Roughness;
	int CubemapIndex;
};

struct GenerateMipsData
{
	UINT SrcMipLevel;				// Texture level of source mip
	UINT NumMipLevels;				// Number of OutMips to write: [1-4]
	UINT SrcDimension;				// Width and height of the source texture are even or odd.
	UINT IsSRGB;					// Must apply gamma correction to sRGB textures.
	DirectX::XMFLOAT2 TexelSize;    // 1.0 / OutMip1.Dimensions

	// Source mip map.
	int InputIndex;

	// Write up to 4 mip map levels.
	int Output1Index;
	int Output2Index;
	int Output3Index;
	int Output4Index;
};

struct EquirectangularToCubemapData
{
	UINT CubemapSize;				// Size of the cubemap face in pixels at the current mipmap level.
	UINT FirstMip;					// The first mip level to generate.
	UINT NumMips;					// The number of mips to generate.

	// Source texture as an equirectangular panoramic image.
	// It is assumed that the src texture has a full mipmap chain.
	int InputIndex;

	// Destination texture as a mip slice in the cubemap texture (texture array with 6 elements).
	int Output1Index;
	int Output2Index;
	int Output3Index;
	int Output4Index;
	int Output5Index;
};

struct RendererFormats
{
	static constexpr DXGI_FORMAT HDRBufferFormat		= DXGI_FORMAT_R32G32B32A32_FLOAT;
	static constexpr DXGI_FORMAT IrradianceFormat		= DXGI_FORMAT_R32G32B32A32_FLOAT;
	static constexpr DXGI_FORMAT PrefilterFormat		= DXGI_FORMAT_R32G32B32A32_FLOAT;
};

struct Shaders
{
	struct VS
	{
		inline static Shader Quad;
		inline static Shader Skybox;
		inline static Shader GBufferMeshes;
		inline static Shader GBufferLights;
	};

	struct PS
	{
		inline static Shader ConvolutionIrradiance;
		inline static Shader ConvolutionPrefilter;
		inline static Shader GBufferMeshes;
		inline static Shader GBufferLights;

		inline static Shader PostProcess_Tonemapping;
	};

	struct CS
	{
		inline static Shader EquirectangularToCubemap;
		inline static Shader GenerateMips;

		inline static Shader ShadingComposition;

		inline static Shader Accumulation;

		inline static Shader PostProcess_BloomMask;
		inline static Shader PostProcess_BloomDownsample;
		inline static Shader PostProcess_BloomBlur;
		inline static Shader PostProcess_BloomUpsampleBlurAccumulation;
		inline static Shader PostProcess_BloomComposition;
	};

	static void Register(RenderDevice* pRenderDevice);
};

struct Libraries
{
	inline static Library Pathtracing;
	inline static Library AmbientOcclusion;
	inline static Library Shading;

	static void Register(RenderDevice* pRenderDevice);
};

struct RootSignatures
{
	inline static RenderResourceHandle ConvolutionIrradiace;
	inline static RenderResourceHandle ConvolutionPrefilter;
	inline static RenderResourceHandle GenerateMips;
	inline static RenderResourceHandle EquirectangularToCubemap;

	inline static RenderResourceHandle Skybox;
	inline static RenderResourceHandle GBufferMeshes;
	inline static RenderResourceHandle GBufferLights;
	inline static RenderResourceHandle ShadingComposition;

	inline static RenderResourceHandle PostProcess_Tonemapping;
	inline static RenderResourceHandle PostProcess_BloomMask;
	inline static RenderResourceHandle PostProcess_BloomDownsample;
	inline static RenderResourceHandle PostProcess_BloomBlur;
	inline static RenderResourceHandle PostProcess_BloomUpsampleBlurAccumulation;
	inline static RenderResourceHandle PostProcess_BloomComposition;

	struct Raytracing
	{
		inline static RenderResourceHandle Accumulation;
		inline static RenderResourceHandle EmptyLocal;

		inline static RenderResourceHandle Global;
	};

	static void Register(RenderDevice* pRenderDevice);
};

struct GraphicsPSOs
{
	inline static RenderResourceHandle ConvolutionIrradiace;
	inline static RenderResourceHandle ConvolutionPrefilter;

	inline static RenderResourceHandle GBufferMeshes;
	inline static RenderResourceHandle GBufferLights;

	inline static RenderResourceHandle PostProcess_Tonemapping;

	static void Register(RenderDevice* pRenderDevice);
};

struct ComputePSOs
{
	inline static RenderResourceHandle GenerateMips;
	inline static RenderResourceHandle EquirectangularToCubemap;

	inline static RenderResourceHandle ShadingComposition;

	inline static RenderResourceHandle Accumulation;

	inline static RenderResourceHandle PostProcess_BloomMask;
	inline static RenderResourceHandle PostProcess_BloomDownsample;
	inline static RenderResourceHandle PostProcess_BloomBlur;
	inline static RenderResourceHandle PostProcess_BloomUpsampleBlurAccumulation;
	inline static RenderResourceHandle PostProcess_BloomComposition;

	static void Register(RenderDevice* pRenderDevice);
};

struct RaytracingPSOs
{
	inline static RenderResourceHandle Pathtracing;
	inline static RenderResourceHandle AmbientOcclusion;
	inline static RenderResourceHandle Shading;
};