#pragma once
#include "RenderDevice.h"

enum CubemapConvolution
{
	Irradiance,
	Prefilter,
	CubemapConvolution_Count
};

struct Resolutions
{
	static constexpr UINT64 BRDFLUT = 512;
	static constexpr UINT64 Irradiance = 64;
	static constexpr UINT64 Prefilter = 512;
	static constexpr UINT64 SunShadowMapResolution = 2048;
};

struct ConvolutionIrradianceSetting
{
	float DeltaPhi = (2.0f * float(DirectX::XM_PI)) / 180.0f;
	float DeltaTheta = (0.5f * float(DirectX::XM_PI)) / 64.0f;
};

struct ConvolutionPrefilterSetting
{
	float Roughness;
	UINT NumSamples = 32u;
};

struct GenerateMipsData
{
	UINT SrcMipLevel;				// Texture level of source mip
	UINT NumMipLevels;				// Number of OutMips to write: [1-4]
	UINT SrcDimension;				// Width and height of the source texture are even or odd.
	UINT IsSRGB;					// Must apply gamma correction to sRGB textures.
	DirectX::XMFLOAT2 TexelSize;    // 1.0 / OutMip1.Dimensions
};

struct EquirectangularToCubemapData
{
	UINT CubemapSize;				// Size of the cubemap face in pixels at the current mipmap level.
	UINT FirstMip;					// The first mip level to generate.
	UINT NumMips;					// The number of mips to generate.
};

struct TonemapData
{
	float Exposure = 4.5f;
	float Gamma = 2.2f;
	unsigned int InputMapIndex;
};

struct RendererFormats
{
	static constexpr DXGI_FORMAT SwapChainBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr DXGI_FORMAT HDRBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static constexpr DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	static constexpr DXGI_FORMAT BRDFLUTFormat = DXGI_FORMAT_R16G16_FLOAT;
	static constexpr DXGI_FORMAT IrradianceFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	static constexpr DXGI_FORMAT PrefilterFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
};

struct RootParameters
{
	struct StandardShaderLayout
	{
		enum
		{
			ConstantDataCB,
			RenderPassDataCB,
			DescriptorTables,
			NumRootParameters
		};
	};
	struct CubemapConvolution
	{
		enum
		{
			Setting,
			RenderPassCBuffer,
			CubemapSRV
		};
	};
	struct GenerateMips
	{
		enum
		{
			GenerateMipsCBuffer,
			SrcMip,
			OutMips
		};
	};
	struct EquirectangularToCubemap
	{
		enum
		{
			PanoToCubemapCBuffer,
			SrcMip,
			OutMips
		};
	};
	struct PBR
	{
		enum
		{
			MaterialTextureIndicesSBuffer,
			MaterialTexturePropertiesSBuffer,
			NumRootParameters
		};
	};
	struct Raytracing
	{
		enum
		{
			GeometryTable,
			RenderTarget,
			Camera
		};

		struct HitGroup
		{
			enum
			{
				HitInfo
			};
		};
	};
};

struct Shaders
{
	struct VS
	{
		inline static RenderResourceHandle Default;
		inline static RenderResourceHandle Quad;
		inline static RenderResourceHandle Shadow;
		inline static RenderResourceHandle Skybox;
	};

	struct PS
	{
		inline static RenderResourceHandle BRDFIntegration;
		inline static RenderResourceHandle ConvolutionIrradiance;
		inline static RenderResourceHandle ConvolutionPrefilter;
		inline static RenderResourceHandle PBR;
		inline static RenderResourceHandle Skybox;

		inline static RenderResourceHandle PostProcess_Tonemapping;
	};

	struct CS
	{
		inline static RenderResourceHandle EquirectangularToCubemap;
		inline static RenderResourceHandle GenerateMips;
	};

	static void Register(RenderDevice* pRenderDevice);
};

struct Libraries
{
	inline static RenderResourceHandle Raytrace;

	static void Register(RenderDevice* pRenderDevice);
};

struct RootSignatures
{
	inline static RenderResourceHandle BRDFIntegration;
	inline static RenderResourceHandle ConvolutionIrradiace;
	inline static RenderResourceHandle ConvolutionPrefilter;
	inline static RenderResourceHandle GenerateMips;
	inline static RenderResourceHandle EquirectangularToCubemap;

	inline static RenderResourceHandle PBR;
	inline static RenderResourceHandle Shadow;
	inline static RenderResourceHandle Skybox;

	inline static RenderResourceHandle PostProcess_Tonemapping;

	struct Raytracing
	{
		inline static RenderResourceHandle Global;
		inline static RenderResourceHandle EmptyLocal;
		inline static RenderResourceHandle HitGroup;
	};

	static void Register(RenderDevice* pRenderDevice);
};

struct GraphicsPSOs
{
	inline static RenderResourceHandle BRDFIntegration;
	inline static RenderResourceHandle ConvolutionIrradiace;
	inline static RenderResourceHandle ConvolutionPrefilter;

	inline static RenderResourceHandle PBR;
	inline static RenderResourceHandle Shadow;
	inline static RenderResourceHandle Skybox;

	inline static RenderResourceHandle PostProcess_Tonemapping;

	static void Register(RenderDevice* pRenderDevice);
};

struct ComputePSOs
{
	inline static RenderResourceHandle GenerateMips;
	inline static RenderResourceHandle EquirectangularToCubemap;

	static void Register(RenderDevice* pRenderDevice);
};

struct RaytracingPSOs
{
	inline static RenderResourceHandle Raytracing;

	static void Register(RenderDevice* pRenderDevice);
};