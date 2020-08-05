#include "pch.h"
#include "RendererRegistry.h"

void Shaders::Register(RenderDevice* pRenderDevice)
{
	// Load VS
	VS::Default = pRenderDevice->CompileShader(Shader::Type::Vertex, L"../../Shaders/VS_Default.hlsl", L"main", { { L"RENDER_SHADOWS", L"0" } });
	VS::Quad = pRenderDevice->CompileShader(Shader::Type::Vertex, L"../../Shaders/VS_Quad.hlsl", L"main", {});
	VS::Shadow = pRenderDevice->CompileShader(Shader::Type::Vertex, L"../../Shaders/VS_Default.hlsl", L"main", { { L"RENDER_SHADOWS", L"1" } });
	VS::Sky = pRenderDevice->CompileShader(Shader::Type::Vertex, L"../../Shaders/VS_Sky.hlsl", L"main", {});
	// Load PS
	PS::BRDFIntegration = pRenderDevice->CompileShader(Shader::Type::Pixel, L"../../Shaders/PS_BRDFIntegration.hlsl", L"main", {});
	PS::ConvolutionIrradiance = pRenderDevice->CompileShader(Shader::Type::Pixel, L"../../Shaders/PS_ConvolutionIrradiance.hlsl", L"main", {});
	PS::ConvolutionPrefilter = pRenderDevice->CompileShader(Shader::Type::Pixel, L"../../Shaders/PS_ConvolutionPrefilter.hlsl", L"main", {});
	PS::PBR = pRenderDevice->CompileShader(Shader::Type::Pixel, L"../../Shaders/PS_PBR.hlsl", L"main", {});
	PS::Sky = pRenderDevice->CompileShader(Shader::Type::Pixel, L"../../Shaders/PS_Sky.hlsl", L"main", {});

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
	{
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
}

void GraphicsPSOs::Register(RenderDevice* pRenderDevice)
{
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
	{
		{ "POSITION"  , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD"  , 0, DXGI_FORMAT_R32G32_FLOAT   , 0,  D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL"    , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT"   , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// BRDF Integration PSO
	{
		GraphicsPipelineState::Properties prop = {};
		prop.Desc.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::BRDFIntegration)->GetD3DRootSignature();
		prop.Desc.VS = pRenderDevice->GetShader(Shaders::VS::Quad)->GetD3DShaderBytecode();
		prop.Desc.PS = pRenderDevice->GetShader(Shaders::PS::BRDFIntegration)->GetD3DShaderBytecode();
		prop.Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		prop.Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		prop.Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		prop.Desc.SampleMask = UINT_MAX;
		prop.Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		prop.Desc.NumRenderTargets = 1;
		prop.Desc.RTVFormats[0] = RendererFormats::BRDFLUTFormat;
		prop.Desc.SampleDesc = { .Count = 1, .Quality = 0 };

		GraphicsPSOs::BRDFIntegration = pRenderDevice->CreateGraphicsPipelineState(prop);
	}

	// Cubemap convolutions PSO
	{
		GraphicsPipelineState::Properties prop = {};
		for (int i = 0; i < CubemapConvolution::CubemapConvolution_Count; ++i)
		{
			prop.Desc.pRootSignature = i == CubemapConvolution::Irradiance ? pRenderDevice->GetRootSignature(RootSignatures::ConvolutionIrradiace)->GetD3DRootSignature()
				: pRenderDevice->GetRootSignature(RootSignatures::ConvolutionPrefilter)->GetD3DRootSignature();
			prop.Desc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
			prop.Desc.VS = pRenderDevice->GetShader(Shaders::VS::Sky)->GetD3DShaderBytecode();
			prop.Desc.PS = i == CubemapConvolution::Irradiance ? pRenderDevice->GetShader(Shaders::PS::ConvolutionIrradiance)->GetD3DShaderBytecode()
				: pRenderDevice->GetShader(Shaders::PS::ConvolutionPrefilter)->GetD3DShaderBytecode();
			prop.Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
			prop.Desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
			prop.Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
			prop.Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
			prop.Desc.SampleMask = UINT_MAX;
			prop.Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
			prop.Desc.NumRenderTargets = 1;
			prop.Desc.RTVFormats[0] = i == CubemapConvolution::Irradiance ? RendererFormats::IrradianceFormat : RendererFormats::PrefilterFormat;
			prop.Desc.SampleDesc = { .Count = 1, .Quality = 0 };

			switch (i)
			{
			case Irradiance:
				GraphicsPSOs::ConvolutionIrradiace = pRenderDevice->CreateGraphicsPipelineState(prop);
				break;
			case Prefilter:
				GraphicsPSOs::ConvolutionPrefilter = pRenderDevice->CreateGraphicsPipelineState(prop);
				break;
			}
		}
	}

	// Shadow PSO
	{
		GraphicsPipelineState::Properties prop = {};
		prop.Desc.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Shadow)->GetD3DRootSignature();
		prop.Desc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
		prop.Desc.VS = pRenderDevice->GetShader(Shaders::VS::Shadow)->GetD3DShaderBytecode();
		prop.Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		prop.Desc.RasterizerState.DepthBias = 100000;
		prop.Desc.RasterizerState.DepthBiasClamp = 0.0f;
		prop.Desc.RasterizerState.SlopeScaledDepthBias = 1.0f;
		prop.Desc.RasterizerState.DepthClipEnable = FALSE;
		prop.Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		prop.Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		prop.Desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		prop.Desc.SampleMask = UINT_MAX;
		prop.Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		prop.Desc.SampleDesc = { .Count = 1, .Quality = 0 };
		prop.Desc.DSVFormat = DXGI_FORMAT_D32_FLOAT; // Set DSVFormat the same as shadow map's depth format

		GraphicsPSOs::Shadow = pRenderDevice->CreateGraphicsPipelineState(prop);
	}

	// PBR PSO
	{
		GraphicsPipelineState::Properties prop = {};
		prop.Desc.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PBR)->GetD3DRootSignature();
		prop.Desc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
		prop.Desc.VS = pRenderDevice->GetShader(Shaders::VS::Default)->GetD3DShaderBytecode();
		prop.Desc.PS = pRenderDevice->GetShader(Shaders::PS::PBR)->GetD3DShaderBytecode();
		prop.Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		prop.Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		prop.Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		prop.Desc.SampleMask = UINT_MAX;
		prop.Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		prop.Desc.NumRenderTargets = 1;
		prop.Desc.RTVFormats[0] = RendererFormats::HDRBufferFormat;
		prop.Desc.SampleDesc = { .Count = 1, .Quality = 0 };
		prop.Desc.DSVFormat = RendererFormats::DepthStencilFormat;

		GraphicsPSOs::PBR = pRenderDevice->CreateGraphicsPipelineState(prop);
	}

	// Skybox PSO
	{
		GraphicsPipelineState::Properties prop = {};
		prop.Desc.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::Skybox)->GetD3DRootSignature();
		prop.Desc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
		prop.Desc.VS = pRenderDevice->GetShader(Shaders::VS::Sky)->GetD3DShaderBytecode();
		prop.Desc.PS = pRenderDevice->GetShader(Shaders::PS::Sky)->GetD3DShaderBytecode();
		prop.Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		prop.Desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
		prop.Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		prop.Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		prop.Desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		prop.Desc.SampleMask = UINT_MAX;
		prop.Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		prop.Desc.NumRenderTargets = 1;
		prop.Desc.RTVFormats[0] = RendererFormats::HDRBufferFormat;
		prop.Desc.SampleDesc = { .Count = 1, .Quality = 0 };
		prop.Desc.DSVFormat = RendererFormats::DepthStencilFormat;

		GraphicsPSOs::Skybox = pRenderDevice->CreateGraphicsPipelineState(prop);
	}

	// Tonemapping PSO
	{
		GraphicsPipelineState::Properties prop = {};
		prop.Desc.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::PostProcess_Tonemapping)->GetD3DRootSignature();
		prop.Desc.VS = pRenderDevice->GetShader(Shaders::VS::Quad)->GetD3DShaderBytecode();
		prop.Desc.PS = pRenderDevice->GetShader(Shaders::PS::PostProcess_Tonemapping)->GetD3DShaderBytecode();
		prop.Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		prop.Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		prop.Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		prop.Desc.SampleMask = UINT_MAX;
		prop.Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		prop.Desc.NumRenderTargets = 1;
		prop.Desc.RTVFormats[0] = RendererFormats::SwapChainBufferFormat;
		prop.Desc.SampleDesc = { .Count = 1, .Quality = 0 };

		GraphicsPSOs::PostProcess_Tonemapping = pRenderDevice->CreateGraphicsPipelineState(prop);
	}
}

