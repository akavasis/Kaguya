#include "pch.h"
#include "RendererRegistry.h"

#include "RenderPass/Pathtracing.h"
#include "RenderPass/Accumulation.h"
#include "RenderPass/PostProcess.h"

void Shaders::Register(RenderDevice* pRenderDevice, std::filesystem::path ExecutableFolderPath)
{
	// Load VS
	{
		VS::Quad = pRenderDevice->CompileShader(Shader::Type::Vertex, ExecutableFolderPath / L"Shaders/VS_Quad.hlsl", L"main", {});
		VS::Skybox = pRenderDevice->CompileShader(Shader::Type::Vertex, ExecutableFolderPath / L"Shaders/VS_Sky.hlsl", L"main", {});
	}

	// Load PS
	{
		PS::BRDFIntegration = pRenderDevice->CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/PS_BRDFIntegration.hlsl", L"main", {});
		PS::ConvolutionIrradiance = pRenderDevice->CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/PS_ConvolutionIrradiance.hlsl", L"main", {});
		PS::ConvolutionPrefilter = pRenderDevice->CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/PS_ConvolutionPrefilter.hlsl", L"main", {});

		PS::PostProcess_Tonemapping = pRenderDevice->CompileShader(Shader::Type::Pixel, ExecutableFolderPath / L"Shaders/PostProcess/Tonemapping.hlsl", L"main", {});
	}

	// Load CS
	{
		CS::EquirectangularToCubemap = pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/CS_EquirectangularToCubemap.hlsl", L"main", {});
		CS::GenerateMips = pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/CS_GenerateMips.hlsl", L"main", {});

		CS::Accumulation = pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/Raytracing/Accumulation.hlsl", L"main", {});

		CS::PostProcess_BloomMask = pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomMask.hlsl", L"main", {});
		CS::PostProcess_BloomDownsample = pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomDownsample.hlsl", L"main", {});
		CS::PostProcess_BloomBlur = pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomBlur.hlsl", L"main", {});
		CS::PostProcess_BloomUpsampleBlurAccumulation = pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomUpsampleBlurAccumulation.hlsl", L"main", {});
		CS::PostProcess_BloomComposition = pRenderDevice->CompileShader(Shader::Type::Compute, ExecutableFolderPath / L"Shaders/PostProcess/BloomComposition.hlsl", L"main", {});
	}
}

void Libraries::Register(RenderDevice* pRenderDevice, std::filesystem::path ExecutableFolderPath)
{
	Pathtracing = pRenderDevice->CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/Pathtracing.hlsl");
	RaytraceGBuffer = pRenderDevice->CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/RaytraceGBuffer.hlsl");
}

void RootSignatures::Register(RenderDevice* pRenderDevice)
{
#pragma region NonStandardRootSignatures
	// BRDFIntegration RS, this is just a empty root signature
	RootSignatures::BRDFIntegration = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
	{
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// Cubemap convolutions RS
	for (int i = 0; i < CubemapConvolution::NumCubemapConvolutions; ++i)
	{
		RenderResourceHandle rootSignatureHandle = pRenderDevice->CreateRootSignature(nullptr, [i](RootSignatureProxy& proxy)
		{
			const UINT num32bitValues = i == Irradiance ? sizeof(ConvolutionIrradianceSetting) / 4 : sizeof(ConvolutionPrefilterSetting) / 4;

			D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
			CD3DX12_DESCRIPTOR_RANGE1 srvRange = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, volatileFlag);

			proxy.AddRootConstantsParameter(0, 0, num32bitValues, D3D12_SHADER_VISIBILITY_PIXEL);
			proxy.AddRootCBVParameter(1, 100);
			proxy.AddRootDescriptorTableParameter({ srvRange }, D3D12_SHADER_VISIBILITY_PIXEL);

			proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);

			proxy.AllowInputLayout();
			proxy.DenyTessellationShaderAccess();
			proxy.DenyGSAccess();
		});

		switch (i)
		{
		case Irradiance: RootSignatures::ConvolutionIrradiace = rootSignatureHandle; break;
		case Prefilter: RootSignatures::ConvolutionPrefilter = rootSignatureHandle; break;
		}
	}

	// Generate mips RS
	RootSignatures::GenerateMips = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
	{
		constexpr UINT num32bitValues = sizeof(GenerateMipsData) / 4;

		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		CD3DX12_DESCRIPTOR_RANGE1 srcMip = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 outMip = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0, 0, volatileFlag);

		proxy.AddRootConstantsParameter(0, 0, num32bitValues);
		proxy.AddRootDescriptorTableParameter({ srcMip });
		proxy.AddRootDescriptorTableParameter({ outMip });

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);

		proxy.DenyVSAccess();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// Equirectangular to cubemap RS
	RootSignatures::EquirectangularToCubemap = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
	{
		constexpr UINT num32bitValues = sizeof(GenerateMipsData) / 4;

		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		CD3DX12_DESCRIPTOR_RANGE1 srcMip = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 outMip = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 0, 0, volatileFlag);

		proxy.AddRootConstantsParameter(0, 0, num32bitValues);
		proxy.AddRootDescriptorTableParameter({ srcMip });
		proxy.AddRootDescriptorTableParameter({ outMip });

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);

		proxy.DenyVSAccess();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});
