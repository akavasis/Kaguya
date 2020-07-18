#include "pch.h"
#include "RendererRegistry.h"

enum SamplerStates : std::size_t
{
	PointWrap,
	PointClamp,
	LinearWrap,
	LinearClamp,
	AnisotropicWrap,
	AnisotropicClamp,
	ShadowPCF,

	NumSamplerStates
};

static std::array<const D3D12_STATIC_SAMPLER_DESC, SamplerStates::NumSamplerStates> GetStaticSamplerStates()
{
	static const CD3DX12_STATIC_SAMPLER_DESC pointWrap = CD3DX12_STATIC_SAMPLER_DESC(
		0,										// shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT,			// filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);		// addressW

	static const CD3DX12_STATIC_SAMPLER_DESC pointClamp = CD3DX12_STATIC_SAMPLER_DESC(
		1,										// shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT,			// filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);		// addressW

	static const CD3DX12_STATIC_SAMPLER_DESC linearWrap = CD3DX12_STATIC_SAMPLER_DESC(
		2,										// shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,		// filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);		// addressW

	static const CD3DX12_STATIC_SAMPLER_DESC linearClamp = CD3DX12_STATIC_SAMPLER_DESC(
		3,										// shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,		// filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);		// addressW

	static const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap = CD3DX12_STATIC_SAMPLER_DESC(
		4,										// shaderRegister
		D3D12_FILTER_ANISOTROPIC,				// filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressW
		0.0f,									// mipLODBias
		16);									// maxAnisotropy

	static const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp = CD3DX12_STATIC_SAMPLER_DESC(
		5,										// shaderRegister
		D3D12_FILTER_ANISOTROPIC,				// filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressW
		0.0f,									// mipLODBias
		16);									// maxAnisotropy

	static const CD3DX12_STATIC_SAMPLER_DESC shadow = CD3DX12_STATIC_SAMPLER_DESC(
		6,													// shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,	// filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressW
		0.0f,												// mipLODBias
		16,													// maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,					// comparision func
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);			// border color

	return
	{
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp, shadow
	};
}

enum DescriptorTables
{
	Tex2DTableRange,
	Tex2DArrayTableRange,
	TexCubeTableRange,

	NumDescriptorTables
};

static std::array<const D3D12_DESCRIPTOR_RANGE1, std::size_t(DescriptorTables::NumDescriptorTables)> GetStandardDescriptorRanges()
{
	static D3D12_DESCRIPTOR_RANGE1 standardDescriptorTables[DescriptorTables::NumDescriptorTables] = {};
	for (std::size_t i = 0; i < DescriptorTables::NumDescriptorTables; ++i)
	{
		standardDescriptorTables[i].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
		standardDescriptorTables[i].NumDescriptors = UINT_MAX;
		standardDescriptorTables[i].BaseShaderRegister = 0;
		standardDescriptorTables[i].RegisterSpace = i;
		standardDescriptorTables[i].OffsetInDescriptorsFromTableStart = 0;
		standardDescriptorTables[i].Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;
	}

	return
	{
		standardDescriptorTables[Tex2DTableRange],
		standardDescriptorTables[Tex2DArrayTableRange],
		standardDescriptorTables[TexCubeTableRange]
	};
}

void Shaders::Register(RenderDevice* pRenderDevice)
{
	// Load VS
	VS::Default = pRenderDevice->LoadShader(Shader::Type::Vertex, L"../../Shaders/VS_Default.hlsl", L"main", {});
	VS::Quad = pRenderDevice->LoadShader(Shader::Type::Vertex, L"../../Shaders/VS_Quad.hlsl", L"main", {});
	VS::Shadow = pRenderDevice->LoadShader(Shader::Type::Vertex, L"../../Shaders/VS_Shadow.hlsl", L"main", {});
	VS::Sky = pRenderDevice->LoadShader(Shader::Type::Vertex, L"../../Shaders/VS_Sky.hlsl", L"main", {});
	// Load PS
	PS::BRDFIntegration = pRenderDevice->LoadShader(Shader::Type::Pixel, L"../../Shaders/PS_BRDFIntegration.hlsl", L"main", {});
	PS::ConvolutionIrradiance = pRenderDevice->LoadShader(Shader::Type::Pixel, L"../../Shaders/PS_ConvolutionIrradiance.hlsl", L"main", {});
	PS::ConvolutionPrefilter = pRenderDevice->LoadShader(Shader::Type::Pixel, L"../../Shaders/PS_ConvolutionPrefilter.hlsl", L"main", {});
	PS::PBR = pRenderDevice->LoadShader(Shader::Type::Pixel, L"../../Shaders/PS_PBR.hlsl", L"main", {});
	PS::Sky = pRenderDevice->LoadShader(Shader::Type::Pixel, L"../../Shaders/PS_Sky.hlsl", L"main", {});
	PS::PostProcess_BloomComposite = pRenderDevice->LoadShader(Shader::Type::Pixel, L"../../Shaders/PostProcess/BloomComposite.hlsl", L"main", {});
	PS::PostProcess_BloomMask = pRenderDevice->LoadShader(Shader::Type::Pixel, L"../../Shaders/PostProcess/BloomMask.hlsl", L"main", {});
	PS::PostProcess_Sepia = pRenderDevice->LoadShader(Shader::Type::Pixel, L"../../Shaders/PostProcess/Sepia.hlsl", L"main", {});
	PS::PostProcess_SobelComposite = pRenderDevice->LoadShader(Shader::Type::Pixel, L"../../Shaders/PostProcess/SobelComposite.hlsl", L"main", {});
	PS::PostProcess_ToneMapping = pRenderDevice->LoadShader(Shader::Type::Pixel, L"../../Shaders/PostProcess/ToneMapping.hlsl", L"main", {});
	// Load CS
	CS::EquirectangularToCubemap = pRenderDevice->LoadShader(Shader::Type::Compute, L"../../Shaders/CS_EquirectangularToCubemap.hlsl", L"main", {});
	CS::GenerateMips = pRenderDevice->LoadShader(Shader::Type::Compute, L"../../Shaders/CS_GenerateMips.hlsl", L"main", {});
	CS::PostProcess_BlurHorizontal = pRenderDevice->LoadShader(Shader::Type::Compute, L"../../Shaders/PostProcess/Blur.hlsl", L"main", { { L"HORIZONTAL", L"1" } });
	CS::PostProcess_BlurVertical = pRenderDevice->LoadShader(Shader::Type::Compute, L"../../Shaders/PostProcess/Blur.hlsl", L"main", { { L"VERTICAL", L"1" } });
}

void RootSignatures::Register(RenderDevice* pRenderDevice)
{
	auto staticSamplers = GetStaticSamplerStates();
	auto standardDescriptorTables = GetStandardDescriptorRanges();

	// BRDFIntegration RS, this is just a empty root signature
	{
		RootSignature::Properties prop = {};
		prop.Desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		RootSignatures::BRDFIntegration = pRenderDevice->CreateRootSignature(prop);
	}

	// Cubemap convolutions RS
	{
		CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::CubemapConvolution::Count] = {};
		for (int i = 0; i < CubemapConvolution::CubemapConvolution_Count; ++i)
		{
			D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
			CD3DX12_DESCRIPTOR_RANGE1 srvRange = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, volatileFlag);
			const UINT num32bitValues = i == Irradiance ? sizeof(ConvolutionIrradianceSetting) / 4 : sizeof(ConvolutionPrefilterSetting) / 4;

			rootParameters[RootParameters::CubemapConvolution::RenderPassCBuffer].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC, D3D12_SHADER_VISIBILITY_VERTEX);
			rootParameters[RootParameters::CubemapConvolution::Setting].InitAsConstants(num32bitValues, 0, 0, D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameters[RootParameters::CubemapConvolution::CubemapSRV].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

			const CD3DX12_STATIC_SAMPLER_DESC linearClamp = CD3DX12_STATIC_SAMPLER_DESC(
				0,										// shaderRegister
				D3D12_FILTER_MIN_MAG_MIP_LINEAR,		// filter
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressU
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressV
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP);		// addressW

			RootSignature::Properties prop = {};
			prop.Desc.NumParameters = ARRAYSIZE(rootParameters);
			prop.Desc.pParameters = rootParameters;
			prop.Desc.NumStaticSamplers = 1;
			prop.Desc.pStaticSamplers = &linearClamp;
			prop.AllowInputLayout();
			prop.DenyHSAccess();
			prop.DenyDSAccess();
			prop.DenyGSAccess();

			switch (i)
			{
			case Irradiance:
				RootSignatures::ConvolutionIrradiace = pRenderDevice->CreateRootSignature(prop);
				break;
			case Prefilter:
				RootSignatures::ConvolutionPrefilter = pRenderDevice->CreateRootSignature(prop);
				break;
			}
		}
	}

	// Shadow RS
	{
		CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::Shadow::Count] = {};

		rootParameters[RootParameters::Shadow::ObjectCBuffer].InitAsConstantBufferView(0, 0);
		rootParameters[RootParameters::Shadow::RenderPassCBuffer].InitAsConstantBufferView(1, 0);

		RootSignature::Properties prop = {};
		prop.Desc.NumParameters = ARRAYSIZE(rootParameters);
		prop.Desc.pParameters = rootParameters;
		prop.AllowInputLayout();

		RootSignatures::Shadow = pRenderDevice->CreateRootSignature(prop);
	}

	// PBR RS
	{
		CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::PBR::Count] = {};

		rootParameters[RootParameters::PBR::ObjectCBuffer].InitAsConstantBufferView(0, 0);
		rootParameters[RootParameters::PBR::RenderPassCBuffer].InitAsConstantBufferView(1, 0);
		rootParameters[RootParameters::PBR::MaterialTextureIndicesSBuffer].InitAsShaderResourceView(0, 100);
		rootParameters[RootParameters::PBR::DescriptorTables].InitAsDescriptorTable(standardDescriptorTables.size(), standardDescriptorTables.data());

		RootSignature::Properties prop = {};
		prop.Desc.NumParameters = ARRAYSIZE(rootParameters);
		prop.Desc.pParameters = rootParameters;
		prop.Desc.NumStaticSamplers = staticSamplers.size();
		prop.Desc.pStaticSamplers = staticSamplers.data();
		prop.AllowInputLayout();

		RootSignatures::PBR = pRenderDevice->CreateRootSignature(prop);
	}

	// Skybox RS
	{
		CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::Skybox::Count] = {};

		rootParameters[RootParameters::Skybox::RenderPassCBuffer].InitAsConstantBufferView(0, 0);
		rootParameters[RootParameters::Skybox::MaterialTextureIndicesSBuffer].InitAsConstantBufferView(0, 100);
		rootParameters[RootParameters::Skybox::DescriptorTables].InitAsDescriptorTable(standardDescriptorTables.size(), standardDescriptorTables.data());

		RootSignature::Properties prop = {};
		prop.Desc.NumParameters = ARRAYSIZE(rootParameters);
		prop.Desc.pParameters = rootParameters;
		prop.Desc.NumStaticSamplers = staticSamplers.size();
		prop.Desc.pStaticSamplers = staticSamplers.data();
		prop.AllowInputLayout();
		prop.DenyHSAccess();
		prop.DenyDSAccess();
		prop.DenyGSAccess();

		RootSignatures::Skybox = pRenderDevice->CreateRootSignature(prop);
	}

	// Generate mips RS
	{
		CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::GenerateMips::Count] = {};

		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		CD3DX12_DESCRIPTOR_RANGE1 srcMip = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 outMip = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0, 0, volatileFlag);

		rootParameters[RootParameters::GenerateMips::GenerateMipsCBuffer].InitAsConstants(sizeof(GenerateMipsCPUData) / 4, 0);
		rootParameters[RootParameters::GenerateMips::SrcMip].InitAsDescriptorTable(1, &srcMip);
		rootParameters[RootParameters::GenerateMips::OutMips].InitAsDescriptorTable(1, &outMip);

		CD3DX12_STATIC_SAMPLER_DESC linearClampSampler = CD3DX12_STATIC_SAMPLER_DESC(
			0,
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		RootSignature::Properties prop = {};
		prop.Desc.NumParameters = ARRAYSIZE(rootParameters);
		prop.Desc.pParameters = rootParameters;
		prop.Desc.NumStaticSamplers = 1;
		prop.Desc.pStaticSamplers = &linearClampSampler;
		prop.DenyVSAccess();
		prop.DenyHSAccess();
		prop.DenyDSAccess();
		prop.DenyGSAccess();

		RootSignatures::GenerateMips = pRenderDevice->CreateRootSignature(prop);
	}

	// Equirectangular to cubemap RS
	{
		CD3DX12_ROOT_PARAMETER1 rootParameters[RootParameters::EquirectangularToCubemap::Count] = {};

		D3D12_DESCRIPTOR_RANGE_FLAGS volatileFlag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE;
		CD3DX12_DESCRIPTOR_RANGE1 srcMip = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, volatileFlag);
		CD3DX12_DESCRIPTOR_RANGE1 outMip = CD3DX12_DESCRIPTOR_RANGE1(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 0, 0, volatileFlag);

		rootParameters[RootParameters::EquirectangularToCubemap::PanoToCubemapCBuffer].InitAsConstants(sizeof(EquirectangularToCubemapCPUData) / 4, 0);
		rootParameters[RootParameters::EquirectangularToCubemap::SrcMip].InitAsDescriptorTable(1, &srcMip);
		rootParameters[RootParameters::EquirectangularToCubemap::OutMips].InitAsDescriptorTable(1, &outMip);

		CD3DX12_STATIC_SAMPLER_DESC linearWrap = CD3DX12_STATIC_SAMPLER_DESC(
			0,
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP);

		RootSignature::Properties prop = {};
		prop.Desc.NumParameters = ARRAYSIZE(rootParameters);
		prop.Desc.pParameters = rootParameters;
		prop.Desc.NumStaticSamplers = 1;
		prop.Desc.pStaticSamplers = &linearWrap;
		prop.DenyVSAccess();
		prop.DenyHSAccess();
		prop.DenyDSAccess();
		prop.DenyGSAccess();

		RootSignatures::EquirectangularToCubemap = pRenderDevice->CreateRootSignature(prop);
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