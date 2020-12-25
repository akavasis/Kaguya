#include "pch.h"
#include "RendererRegistry.h"
#include <Core/Application.h>

#define InitRootSignature(Handle) Handle = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::RootSignature, "Root Signature["#Handle"]")
#define InitGraphicsPSO(Handle) Handle = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::PipelineState, "Graphics PSO["#Handle"]")
#define InitComputePSO(Handle) Handle = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::PipelineState, "Compute PSO["#Handle"]")
#define InitRaytracingPSO(Handle) Handle = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::RaytracingPipelineState, "Raytracing PSO["#Handle"]")

const LPCWSTR VSEntryPoint = L"VSMain";
const LPCWSTR MSEntryPoint = L"MSMain";
const LPCWSTR PSEntryPoint = L"PSMain";
const LPCWSTR CSEntryPoint = L"CSMain";

void Shaders::Register(const ShaderCompiler& ShaderCompiler)
{
	const auto& ExecutableFolderPath = Application::ExecutableFolderPath;

	// Load VS
	{
		VS::Quad										= ShaderCompiler.CompileShader(Shader::Type::Vertex, ExecutableFolderPath / L"Shaders/Quad.hlsl",		VSEntryPoint, {});
		VS::Skybox										= ShaderCompiler.CompileShader(Shader::Type::Vertex, ExecutableFolderPath / L"Shaders/Skybox.hlsli",		VSEntryPoint, {});
	}

	// Load MS
	{
		MS::GBufferMeshes								= ShaderCompiler.CompileShader(Shader::Type::Mesh, ExecutableFolderPath / L"Shaders/GBufferMeshes.hlsl", MSEntryPoint, {});
		MS::GBufferLights								= ShaderCompiler.CompileShader(Shader::Type::Mesh, ExecutableFolderPath / L"Shaders/GBufferLights.hlsl", MSEntryPoint, {});
	}

	// Load PS
	{
		PS::ConvolutionIrradiance						= ShaderCompiler.CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/ConvolutionIrradiance.hlsl",	PSEntryPoint, {});
		PS::ConvolutionPrefilter						= ShaderCompiler.CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/ConvolutionPrefilter.hlsl",		PSEntryPoint, {});

		PS::GBufferMeshes								= ShaderCompiler.CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/GBufferMeshes.hlsl",			PSEntryPoint, {});
		PS::GBufferLights								= ShaderCompiler.CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/GBufferLights.hlsl",			PSEntryPoint, {});

		PS::PostProcess_Tonemapping						= ShaderCompiler.CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/PostProcess/Tonemapping.hlsl",	PSEntryPoint, {});
	}

	// Load CS
	{
		CS::EquirectangularToCubemap					= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/EquirectangularToCubemap.hlsl",	CSEntryPoint, {});
		CS::GenerateMips								= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/GenerateMips.hlsl",				CSEntryPoint, {});

		CS::SVGF_Reproject								= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/SVGF/SVGF_Reproject.hlsl",		CSEntryPoint, {});
		CS::SVGF_FilterMoments							= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/SVGF/SVGF_FilterMoments.hlsl",	CSEntryPoint, {});
		CS::SVGF_Atrous									= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/SVGF/SVGF_Atrous.hlsl",			CSEntryPoint, {});

		CS::ShadingComposition							= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/ShadingComposition.hlsl",			CSEntryPoint, {});

		CS::Accumulation								= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/Accumulation.hlsl",				CSEntryPoint, {});

		CS::PostProcess_BloomMask						= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomMask.hlsl",						CSEntryPoint, {});
		CS::PostProcess_BloomDownsample					= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomDownsample.hlsl",				CSEntryPoint, {});
		CS::PostProcess_BloomBlur						= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomBlur.hlsl",						CSEntryPoint, {});
		CS::PostProcess_BloomUpsampleBlurAccumulation	= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomUpsampleBlurAccumulation.hlsl",	CSEntryPoint, {});
		CS::PostProcess_BloomComposition				= ShaderCompiler.CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomComposition.hlsl",				CSEntryPoint, {});
	}
}

