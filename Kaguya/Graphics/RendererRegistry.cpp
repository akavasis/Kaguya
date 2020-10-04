#include "pch.h"
#include "RendererRegistry.h"

#include "RenderPass/Pathtracing.h"
#include "RenderPass/RaytraceGBuffer.h"
#include "RenderPass/Accumulation.h"
#include "RenderPass/PostProcess.h"

static constexpr size_t SizeOfBuiltInTriangleIntersectionAttributes = 2 * sizeof(float);

#define ENUM_TO_LSTR(Enum) L#Enum

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
	AmbientOcclusion = pRenderDevice->CompileLibrary(ExecutableFolderPath / L"Shaders/Raytracing/AmbientOcclusion.hlsl");
}

void RootSignatures::Register(RenderDevice* pRenderDevice)
{
	// BRDFIntegration RS, this is just a empty root signature
	BRDFIntegration = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// Cubemap convolutions RS
	for (int i = 0; i < CubemapConvolution::NumCubemapConvolutions; ++i)
	{
		RenderResourceHandle rootSignatureHandle = pRenderDevice->CreateRootSignature([i](RootSignatureProxy& proxy)
		{
			D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

			if (i == Irradiance)
				proxy.AddRootConstantsParameter(RootConstants<ConvolutionIrradianceSetting>(0, 0));
			else
				proxy.AddRootConstantsParameter(RootConstants<ConvolutionPrefilterSetting>(0, 0));
			proxy.AddRootCBVParameter(RootCBV(1, 100));

			RootDescriptorTable descriptorTable;
			descriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(1, 0, 0, volatileFlag, D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND));

			proxy.AddRootDescriptorTableParameter(descriptorTable);

			proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);

			proxy.AllowInputLayout();
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
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		proxy.AddRootConstantsParameter(RootConstants<GenerateMipsData>(0, 0));

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 16);

		proxy.DenyVSAccess();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// Equirectangular to cubemap RS
	EquirectangularToCubemap = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		proxy.AddRootConstantsParameter(RootConstants<EquirectangularToCubemapData>(0, 0));

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP, 16);

		proxy.DenyVSAccess();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	Skybox = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.AllowInputLayout();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	PostProcess_BloomMask = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, 5));

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0);

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	PostProcess_BloomDownsample = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, 7));

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0);

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	PostProcess_BloomBlur = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, 4));

		proxy.AllowInputLayout();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	PostProcess_BloomUpsampleBlurAccumulation = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, 6));

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_BORDER, 0, D3D12_COMPARISON_FUNC_LESS_EQUAL, D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	PostProcess_BloomComposition = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;

		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, 6));

		proxy.AddStaticSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, 0);

		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	PostProcess_Tonemapping = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<void>(0, 0, 3));

		proxy.AllowInputLayout();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	Raytracing::Accumulation = pRenderDevice->CreateRootSignature([](RootSignatureProxy& proxy)
	{
		proxy.AddRootConstantsParameter(RootConstants<Accumulation::SSettings>(0, 0));

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
		proxy.AddRootSRVParameter(RootSRV(0, 0)); // BVH,					t0 | s0
		proxy.AddRootSRVParameter(RootSRV(1, 0)); // Vertex Buffer,			t1 | s0
		proxy.AddRootSRVParameter(RootSRV(2, 0)); // Index Buffer,			t2 | s0
		proxy.AddRootSRVParameter(RootSRV(3, 0)); // Geometry info Buffer,	t3 | s0
		proxy.AddRootSRVParameter(RootSRV(4, 0)); // Material Buffer,		t4 | s0
	});
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
		proxy.pVS = &Shaders::VS::Quad;
		proxy.pPS = &Shaders::PS::BRDFIntegration;
		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;
		proxy.AddRenderTargetFormat(RendererFormats::BRDFLUTFormat);
	});

	// Cubemap convolutions PSO
	for (int i = 0; i < CubemapConvolution::NumCubemapConvolutions; ++i)
	{
		const RootSignature* pRS = i == CubemapConvolution::Irradiance ? pRenderDevice->GetRootSignature(RootSignatures::ConvolutionIrradiace)
			: pRenderDevice->GetRootSignature(RootSignatures::ConvolutionPrefilter);
		const Shader* pVS = &Shaders::VS::Skybox;
		const Shader* pPS = i == CubemapConvolution::Irradiance ? &Shaders::PS::ConvolutionIrradiance : &Shaders::PS::ConvolutionPrefilter;
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
		proxy.pVS = &Shaders::VS::Quad;
		proxy.pPS = &Shaders::PS::PostProcess_Tonemapping;

		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;
		proxy.AddRenderTargetFormat(RendererFormats::SwapChainBufferFormat);
	});
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

	Accumulation = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Accumulation);
		proxy.pCS = &Shaders::CS::Accumulation;
	});

	PostProcess_BloomMask = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomMask);
		proxy.pCS = &Shaders::CS::PostProcess_BloomMask;
	});

	PostProcess_BloomDownsample = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomDownsample);
		proxy.pCS = &Shaders::CS::PostProcess_BloomDownsample;
	});

	PostProcess_BloomBlur = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomBlur);
		proxy.pCS = &Shaders::CS::PostProcess_BloomBlur;
	});

	PostProcess_BloomUpsampleBlurAccumulation = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomUpsampleBlurAccumulation);
		proxy.pCS = &Shaders::CS::PostProcess_BloomUpsampleBlurAccumulation;
	});

	PostProcess_BloomComposition = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_BloomComposition);
		proxy.pCS = &Shaders::CS::PostProcess_BloomComposition;
	});
}