#pragma endregion

	StandardShaderLayoutOptions options = {};

	// Skybox
	Skybox = pRenderDevice->CreateRootSignature(&options, [](RootSignatureProxy& proxy)
	{
		proxy.AllowInputLayout();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// PostProcess_BloomMask
	PostProcess_BloomMask = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
	{
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		CD3DX12_DESCRIPTOR_RANGE1 input = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 output = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, volatileFlag);

		proxy.AddRootConstantsParameter(0, 0, 3);
		proxy.AddRootDescriptorTableParameter({ input });
		proxy.AddRootDescriptorTableParameter({ output });

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0);

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// PostProcess_BloomDownsample
	PostProcess_BloomDownsample = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
	{
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		CD3DX12_DESCRIPTOR_RANGE1 bloom = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 output1 = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 output2 = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 output3 = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 2, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 output4 = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 3, 0, volatileFlag);

		proxy.AddRootConstantsParameter(0, 0, 2);
		proxy.AddRootDescriptorTableParameter({ bloom });
		proxy.AddRootDescriptorTableParameter({ output1 });
		proxy.AddRootDescriptorTableParameter({ output2 });
		proxy.AddRootDescriptorTableParameter({ output3 });
		proxy.AddRootDescriptorTableParameter({ output4 });

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0);

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// PostProcess_Blur
	PostProcess_BloomBlur = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
	{
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		CD3DX12_DESCRIPTOR_RANGE1 input = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 output = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, volatileFlag);

		proxy.AddRootConstantsParameter(0, 0, 2);
		proxy.AddRootDescriptorTableParameter({ input });
		proxy.AddRootDescriptorTableParameter({ output });

		proxy.AllowInputLayout();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// PostProcess_BloomUpsampleBlurAccumulation
	PostProcess_BloomUpsampleBlurAccumulation = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
	{
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		CD3DX12_DESCRIPTOR_RANGE1 highResolution = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 lowResolution = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 output = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, volatileFlag);

		proxy.AddRootConstantsParameter(0, 0, 3);
		proxy.AddRootDescriptorTableParameter({ highResolution });
		proxy.AddRootDescriptorTableParameter({ lowResolution });
		proxy.AddRootDescriptorTableParameter({ output });

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_BORDER, 0, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// PostProcess_BloomComposition
	PostProcess_BloomComposition = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
	{
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		CD3DX12_DESCRIPTOR_RANGE1 input = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 bloom = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 output = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, volatileFlag);

		proxy.AddRootConstantsParameter(0, 0, 3);
		proxy.AddRootDescriptorTableParameter({ input });
		proxy.AddRootDescriptorTableParameter({ bloom });
		proxy.AddRootDescriptorTableParameter({ output });

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0);

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// PostProcess_Tonemapping
	RootSignatures::PostProcess_Tonemapping = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
	{
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		CD3DX12_DESCRIPTOR_RANGE1 input = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, volatileFlag);

		proxy.AddRootConstantsParameter(0, 0, 2);
		proxy.AddRootDescriptorTableParameter({ input });

		proxy.AllowInputLayout();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// Raytracing RSs
	{
		// Accumulation
		RootSignatures::Raytracing::Accumulation = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
		{
			D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
			CD3DX12_DESCRIPTOR_RANGE1 input = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, volatileFlag);
			CD3DX12_DESCRIPTOR_RANGE1 output = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1, 0, volatileFlag);

			proxy.AddRootConstantsParameter<Accumulation::SSettings>(0, 0);
			proxy.AddRootDescriptorTableParameter({ input });
			proxy.AddRootDescriptorTableParameter({ output });

			proxy.DenyTessellationShaderAccess();
			proxy.DenyGSAccess();
		});

		// Path
		{
			// Global RS
			options.InitConstantDataTypeAsRootConstants = FALSE;
			options.Num32BitValues = 0;
			RootSignatures::Raytracing::Pathtracing::Global = pRenderDevice->CreateRootSignature(&options, [](RootSignatureProxy& proxy)
			{
				CD3DX12_DESCRIPTOR_RANGE1 srvRange = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0, 0);
				CD3DX12_DESCRIPTOR_RANGE1 uavRange = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);

				proxy.AddRootDescriptorTableParameter({ srvRange }); // GeometryTable
				proxy.AddRootDescriptorTableParameter({ uavRange }); // Render Target
			});

			// Empty Local RS
			RootSignatures::Raytracing::Pathtracing::EmptyLocal = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
			{
				proxy.SetAsLocalRootSignature();
			});

			RootSignatures::Raytracing::Pathtracing::HitGroup = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
			{
				proxy.AddRootConstantsParameter(0, 0, 1);

				proxy.SetAsLocalRootSignature();
			});
		}
	}
}

