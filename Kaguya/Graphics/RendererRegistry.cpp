#include "pch.h"
#include "RendererRegistry.h"

void Shaders::Register(RenderDevice* pRenderDevice)
{
	// Load VS
	VS::Default = pRenderDevice->CompileShader(Shader::Type::Vertex, L"../../Shaders/VS_Default.hlsl", L"main", { { L"RENDER_SHADOWS", L"0" } });
	VS::Quad = pRenderDevice->CompileShader(Shader::Type::Vertex, L"../../Shaders/VS_Quad.hlsl", L"main", {});
	VS::Shadow = pRenderDevice->CompileShader(Shader::Type::Vertex, L"../../Shaders/VS_Default.hlsl", L"main", { { L"RENDER_SHADOWS", L"1" } });
	VS::Skybox = pRenderDevice->CompileShader(Shader::Type::Vertex, L"../../Shaders/VS_Sky.hlsl", L"main", {});
	// Load PS
	PS::BRDFIntegration = pRenderDevice->CompileShader(Shader::Type::Pixel, L"../../Shaders/PS_BRDFIntegration.hlsl", L"main", {});
	PS::ConvolutionIrradiance = pRenderDevice->CompileShader(Shader::Type::Pixel, L"../../Shaders/PS_ConvolutionIrradiance.hlsl", L"main", {});
	PS::ConvolutionPrefilter = pRenderDevice->CompileShader(Shader::Type::Pixel, L"../../Shaders/PS_ConvolutionPrefilter.hlsl", L"main", {});
	PS::PBR = pRenderDevice->CompileShader(Shader::Type::Pixel, L"../../Shaders/PS_PBR.hlsl", L"main", {});
	PS::Skybox = pRenderDevice->CompileShader(Shader::Type::Pixel, L"../../Shaders/PS_Sky.hlsl", L"main", {});

	PS::PostProcess_Tonemapping = pRenderDevice->CompileShader(Shader::Type::Pixel, L"../../Shaders/PostProcess/Tonemapping.hlsl", L"main", {});
	// Load CS
	CS::EquirectangularToCubemap = pRenderDevice->CompileShader(Shader::Type::Compute, L"../../Shaders/CS_EquirectangularToCubemap.hlsl", L"main", {});
	CS::GenerateMips = pRenderDevice->CompileShader(Shader::Type::Compute, L"../../Shaders/CS_GenerateMips.hlsl", L"main", {});
}

void Libraries::Register(RenderDevice* pRenderDevice)
{
	RayGeneration = pRenderDevice->CompileLibrary(L"../../Shaders/Raytracing/RayGeneration.hlsl");
	ClosestHit = pRenderDevice->CompileLibrary(L"../../Shaders/Raytracing/Hit.hlsl");
	Miss = pRenderDevice->CompileLibrary(L"../../Shaders/Raytracing/Miss.hlsl");
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
	for (int i = 0; i < CubemapConvolution::CubemapConvolution_Count; ++i)
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

	// Shadow RS
	StandardShaderLayoutOptions options = {};
	RootSignatures::Shadow = pRenderDevice->CreateRootSignature(&options, [](RootSignatureProxy& proxy)
	{
		proxy.AllowInputLayout();
	});

	// PBR RS
	RootSignatures::PBR = pRenderDevice->CreateRootSignature(&options, [](RootSignatureProxy& proxy)
	{
		proxy.AddRootSRVParameter(0, 0, {}, D3D12_SHADER_VISIBILITY_PIXEL);
		proxy.AddRootSRVParameter(0, 1, {}, D3D12_SHADER_VISIBILITY_PIXEL);

		proxy.AllowInputLayout();
	});

	// Skybox RS
	RootSignatures::Skybox = pRenderDevice->CreateRootSignature(&options, [](RootSignatureProxy& proxy)
	{
		proxy.AllowInputLayout();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// Tonemapping RS
	options.InitConstantDataTypeAsRootConstants = TRUE;
	options.Num32BitValues = sizeof(TonemapData) / 4;
	RootSignatures::PostProcess_Tonemapping = pRenderDevice->CreateRootSignature(&options, [](RootSignatureProxy& proxy)
	{
		proxy.AllowInputLayout();
		proxy.DenyTessellationShaderAccess();
		proxy.DenyGSAccess();
	});

	// Raytracing RSs
	// Ray Generation RS
	RootSignatures::Raytracing::RayGeneration = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
	{
		proxy.AddRootSRVParameter(0, 0); // RaytracingAccelerationStructure
		proxy.AddRootCBVParameter(0, 0); // Camera

		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		CD3DX12_DESCRIPTOR_RANGE1 uavRange = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0, volatileFlag);
		proxy.AddRootDescriptorTableParameter({ uavRange });

		proxy.SetAsLocalRootSignature();
	});

	// Closest Hit RS
	RootSignatures::Raytracing::Hit = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
	{
		proxy.SetAsLocalRootSignature();
	});

	// Miss RS
	RootSignatures::Raytracing::Miss = pRenderDevice->CreateRootSignature(nullptr, [](RootSignatureProxy& proxy)
	{
		proxy.SetAsLocalRootSignature();
	});
}

