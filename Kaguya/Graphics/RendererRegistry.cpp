#include "pch.h"
#include "RendererRegistry.h"
#include <Core/Application.h>

void Shaders::Register(RenderDevice* pRenderDevice)
{
	const auto& ExecutableFolderPath = Application::ExecutableFolderPath;

	// Load VS
	{
		VS::Quad										= pRenderDevice->CompileShader(Shader::Type::Vertex, ExecutableFolderPath / L"Shaders/Quad.hlsl",			L"VSMain", {});
		VS::Skybox										= pRenderDevice->CompileShader(Shader::Type::Vertex, ExecutableFolderPath / L"Shaders/Skybox.hlsli",		L"VSMain", {});
		VS::GBufferMeshes								= pRenderDevice->CompileShader(Shader::Type::Vertex, ExecutableFolderPath / L"Shaders/GBufferMeshes.hlsl",	L"VSMain", {});
		VS::GBufferLights								= pRenderDevice->CompileShader(Shader::Type::Vertex, ExecutableFolderPath / L"Shaders/GBufferLights.hlsl",	L"VSMain", {});
	}

	// Load PS
	{
		PS::ConvolutionIrradiance						= pRenderDevice->CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/ConvolutionIrradiance.hlsl",	L"PSMain", {});
		PS::ConvolutionPrefilter						= pRenderDevice->CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/ConvolutionPrefilter.hlsl",	L"PSMain", {});
		PS::GBufferMeshes								= pRenderDevice->CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/GBufferMeshes.hlsl",			L"PSMain", {});
		PS::GBufferLights								= pRenderDevice->CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/GBufferLights.hlsl",			L"PSMain", {});

		PS::PostProcess_Tonemapping						= pRenderDevice->CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/PostProcess/Tonemapping.hlsl",	L"PSMain", {});
	}

	// Load CS
	{
		CS::EquirectangularToCubemap					= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/EquirectangularToCubemap.hlsl",					L"CSMain", {});
		CS::GenerateMips								= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/GenerateMips.hlsl",								L"CSMain", {});

		CS::EstimateNoise								= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/Denoiser/EstimateNoise.hlsl",					L"CSMain", {});
		CS::FilterNoise									= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/Denoiser/FilterNoise.hlsl",						L"CSMain", {});
		CS::Denoise										= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/Denoiser/Denoise.hlsl",							L"CSMain", {});
		CS::WeightedShadowComposition					= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/Denoiser/WeightedShadowComposition.hlsl",		L"CSMain", {});
		CS::ShadingComposition							= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/ShadingComposition.hlsl",						L"CSMain", {});

		CS::Accumulation								= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/Raytracing/Accumulation.hlsl",					L"CSMain", {});

		CS::PostProcess_BloomMask						= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomMask.hlsl",						L"CSMain", {});
		CS::PostProcess_BloomDownsample					= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomDownsample.hlsl",				L"CSMain", {});
		CS::PostProcess_BloomBlur						= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomBlur.hlsl",						L"CSMain", {});
		CS::PostProcess_BloomUpsampleBlurAccumulation	= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomUpsampleBlurAccumulation.hlsl", L"CSMain", {});
		CS::PostProcess_BloomComposition				= pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomComposition.hlsl",				L"CSMain", {});
	}
}

void Libraries::Register(RenderDevice* pRenderDevice)
{
	const auto& ExecutableFolderPath = Application::ExecutableFolderPath;

	Pathtracing			= pRenderDevice->CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/Pathtracing.hlsl");
	AmbientOcclusion	= pRenderDevice->CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/AmbientOcclusion.hlsl");
	Shading				= pRenderDevice->CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/Shading.hlsl");
}

