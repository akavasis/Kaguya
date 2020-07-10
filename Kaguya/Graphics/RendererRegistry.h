#pragma once
#include "RenderDevice.h"

enum CubemapConvolution
{
	Irradiance,
	Prefilter,
	CubemapConvolution_Count
};

struct CubemapConvolutionResolution
{
	static constexpr UINT Irradiance = 64u;
	static constexpr UINT Prefilter = 512u;
};

struct IrradianceConvolutionSetting
{
	float DeltaPhi = (2.0f * float(DirectX::XM_PI)) / 180.0f;
	float DeltaTheta = (0.5f * float(DirectX::XM_PI)) / 64.0f;
};

struct PrefilterConvolutionSetting
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
	enum Shadow
	{
		ShadowRootParameter_ObjectCBuffer,
		ShadowRootParameter_ShadowPassCBuffer,
		ShadowRootParameter_Count
	};
	enum PBR
	{
		PBRRootParameter_ObjectCBuffer,
		PBRRootParameter_MaterialCBuffer,
		PBRRootParameter_RenderPassCBuffer,
		PBRRootParameter_MaterialTexturesSRV,
		PBRRootParameter_ShadowMapAndEnvironmentMapsSRV,
		PBRRootParameter_Count
	};
	enum Skybox
	{
		SkyboxRootParameter_RenderPassCBuffer,
		SkyboxRootParameter_CubeMapSRV,
		SkyboxRootParameter_Count
	};
	enum CubemapConvolution
	{
		CubemapConvolutionRootParameter_RenderPassCBuffer,
		CubemapConvolutionRootParameter_Setting,
		CubemapConvolutionRootParameter_CubemapSRV,
		CubemapConvolutionRootParameter_Count,
	};
	enum GenerateMips
	{
		GenerateMipsRootParameter_GenerateMipsCB,
		GenerateMipsRootParameter_SrcMip,
		GenerateMipsRootParameter_OutMip,
		GenerateMipsRootParameter_Count
	};
	enum EquirectangularToCubemap
	{
		EquirectangularToCubemapRootParameter_PanoToCubemapCB,
		EquirectangularToCubemapRootParameter_SrcTexture,
		EquirectangularToCubemapRootParameter_DstMips,
		EquirectangularToCubemapRootParameter_Count
	};
};

struct Shaders
{
	inline static RenderResourceHandle VS_Default;
	inline static RenderResourceHandle VS_Quad;
	inline static RenderResourceHandle VS_Sky;
	inline static RenderResourceHandle VS_Shadow;

	inline static RenderResourceHandle PS_Default;
	inline static RenderResourceHandle PS_BlinnPhong;
	inline static RenderResourceHandle PS_PBR;
	inline static RenderResourceHandle PS_Sepia;
	inline static RenderResourceHandle PS_SobelComposite;
	inline static RenderResourceHandle PS_BloomMask;
	inline static RenderResourceHandle PS_BloomComposite;
	inline static RenderResourceHandle PS_ToneMapping;
	inline static RenderResourceHandle PS_Sky;
	inline static RenderResourceHandle PS_IrradianceConvolution;
	inline static RenderResourceHandle PS_PrefilterConvolution;
	inline static RenderResourceHandle PS_BRDFIntegration;

	inline static RenderResourceHandle CS_HorizontalBlur;
	inline static RenderResourceHandle CS_VerticalBlur;
	inline static RenderResourceHandle CS_Sobel;
	inline static RenderResourceHandle CS_EquirectangularToCubeMap;
	inline static RenderResourceHandle CS_GenerateMips;

	static void Register(RenderDevice* pRenderDevice);
};

struct RootSignatures
{
	inline static RenderResourceHandle Shadow;
	inline static RenderResourceHandle PBR;
	inline static RenderResourceHandle Skybox;

	inline static RenderResourceHandle BRDFIntegration;
	inline static RenderResourceHandle IrradiaceConvolution;
	inline static RenderResourceHandle PrefilterConvolution;

	inline static RenderResourceHandle GenerateMips;
	inline static RenderResourceHandle EquirectangularToCubemap;

	static void Register(RenderDevice* pRenderDevice);
};

struct GraphicsPSOs
{
	inline static RenderResourceHandle Shadow;
	inline static RenderResourceHandle PBR, PBRWireFrame;
	inline static RenderResourceHandle Skybox;

	inline static RenderResourceHandle BRDFIntegration;
	inline static RenderResourceHandle IrradiaceConvolution;
	inline static RenderResourceHandle PrefilterConvolution;

	static void Register(RenderDevice* pRenderDevice);
};

struct ComputePSOs
{
	inline static RenderResourceHandle GenerateMips;
	inline static RenderResourceHandle EquirectangularToCubemap;

	static void Register(RenderDevice* pRenderDevice);
};