void GraphicsPSOs::Register(RenderDevice* pRenderDevice)
{
	GraphicsPipelineStateProxy inputLayoutProxy;
	inputLayoutProxy.InputLayout.AddVertexLayoutElement("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, -1);
	inputLayoutProxy.InputLayout.AddVertexLayoutElement("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, -1);
	inputLayoutProxy.InputLayout.AddVertexLayoutElement("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, -1);
	inputLayoutProxy.InputLayout.AddVertexLayoutElement("TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, -1);
	inputLayoutProxy.InputLayout.AddVertexLayoutElement("BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, -1);

	// BRDFIntegration
	BRDFIntegration = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::BRDFIntegration);
		proxy.pVS = pRenderDevice->GetShader(Shaders::VS::Quad);
		proxy.pPS = pRenderDevice->GetShader(Shaders::PS::BRDFIntegration);
		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;
		proxy.AddRenderTargetFormat(RendererFormats::BRDFLUTFormat);
	});

	// Cubemap convolutions PSO
	for (int i = 0; i < CubemapConvolution::NumCubemapConvolutions; ++i)
	{
		const RootSignature* pRS = i == CubemapConvolution::Irradiance ? pRenderDevice->GetRootSignature(RootSignatures::ConvolutionIrradiace)
			: pRenderDevice->GetRootSignature(RootSignatures::ConvolutionPrefilter);
		const Shader* pVS = pRenderDevice->GetShader(Shaders::VS::Skybox);
		const Shader* pPS = i == CubemapConvolution::Irradiance ? pRenderDevice->GetShader(Shaders::PS::ConvolutionIrradiance)
			: pRenderDevice->GetShader(Shaders::PS::ConvolutionPrefilter);
		DXGI_FORMAT rtvFormat = i == CubemapConvolution::Irradiance ? RendererFormats::IrradianceFormat : RendererFormats::PrefilterFormat;

		RenderResourceHandle handle = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
		{
			proxy = inputLayoutProxy;

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

	// PostProcess_Tonemapping
	PostProcess_Tonemapping = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_Tonemapping);
		proxy.pVS = pRenderDevice->GetShader(Shaders::VS::Quad);
		proxy.pPS = pRenderDevice->GetShader(Shaders::PS::PostProcess_Tonemapping);

		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;
		proxy.AddRenderTargetFormat(RendererFormats::SwapChainBufferFormat);
	});
}

void ComputePSOs::Register(RenderDevice* pRenderDevice)
{
	GenerateMips = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::GenerateMips);
		proxy.pCS = pRenderDevice->GetShader(Shaders::CS::GenerateMips);
	});

	EquirectangularToCubemap = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::EquirectangularToCubemap);
		proxy.pCS = pRenderDevice->GetShader(Shaders::CS::EquirectangularToCubemap);
	});

	Accumulation = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Accumulation);
		proxy.pCS = pRenderDevice->GetShader(Shaders::CS::Accumulation);
	});

	PostProcess_BloomMask = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomMask);
		proxy.pCS = pRenderDevice->GetShader(Shaders::CS::PostProcess_BloomMask);
	});

	PostProcess_BloomDownsample = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomDownsample);
		proxy.pCS = pRenderDevice->GetShader(Shaders::CS::PostProcess_BloomDownsample);
	});

	PostProcess_BloomBlur = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomBlur);
		proxy.pCS = pRenderDevice->GetShader(Shaders::CS::PostProcess_BloomBlur);
	});

	PostProcess_BloomUpsampleBlurAccumulation = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomUpsampleBlurAccumulation);
		proxy.pCS = pRenderDevice->GetShader(Shaders::CS::PostProcess_BloomUpsampleBlurAccumulation);
	});

	PostProcess_BloomComposition = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomComposition);
		proxy.pCS = pRenderDevice->GetShader(Shaders::CS::PostProcess_BloomComposition);
	});
}