void ComputePSOs::Register(RenderDevice* pRenderDevice)
{
	// Generate mips PSO
	{
		ComputePipelineState::Properties prop = {};
		prop.Desc.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::GenerateMips)->GetD3DRootSignature();
		prop.Desc.CS = pRenderDevice->GetShader(Shaders::CS::GenerateMips)->GetD3DShaderBytecode();
		prop.Desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		ComputePSOs::GenerateMips = pRenderDevice->CreateComputePipelineState(prop);
	}

	// Equirectangular to cubemap PSO
	{
		ComputePipelineState::Properties prop = {};
		prop.Desc.pRootSignature = pRenderDevice->GetRootSignature(RootSignatures::EquirectangularToCubemap)->GetD3DRootSignature();
		prop.Desc.CS = pRenderDevice->GetShader(Shaders::CS::EquirectangularToCubemap)->GetD3DShaderBytecode();
		prop.Desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		ComputePSOs::EquirectangularToCubemap = pRenderDevice->CreateComputePipelineState(prop);
	}
}

void RaytracingPSOs::Register(RenderDevice* pRenderDevice)
{
	Raytracing = pRenderDevice->CreateRaytracingPipelineState([&](RaytracingPipelineStateProxy& proxy)
	{
		Library* pRayGenerationLibrary = pRenderDevice->GetLibrary(Libraries::RayGeneration);
		Library* pClosestHitLibrary = pRenderDevice->GetLibrary(Libraries::ClosestHit);
		Library* pMissLibrary = pRenderDevice->GetLibrary(Libraries::Miss);

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