void GraphicsPSOs::Register(RenderDevice* pRenderDevice)
{
	GraphicsPipelineStateProxy inputLayoutProxy;
	inputLayoutProxy.InputLayout.AddVertexLayoutElement("POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, -1);
	inputLayoutProxy.InputLayout.AddVertexLayoutElement("TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, -1);
	inputLayoutProxy.InputLayout.AddVertexLayoutElement("NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, -1);
	inputLayoutProxy.InputLayout.AddVertexLayoutElement("TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, -1);

	// BRDF Integration PSO
	GraphicsPSOs::BRDFIntegration = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::BRDFIntegration);
		proxy.pVS = pRenderDevice->GetShader(Shaders::VS::Quad);
		proxy.pPS = pRenderDevice->GetShader(Shaders::PS::BRDFIntegration);
		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;
		proxy.AddRenderTargetFormat(RendererFormats::BRDFLUTFormat);
	});

	// Cubemap convolutions PSO
	for (int i = 0; i < CubemapConvolution::CubemapConvolution_Count; ++i)
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
		case Irradiance: GraphicsPSOs::ConvolutionIrradiace = handle; break;
		case Prefilter: GraphicsPSOs::ConvolutionPrefilter = handle; break;
		}
	}

	// Shadow PSO
	GraphicsPSOs::Shadow = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
	{
		proxy = inputLayoutProxy;

		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Shadow);
		proxy.pVS = pRenderDevice->GetShader(Shaders::VS::Shadow);

		proxy.RasterizerState.SetDepthClipEnable(false);
		proxy.RasterizerState.SetDepthBias(100000);
		proxy.RasterizerState.SetDepthBiasClamp(0.0f);
		proxy.RasterizerState.SetSlopeScaledDepthBias(1.0f);

		proxy.DepthStencilState.SetDepthFunc(ComparisonFunc::LessEqual);

		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;
		proxy.AddRenderTargetFormat(RendererFormats::BRDFLUTFormat);
		proxy.SetDepthStencilFormat(DXGI_FORMAT_D32_FLOAT);
	});

	// PBR PSO
	GraphicsPSOs::PBR = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
	{
		proxy = inputLayoutProxy;

		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PBR);
		proxy.pVS = pRenderDevice->GetShader(Shaders::VS::Default);
		proxy.pPS = pRenderDevice->GetShader(Shaders::PS::PBR);

		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;
		proxy.AddRenderTargetFormat(RendererFormats::HDRBufferFormat);
		proxy.SetDepthStencilFormat(RendererFormats::DepthStencilFormat);
	});

	// Skybox PSO
	GraphicsPSOs::Skybox = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
	{
		proxy = inputLayoutProxy;

		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Skybox);
		proxy.pVS = pRenderDevice->GetShader(Shaders::VS::Skybox);
		proxy.pPS = pRenderDevice->GetShader(Shaders::PS::Skybox);

		proxy.RasterizerState.SetCullMode(RasterizerState::CullMode::None);

		proxy.DepthStencilState.SetDepthFunc(ComparisonFunc::LessEqual);

		proxy.PrimitiveTopology = PrimitiveTopology::Triangle;
		proxy.AddRenderTargetFormat(RendererFormats::HDRBufferFormat);
		proxy.SetDepthStencilFormat(RendererFormats::DepthStencilFormat);
	});

	// Tonemapping PSO
	GraphicsPSOs::PostProcess_Tonemapping = pRenderDevice->CreateGraphicsPipelineState([=](GraphicsPipelineStateProxy& proxy)
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
	// Generate mips PSO
	ComputePSOs::GenerateMips = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::GenerateMips);
		proxy.pCS = pRenderDevice->GetShader(Shaders::CS::GenerateMips);
	});

	// Equirectangular to cubemap PSO
	ComputePSOs::EquirectangularToCubemap = pRenderDevice->CreateComputePipelineState([=](ComputePipelineStateProxy& proxy)
	{
		proxy.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::EquirectangularToCubemap);
		proxy.pCS = pRenderDevice->GetShader(Shaders::CS::EquirectangularToCubemap);
	});
}

void RaytracingPSOs::Register(RenderDevice* pRenderDevice)
{
	Raytracing = pRenderDevice->CreateRaytracingPipelineState([&](RaytracingPipelineStateProxy& proxy)
	{
		const Library* pRayGenerationLibrary = pRenderDevice->GetLibrary(Libraries::RayGeneration);
		const Library* pClosestHitLibrary = pRenderDevice->GetLibrary(Libraries::ClosestHit);
		const Library* pMissLibrary = pRenderDevice->GetLibrary(Libraries::Miss);

		proxy.AddLibrary(pRayGenerationLibrary, { L"RayGen" });
		proxy.AddLibrary(pClosestHitLibrary, { L"ClosestHit" });
		proxy.AddLibrary(pMissLibrary, { L"Miss" });

		proxy.AddHitGroup(L"HitGroup", nullptr, L"ClosestHit", nullptr);

		RootSignature* pRayGenerationRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::RayGeneration);
		RootSignature* pHitRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Hit);
		RootSignature* pMissRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Raytracing::Miss);

		// The following section associates the root signature to each shader. Note
		// that we can explicitly show that some shaders share the same root signature
		// (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
		// to as hit groups, meaning that the underlying intersection, any-hit and
		// closest-hit shaders share the same root signature.
		proxy.AddRootSignatureAssociation(pRayGenerationRootSignature, { L"RayGen" });
		proxy.AddRootSignatureAssociation(pHitRootSignature, { L"HitGroup" });
		proxy.AddRootSignatureAssociation(pMissRootSignature, { L"Miss" });

		proxy.SetRaytracingShaderConfig(4 * sizeof(float), 2 * sizeof(float));
		proxy.SetRaytracingPipelineConfig(1);
	});
}