void RaytracingPSOs::Register(RenderDevice* pRenderDevice)
{
#define ENUM_TO_LSTR(Enum) L#Enum

	Pathtracing = pRenderDevice->CreateRaytracingPipelineState([=](RaytracingPipelineStateProxy& proxy)
	{
		enum Symbols
		{
			RayGeneration,
			Miss,
			ShadowMiss,
			ClosestHit,
			ShadowClosestHit,
			NumSymbols
		};

		const LPCWSTR symbols[NumSymbols] =
		{
			ENUM_TO_LSTR(RayGeneration),
			ENUM_TO_LSTR(Miss), ENUM_TO_LSTR(ShadowMiss),
			ENUM_TO_LSTR(ClosestHit), ENUM_TO_LSTR(ShadowClosestHit)
		};

		enum HitGroups
		{
			Default,
			Shadow,
			NumHitGroups
		};

		const LPCWSTR hitGroups[NumHitGroups] =
		{
			ENUM_TO_LSTR(Default),
			ENUM_TO_LSTR(Shadow)
		};

		const Library* pRaytraceLibrary = pRenderDevice->GetLibrary(Libraries::Pathtracing);

		proxy.AddLibrary(pRaytraceLibrary,
			{
				symbols[RayGeneration],
				symbols[Miss], symbols[ShadowMiss],
				symbols[ClosestHit], symbols[ShadowClosestHit]
			});

		proxy.AddHitGroup(hitGroups[Default], nullptr, symbols[ClosestHit], nullptr);
		proxy.AddHitGroup(hitGroups[Shadow], nullptr, symbols[ShadowClosestHit], nullptr);

		RootSignature* pGlobalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Pathtracing::Global);
		RootSignature* pEmptyLocalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Pathtracing::EmptyLocal);
		RootSignature* pHitGroupRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Pathtracing::HitGroup);

		// The following section associates the root signature to each shader. Note
		// that we can explicitly show that some shaders share the same root signature
		// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
		// to as hit groups, meaning that the underlying intersection, any-hit and
		// closest-hit shaders share the same root signature.
		proxy.AddRootSignatureAssociation(pEmptyLocalRootSignature,
			{
				symbols[RayGeneration], symbols[Miss], symbols[ShadowMiss]
			});

		proxy.AddRootSignatureAssociation(pHitGroupRootSignature,
			{
				hitGroups[Default], hitGroups[Shadow]
			});

		proxy.SetGlobalRootSignature(pGlobalRootSignature);

		proxy.SetRaytracingShaderConfig(6 * sizeof(float) + 2 * sizeof(unsigned int), 2 * sizeof(float));
		proxy.SetRaytracingPipelineConfig(8);
	});
}