void RaytracingPSOs::Register(RenderDevice* pRenderDevice)
{
	Pathtracing = pRenderDevice->CreateRaytracingPipelineState([=](RaytracingPipelineStateProxy& proxy)
	{
		enum Symbols
		{
			RayGeneration,
			Miss,
			ClosestHit,
			NumSymbols
		};

		const LPCWSTR symbols[NumSymbols] =
		{
			ENUM_TO_LSTR(RayGeneration),
			ENUM_TO_LSTR(Miss),
			ENUM_TO_LSTR(ClosestHit)
		};

		enum HitGroups
		{
			Default,
			NumHitGroups
		};

		const LPCWSTR hitGroups[NumHitGroups] =
		{
			ENUM_TO_LSTR(Default)
		};

		const Library* pRaytraceLibrary = &Libraries::Pathtracing;

		proxy.AddLibrary(pRaytraceLibrary,
			{
				symbols[RayGeneration],
				symbols[Miss],
				symbols[ClosestHit]
			});

		proxy.AddHitGroup(hitGroups[Default], nullptr, symbols[ClosestHit], nullptr);

		RootSignature* pGlobalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Global);
		RootSignature* pEmptyLocalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::EmptyLocal);

		// The following section associates the root signature to each shader. Note
		// that we can explicitly show that some shaders share the same root signature
		// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
		// to as hit groups, meaning that the underlying intersection, any-hit and
		// closest-hit shaders share the same root signature.
		proxy.AddRootSignatureAssociation(pEmptyLocalRootSignature,
			{
				symbols[RayGeneration],
				symbols[Miss],
				hitGroups[Default]
			});

		proxy.SetGlobalRootSignature(pGlobalRootSignature);

		proxy.SetRaytracingShaderConfig(6 * sizeof(float) + 2 * sizeof(unsigned int), SizeOfBuiltInTriangleIntersectionAttributes);
		proxy.SetRaytracingPipelineConfig(8);
	});

	RaytraceGBuffer = pRenderDevice->CreateRaytracingPipelineState([&](RaytracingPipelineStateProxy& proxy)
	{
		enum Symbols
		{
			RayGeneration,
			Miss,
			ClosestHit,
			NumSymbols
		};

		const LPCWSTR symbols[NumSymbols] =
		{
			ENUM_TO_LSTR(RayGeneration),
			ENUM_TO_LSTR(Miss),
			ENUM_TO_LSTR(ClosestHit)
		};

		enum HitGroups
		{
			Default,
			NumHitGroups
		};

		const LPCWSTR hitGroups[NumHitGroups] =
		{
			ENUM_TO_LSTR(Default)
		};

		const Library* pRaytraceLibrary = &Libraries::RaytraceGBuffer;

		proxy.AddLibrary(pRaytraceLibrary,
			{
				symbols[RayGeneration],
				symbols[Miss],
				symbols[ClosestHit]
			});

		proxy.AddHitGroup(hitGroups[Default], nullptr, symbols[ClosestHit], nullptr);

		RootSignature* pGlobalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Global);
		RootSignature* pEmptyLocalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::EmptyLocal);

		// The following section associates the root signature to each shader. Note
		// that we can explicitly show that some shaders share the same root signature
		// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
		// to as hit groups, meaning that the underlying intersection, any-hit and
		// closest-hit shaders share the same root signature.
		proxy.AddRootSignatureAssociation(pEmptyLocalRootSignature,
			{
				symbols[RayGeneration],
				symbols[Miss],
				hitGroups[Default]
			});

		proxy.SetGlobalRootSignature(pGlobalRootSignature);

		proxy.SetRaytracingShaderConfig(sizeof(int), SizeOfBuiltInTriangleIntersectionAttributes);
		proxy.SetRaytracingPipelineConfig(2);
	});

	AmbientOcclusion = pRenderDevice->CreateRaytracingPipelineState([&](RaytracingPipelineStateProxy& proxy)
	{
		enum Symbols
		{
			RayGeneration,
			Miss,
			ClosestHit,
			NumSymbols
		};

		const LPCWSTR symbols[NumSymbols] =
		{
			ENUM_TO_LSTR(RayGeneration),
			ENUM_TO_LSTR(Miss),
			ENUM_TO_LSTR(ClosestHit)
		};

		enum HitGroups
		{
			Default,
			NumHitGroups
		};

		const LPCWSTR hitGroups[NumHitGroups] =
		{
			ENUM_TO_LSTR(Default)
		};

		const Library* pRaytraceLibrary = &Libraries::RaytraceGBuffer;

		proxy.AddLibrary(pRaytraceLibrary,
			{
				symbols[RayGeneration],
				symbols[Miss],
				symbols[ClosestHit]
			});

		proxy.AddHitGroup(hitGroups[Default], nullptr, symbols[ClosestHit], nullptr);

		RootSignature* pGlobalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Global);
		RootSignature* pEmptyLocalRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::EmptyLocal);

		// The following section associates the root signature to each shader. Note
		// that we can explicitly show that some shaders share the same root signature
		// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
		// to as hit groups, meaning that the underlying intersection, any-hit and
		// closest-hit shaders share the same root signature.
		proxy.AddRootSignatureAssociation(pEmptyLocalRootSignature,
			{
				symbols[RayGeneration],
				symbols[Miss],
				hitGroups[Default]
			});

		proxy.SetGlobalRootSignature(pGlobalRootSignature);

		proxy.SetRaytracingShaderConfig(sizeof(int), SizeOfBuiltInTriangleIntersectionAttributes);
		proxy.SetRaytracingPipelineConfig(2);
	});
}