#pragma once
#include "Scene/Light.h"
#include "Scene/SharedTypes.h"
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

struct GenerateMipsCPUData
{
	UINT SrcMipLevel;				// Texture level of source mip
	UINT NumMipLevels;				// Number of OutMips to write: [1-4]
	UINT SrcDimension;				// Width and height of the source texture are even or odd.
	UINT IsSRGB;					// Must apply gamma correction to sRGB textures.
	DirectX::XMFLOAT2 TexelSize;    // 1.0 / OutMip1.Dimensions
};

struct EquirectangularToCubemapCPUData
{
	UINT CubemapSize;				// Size of the cubemap face in pixels at the current mipmap level.
	UINT FirstMip;					// The first mip level to generate.
	UINT NumMips;					// The number of mips to generate.
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
	struct CubemapConvolution
	{
		enum
		{
			RenderPassCBuffer,
			Setting,
			CubemapSRV,
			Count,
		};
	};
	struct Shadow
	{
		enum
		{
			ObjectCBuffer,
			RenderPassCBuffer,
			Count
		};
	};
	struct PBR
	{
		enum
		{
			ObjectCBuffer,
			RenderPassCBuffer,
			MaterialTextureIndicesSBuffer,
			DescriptorTables,
			Count
		};
	};
	struct Skybox
	{
		enum
		{
			RenderPassCBuffer,
			MaterialTextureIndicesSBuffer,
			DescriptorTables,
			Count
		};
	};
	struct GenerateMips
	{
		enum
		{
			GenerateMipsCBuffer,
			SrcMip,
			OutMips,
			Count
		};
	};
	struct EquirectangularToCubemap
	{
		enum
		{
			PanoToCubemapCBuffer,
			SrcMip,
			OutMips,
			Count
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
		inline static RenderResourceHandle Sky;
	};

	struct PS
	{
		inline static RenderResourceHandle BRDFIntegration;
		inline static RenderResourceHandle ConvolutionIrradiance;
		inline static RenderResourceHandle ConvolutionPrefilter;
		inline static RenderResourceHandle PBR;
		inline static RenderResourceHandle Sky;
		inline static RenderResourceHandle PostProcess_BloomComposite;
		inline static RenderResourceHandle PostProcess_BloomMask;
		inline static RenderResourceHandle PostProcess_Sepia;
		inline static RenderResourceHandle PostProcess_SobelComposite;
		inline static RenderResourceHandle PostProcess_ToneMapping;
	};

	struct CS
	{
		inline static RenderResourceHandle EquirectangularToCubemap;
		inline static RenderResourceHandle GenerateMips;
		inline static RenderResourceHandle PostProcess_BlurHorizontal;
		inline static RenderResourceHandle PostProcess_BlurVertical;
		inline static RenderResourceHandle PostProcess_Sobel;
	};

	static void Register(RenderDevice* pRenderDevice);
};

struct RootSignatures
{
	inline static RenderResourceHandle BRDFIntegration;
	inline static RenderResourceHandle ConvolutionIrradiace;
	inline static RenderResourceHandle ConvolutionPrefilter;

	inline static RenderResourceHandle PBR;
	inline static RenderResourceHandle Shadow;
	inline static RenderResourceHandle Skybox;

	inline static RenderResourceHandle GenerateMips;
	inline static RenderResourceHandle EquirectangularToCubemap;

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

	static void Register(RenderDevice* pRenderDevice);
};

struct ComputePSOs
{
	inline static RenderResourceHandle GenerateMips;
	inline static RenderResourceHandle EquirectangularToCubemap;

	static void Register(RenderDevice* pRenderDevice);
};