void RootSignatures::Register(RenderDevice* pRenderDevice)
{
	// Cubemap convolutions RS
	for (int i = 0; i < CubemapConvolution::NumCubemapConvolutions; ++i)
	{
		RenderResourceHandle rootSignatureHandle = pRenderDevice->CreateRootSignature([i](RootSignatureProxy& proxy)
		{
			UINT Num32BitValues = i == Irradiance ? sizeof(ConvolutionIrradianceSettings) / 4 : sizeof(ConvolutionPrefilterSettings) / 4;

			proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, Num32BitValues));	// Settings					b0 | space0
			proxy.AddRootConstantsParameter(RootConstants<void>(1, 0, 1));				// RootConstants			b1 | space0
			proxy.AddRootSRVParameter(RootSRV(0, 0));									// Vertex Buffer			t1 | space0
			proxy.AddRootSRVParameter(RootSRV(1, 0));									// Index Buffer				t2 | space0
			proxy.AddRootSRVParameter(RootSRV(2, 0));									// Geometry info Buffer		t3 | space0

			proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);

			proxy.DenyTessellationShaderAccess();
			proxy.DenyGSAccess();
		});

		switch (i)
		{
		case Irradiance: ConvolutionIrradiace = rootSignatureHandle; break;
		case Prefilter: ConvolutionPrefilter = rootSignatureHandle; break;
		}
	}

	// Generate mips RS
	GenerateMips = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<GenerateMipsData>(0, 0));

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);

		proxy.DenyVSAccess();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// Equirectangular to cubemap RS
	EquirectangularToCubemap = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<EquirectangularToCubemapData>(0, 0));

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);

		proxy.DenyVSAccess();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	Skybox = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<void>(1, 0, 1));				// RootConstants			b1 | space0
		proxy.AddRootSRVParameter(RootSRV(0, 0));									// Vertex Buffer			t1 | space0
		proxy.AddRootSRVParameter(RootSRV(1, 0));									// Index Buffer				t2 | space0
		proxy.AddRootSRVParameter(RootSRV(2, 0));									// Geometry info Buffer		t3 | space0

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// Empty Local RS
	Raytracing::EmptyLocal = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.SetAsLocalRootSignature();
	},
		false);

	// Global RS
	Raytracing::Global = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.AddRootSRVParameter(RootSRV(0, 0));	// BVH,						t0 | space0
		proxy.AddRootSRVParameter(RootSRV(1, 0));	// Vertex Buffer,			t1 | space0
		proxy.AddRootSRVParameter(RootSRV(2, 0));	// Index Buffer,			t2 | space0
		proxy.AddRootSRVParameter(RootSRV(3, 0));	// Geometry info Buffer,	t3 | space0
		proxy.AddRootSRVParameter(RootSRV(4, 0));	// Light Buffer,			t4 | space0
		proxy.AddRootSRVParameter(RootSRV(5, 0));	// Material Buffer,			t5 | space0

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);	// SamplerLinearWrap	s0 | space0;
		proxy.AddStaticSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);	// SamplerLinearClamp	s1 | space0;
	});
}

void GraphicsPSOs::Register(RenderDevice* pRenderDevice)
{
	for (int i = 0; i < CubemapConvolution::NumCubemapConvolutions; ++i)
	{
		const RootSignature* pRS = i == CubemapConvolution::Irradiance ? pRenderDevice->GetRootSignature(RootSignatures::ConvolutionIrradiace)
			: pRenderDevice->GetRootSignature(RootSignatures::ConvolutionPrefilter);
		const Shader* pVS = &Shaders::VS::Skybox;
		const Shader* pPS = i == CubemapConvolution::Irradiance ? &Shaders::PS::ConvolutionIrradiance : &Shaders::PS::ConvolutionPrefilter;
		DXGI_FORMAT rtvFormat = i == CubemapConvolution::Irradiance ? RendererFormats::IrradianceFormat : RendererFormats::PrefilterFormat;

		RenderResourceHandle handle = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
		{
			proxy.pRootSignature = pRS;
			proxy.pVS = pVS;
			proxy.pPS = pPS;

			proxy.RasterizerState.SetCullMode(RasterizerState::CullMode::None);

			proxy.PrimitiveTopology = PrimitiveTopology::Triangle;
			proxy.AddRenderTargetFormat(rtvFormat);
		});

		switch (i)
		{
		case Irradiance: ConvolutionIrradiace = handle; break;
		case Prefilter: ConvolutionPrefilter = handle; break;
		}
	}
}

void ComputePSOs::Register(RenderDevice* pRenderDevice)
{
	GenerateMips = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::GenerateMips);
		proxy.pCS = &Shaders::CS::GenerateMips;
	});

	EquirectangularToCubemap = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::EquirectangularToCubemap);
		proxy.pCS = &Shaders::CS::EquirectangularToCubemap;
	});
}