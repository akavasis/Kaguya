#include "pch.h"
#include "RendererRegistry.h"
#include <Core/Application.h>

#define InitRootSignature(Handle) Handle = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::RootSignature, "Root Signature["#Handle"]")
#define InitGraphicsPSO(Handle) Handle = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::GraphicsPSO, "Graphics PSO["#Handle"]")
#define InitComputePSO(Handle) Handle = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::ComputePSO, "Compute PSO["#Handle"]")
#define InitRaytracingPSO(Handle) Handle = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::RaytracingPSO, "Raytracing PSO["#Handle"]")

const wchar_t* VSEntryPoint = L"VSMain";
const wchar_t* MSEntryPoint = L"MSMain";
const wchar_t* PSEntryPoint = L"PSMain";
const wchar_t* CSEntryPoint = L"CSMain";

void Shaders::Register(RenderDevice* pRenderDevice)
{
	const auto& ExecutableFolderPath = Application::ExecutableFolderPath;

	// Load VS
	{
		VS::Quad										= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Vertex, ExecutableFolderPath / L"Shaders/Quad.hlsl",		VSEntryPoint, {});
		VS::Skybox										= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Vertex, ExecutableFolderPath / L"Shaders/Skybox.hlsli",		VSEntryPoint, {});
	}

	// Load MS
	{
		MS::GBufferMeshes								= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Mesh, ExecutableFolderPath / L"Shaders/GBufferMeshes.hlsl", MSEntryPoint, {});
		MS::GBufferLights								= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Mesh, ExecutableFolderPath / L"Shaders/GBufferLights.hlsl", MSEntryPoint, {});
	}

	// Load PS
	{
		PS::ConvolutionIrradiance						= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/ConvolutionIrradiance.hlsl",	PSEntryPoint, {});
		PS::ConvolutionPrefilter						= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/ConvolutionPrefilter.hlsl",		PSEntryPoint, {});

		PS::GBufferMeshes								= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/GBufferMeshes.hlsl",			PSEntryPoint, {});
		PS::GBufferLights								= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/GBufferLights.hlsl",			PSEntryPoint, {});

		PS::PostProcess_Tonemapping						= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/PostProcess/Tonemapping.hlsl",	PSEntryPoint, {});
	}

	// Load CS
	{
		CS::EquirectangularToCubemap					= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/EquirectangularToCubemap.hlsl",	CSEntryPoint, {});
		CS::GenerateMips								= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/GenerateMips.hlsl",				CSEntryPoint, {});

		CS::SVGF_Reproject								= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/SVGF/SVGF_Reproject.hlsl",		CSEntryPoint, {});
		CS::SVGF_FilterMoments							= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/SVGF/SVGF_FilterMoments.hlsl",	CSEntryPoint, {});
		CS::SVGF_Atrous									= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/SVGF/SVGF_Atrous.hlsl",			CSEntryPoint, {});

		CS::ShadingComposition							= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/ShadingComposition.hlsl",			CSEntryPoint, {});

		CS::Accumulation								= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/Accumulation.hlsl",				CSEntryPoint, {});

		CS::PostProcess_BloomMask						= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomMask.hlsl",						CSEntryPoint, {});
		CS::PostProcess_BloomDownsample					= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomDownsample.hlsl",				CSEntryPoint, {});
		CS::PostProcess_BloomBlur						= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomBlur.hlsl",						CSEntryPoint, {});
		CS::PostProcess_BloomUpsampleBlurAccumulation	= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomUpsampleBlurAccumulation.hlsl",	CSEntryPoint, {});
		CS::PostProcess_BloomComposition				= pRenderDevice->ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomComposition.hlsl",				CSEntryPoint, {});
	}
}

void Libraries::Register(RenderDevice* pRenderDevice)
{
	const auto& ExecutableFolderPath = Application::ExecutableFolderPath;

	//Pathtracing			= pRenderDevice->ShaderCompiler.CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/Pathtracing.hlsl");
	//AmbientOcclusion	= pRenderDevice->ShaderCompiler.CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/AmbientOcclusion.hlsl");
	Shading				= pRenderDevice->ShaderCompiler.CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/Shading.hlsl");
}