void Libraries::Register(const ShaderCompiler& ShaderCompiler)
{
	const auto& ExecutableFolderPath = Application::ExecutableFolderPath;

	Pathtracing			= ShaderCompiler.CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/Pathtracing.hlsl");
	//AmbientOcclusion	= ShaderCompiler.CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/AmbientOcclusion.hlsl");
	Shading				= ShaderCompiler.CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/Shading.hlsl");
	Picking				= ShaderCompiler.CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/Picking.hlsl");
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

	InitRootSignature(PostProcess_Tonemapping);
	InitRootSignature(PostProcess_BloomMask);
	InitRootSignature(PostProcess_BloomDownsample);
	InitRootSignature(PostProcess_BloomBlur);
	InitRootSignature(PostProcess_BloomUpsampleBlurAccumulation);
	InitRootSignature(PostProcess_BloomComposition);

	InitRootSignature(Raytracing::Local);
	InitRootSignature(Raytracing::EmptyLocal);
	InitRootSignature(Raytracing::Global);

	InitRootSignature(Raytracing::Local::Picking);

	pRenderDevice->CreateRootSignature(Default, [](RootSignatureBuilder& Builder)
	{
		Builder.DenyTessellationShaderAccess();
		Builder.DenyGSAccess();
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

		pRenderDevice->CreateRootSignature(RootSignatureHandle, [i](RootSignatureBuilder& Builder)
		{
			UINT Num32BitValues = i == Irradiance ? sizeof(ConvolutionIrradianceSettings) / 4 : sizeof(ConvolutionPrefilterSettings) / 4;

			Builder.AddRootConstantsParameter(RootConstants<void>(0, 0, Num32BitValues));	// Settings					b0 | space0
			Builder.AddRootConstantsParameter(RootConstants<void>(1, 0, 1));				// RootConstants			b1 | space0
			Builder.AddRootSRVParameter(RootSRV(0, 0));										// Vertex Buffer			t1 | space0
			Builder.AddRootSRVParameter(RootSRV(1, 0));										// Index Buffer				t2 | space0
			Builder.AddRootSRVParameter(RootSRV(2, 0));										// Geometry info Buffer		t3 | space0

			Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);

			Builder.DenyTessellationShaderAccess();
			Builder.DenyGSAccess();
		});
	}

	// Generate mips RS
	pRenderDevice->CreateRootSignature(GenerateMips, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter(RootConstants<GenerateMipsData>(0, 0));

		Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);

		Builder.DenyVSAccess();
		Builder.DenyTessellationShaderAccess();
		Builder.DenyGSAccess();
	});

	// Equirectangular to cubemap RS
	pRenderDevice->CreateRootSignature(EquirectangularToCubemap, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter(RootConstants<EquirectangularToCubemapData>(0, 0));

		Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);

		Builder.DenyVSAccess();
		Builder.DenyTessellationShaderAccess();
		Builder.DenyGSAccess();
	});

	pRenderDevice->CreateRootSignature(Skybox, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootConstantsParameter(RootConstants<void>(1, 0, 1));			// RootConstants			b1 | space0
		Builder.AddRootSRVParameter(RootSRV(0, 0));									// Vertex Buffer			t1 | space0
		Builder.AddRootSRVParameter(RootSRV(1, 0));									// Index Buffer				t2 | space0
		Builder.AddRootSRVParameter(RootSRV(2, 0));									// Geometry info Buffer		t3 | space0

		Builder.DenyTessellationShaderAccess();
		Builder.DenyGSAccess();
	});

	// Local Picking RS
	pRenderDevice->CreateRootSignature(Raytracing::Local::Picking, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootUAVParameter(RootUAV(0, 0));	// PickingResult,			u0 | space0

		Builder.SetAsLocalRootSignature();
	},
		false);

	// Local RS
	pRenderDevice->CreateRootSignature(Raytracing::Local, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootSRVParameter(RootSRV(0, 1));	// VertexBuffer,			t0 | space1
		Builder.AddRootSRVParameter(RootSRV(1, 1));	// IndexBuffer				t1 | space1

		Builder.SetAsLocalRootSignature();
	},
		false);

	// Empty Local RS
	pRenderDevice->CreateRootSignature(Raytracing::EmptyLocal, [](RootSignatureBuilder& Builder)
	{
		Builder.SetAsLocalRootSignature();
	},
		false);

	// Global RS
	pRenderDevice->CreateRootSignature(Raytracing::Global, [](RootSignatureBuilder& Builder)
	{
		Builder.AddRootSRVParameter(RootSRV(0, 0));	// BVH,						t0 | space0
		Builder.AddRootSRVParameter(RootSRV(1, 0));	// Meshes,					t1 | space0
		Builder.AddRootSRVParameter(RootSRV(2, 0));	// Lights,					t2 | space0
		Builder.AddRootSRVParameter(RootSRV(3, 0));	// Materials				t3 | space0

		Builder.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);	// SamplerLinearWrap	s0 | space0;
		Builder.AddStaticSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);	// SamplerLinearClamp	s1 | space0;
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

		pRenderDevice->CreateGraphicsPipelineState(RootSignatureHandle, [=](GraphicsPipelineStateBuilder& Builder)
		{
			Builder.pRootSignature = pRS;
			Builder.pVS = pVS;
			Builder.pPS = pPS;

			Builder.RasterizerState.SetCullMode(RasterizerState::CullMode::None);

			Builder.PrimitiveTopology = PrimitiveTopology::Triangle;
			Builder.AddRenderTargetFormat(rtvFormat);
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

	pRenderDevice->CreateComputePipelineState(GenerateMips, [=](ComputePipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::GenerateMips);
		Builder.pCS = &Shaders::CS::GenerateMips;
	});

	pRenderDevice->CreateComputePipelineState(EquirectangularToCubemap, [=](ComputePipelineStateBuilder& Builder)
	{
		Builder.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::EquirectangularToCubemap);
		Builder.pCS = &Shaders::CS::EquirectangularToCubemap;
	});
}

void RaytracingPSOs::Register(RenderDevice* pRenderDevice)
{
	InitRaytracingPSO(Pathtracing);
	InitRaytracingPSO(AmbientOcclusion);
	InitRaytracingPSO(Shading);
	InitRaytracingPSO(Picking);
}