void RootSignatures::Register(RenderDevice* pRenderDevice)
{
	InitRootSignature(Default);

	InitRootSignature(ConvolutionIrradiace);
	InitRootSignature(ConvolutionPrefilter);
	InitRootSignature(GenerateMips);
	InitRootSignature(EquirectangularToCubemap);

	InitRootSignature(Skybox);
	InitRootSignature(GBufferMeshes);
	InitRootSignature(GBufferLights);

	InitRootSignature(SVGF_Reproject);
	InitRootSignature(SVGF_FilterMoments);
	InitRootSignature(SVGF_Atrous);

	InitRootSignature(ShadingComposition);

	InitRootSignature(Accumulation);
	InitRootSignature(PostProcess_Tonemapping);
	InitRootSignature(PostProcess_BloomMask);
	InitRootSignature(PostProcess_BloomDownsample);
	InitRootSignature(PostProcess_BloomBlur);
	InitRootSignature(PostProcess_BloomUpsampleBlurAccumulation);
	InitRootSignature(PostProcess_BloomComposition);

	InitRootSignature(Raytracing::Local);
	InitRootSignature(Raytracing::EmptyLocal);
	InitRootSignature(Raytracing::Global);

	pRenderDevice->CreateRootSignature(Default, [](RootSignatureProxy& proxy)
	{
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// Cubemap convolutions RS
	for (int i = 0; i < CubemapConvolution::NumCubemapConvolutions; ++i)
	{
		RenderResourceHandle RootSignatureHandle;
		switch (i)
		{
		case Irradiance: RootSignatureHandle = ConvolutionIrradiace; break;
		case Prefilter:  RootSignatureHandle = ConvolutionPrefilter; break;
		}

		pRenderDevice->CreateRootSignature(RootSignatureHandle, [i](RootSignatureProxy& proxy)
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
	}

	// Generate mips RS
	pRenderDevice->CreateRootSignature(GenerateMips, [](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<GenerateMipsData>(0, 0));

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);

		proxy.DenyVSAccess();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// Equirectangular to cubemap RS
	pRenderDevice->CreateRootSignature(EquirectangularToCubemap, [](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<EquirectangularToCubemapData>(0, 0));

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);

		proxy.DenyVSAccess();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	pRenderDevice->CreateRootSignature(Skybox, [](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<void>(1, 0, 1));				// RootConstants			b1 | space0
		proxy.AddRootSRVParameter(RootSRV(0, 0));									// Vertex Buffer			t1 | space0
		proxy.AddRootSRVParameter(RootSRV(1, 0));									// Index Buffer				t2 | space0
		proxy.AddRootSRVParameter(RootSRV(2, 0));									// Geometry info Buffer		t3 | space0

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// Local RS
	pRenderDevice->CreateRootSignature(Raytracing::Local, [](RootSignatureProxy& proxy)
	{
		proxy.AddRootSRVParameter(RootSRV(0, 1));	// VertexBuffer,			t0 | space1
		proxy.AddRootSRVParameter(RootSRV(1, 1));	// IndexBuffer				t1 | space1

		proxy.SetAsLocalRootSignature();
	},
		false);

	// Empty Local RS
	pRenderDevice->CreateRootSignature(Raytracing::EmptyLocal, [](RootSignatureProxy& proxy)
	{
		proxy.SetAsLocalRootSignature();
	},
		false);

	// Global RS
	pRenderDevice->CreateRootSignature(Raytracing::Global, [](RootSignatureProxy& proxy)
	{
		proxy.AddRootSRVParameter(RootSRV(0, 0));	// BVH,						t0 | space0
		proxy.AddRootSRVParameter(RootSRV(1, 0));	// Meshes,					t1 | space0
		proxy.AddRootSRVParameter(RootSRV(2, 0));	// Lights,					t2 | space0
		proxy.AddRootSRVParameter(RootSRV(3, 0));	// Materials				t3 | space0

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);	// SamplerLinearWrap	s0 | space0;
		proxy.AddStaticSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);	// SamplerLinearClamp	s1 | space0;
	});
}

void GraphicsPSOs::Register(RenderDevice* pRenderDevice)
{
	InitGraphicsPSO(ConvolutionIrradiace);
	InitGraphicsPSO(ConvolutionPrefilter);

	InitGraphicsPSO(GBufferMeshes);
	InitGraphicsPSO(GBufferLights);

	InitGraphicsPSO(PostProcess_Tonemapping);

	for (int i = 0; i < CubemapConvolution::NumCubemapConvolutions; ++i)
	{
		const RootSignature* pRS = i == CubemapConvolution::Irradiance ? pRenderDevice->GetRootSignature(RootSignatures::ConvolutionIrradiace)
			: pRenderDevice->GetRootSignature(RootSignatures::ConvolutionPrefilter);
		const Shader* pVS = &Shaders::VS::Skybox;
		const Shader* pPS = i == CubemapConvolution::Irradiance ? &Shaders::PS::ConvolutionIrradiance : &Shaders::PS::ConvolutionPrefilter;
		DXGI_FORMAT rtvFormat = i == CubemapConvolution::Irradiance ? RendererFormats::IrradianceFormat : RendererFormats::PrefilterFormat;

		RenderResourceHandle RootSignatureHandle;
		switch (i)
		{
		case Irradiance: RootSignatureHandle = ConvolutionIrradiace; break;
		case Prefilter:  RootSignatureHandle = ConvolutionPrefilter; break;
		}

		pRenderDevice->CreateGraphicsPipelineState(RootSignatureHandle, [=](GraphicsPipelineStateProxy& proxy)
		{
			proxy.pRootSignature = pRS;
			proxy.pVS = pVS;
			proxy.pPS = pPS;

			proxy.RasterizerState.SetCullMode(RasterizerState::CullMode::None);

			proxy.PrimitiveTopology = PrimitiveTopology::Triangle;
			proxy.AddRenderTargetFormat(rtvFormat);
		});
	}
}

void ComputePSOs::Register(RenderDevice* pRenderDevice)
{
	InitComputePSO(GenerateMips);
	InitComputePSO(EquirectangularToCubemap);

	InitComputePSO(SVGF_Reproject);
	InitComputePSO(SVGF_FilterMoments);
	InitComputePSO(SVGF_Atrous);

	InitComputePSO(ShadingComposition);

	InitComputePSO(Accumulation);

	InitComputePSO(PostProcess_BloomMask);
	InitComputePSO(PostProcess_BloomDownsample);
	InitComputePSO(PostProcess_BloomBlur);
	InitComputePSO(PostProcess_BloomUpsampleBlurAccumulation);
	InitComputePSO(PostProcess_BloomComposition);

	pRenderDevice->CreateComputePipelineState(GenerateMips, [=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::GenerateMips);
		proxy.pCS = &Shaders::CS::GenerateMips;
	});

	pRenderDevice->CreateComputePipelineState(EquirectangularToCubemap, [=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::EquirectangularToCubemap);
		proxy.pCS = &Shaders::CS::EquirectangularToCubemap;
	});
}

void RaytracingPSOs::Register(RenderDevice* pRenderDevice)
{
	InitRaytracingPSO(Pathtracing);
	InitRaytracingPSO(AmbientOcclusion);
	InitRaytracingPSO(Shading);
}