#include "pch.h"
#include "RendererRegistry.h"

RenderResourceHandle RootSignatures::Shadow;
RenderResourceHandle RootSignatures::PBR;
RenderResourceHandle RootSignatures::Skybox;

RenderResourceHandle RootSignatures::BRDFIntegration;
RenderResourceHandle RootSignatures::IrradiaceConvolution;
RenderResourceHandle RootSignatures::PrefilterConvolution;

RenderResourceHandle RootSignatures::GenerateMips;
RenderResourceHandle RootSignatures::EquirectangularToCubemap;

RenderResourceHandle GraphicsPSOs::Shadow;
RenderResourceHandle GraphicsPSOs::PBR, GraphicsPSOs::PBRWireFrame;
RenderResourceHandle GraphicsPSOs::Skybox;

RenderResourceHandle GraphicsPSOs::BRDFIntegration;
RenderResourceHandle GraphicsPSOs::IrradiaceConvolution;
RenderResourceHandle GraphicsPSOs::PrefilterConvolution;

RenderResourceHandle ComputePSOs::GenerateMips;
RenderResourceHandle ComputePSOs::EquirectangularToCubemap;

enum class SamplerState : UINT
{
	PointWrap,
	PointClamp,
	LinearWrap,
	LinearClamp,
	AnisotropicWrap,
	AnisotropicClamp,
	ShadowPCF,

	Count
};

std::array<const D3D12_STATIC_SAMPLER_DESC, UINT(SamplerState::Count)> GetStaticSamplerStates()
{
	const CD3DX12_STATIC_SAMPLER_DESC pointWrap = CD3DX12_STATIC_SAMPLER_DESC(
		0,										// shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT,			// filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);		// addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp = CD3DX12_STATIC_SAMPLER_DESC(
		1,										// shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT,			// filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);		// addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap = CD3DX12_STATIC_SAMPLER_DESC(
		2,										// shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,		// filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);		// addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp = CD3DX12_STATIC_SAMPLER_DESC(
		3,										// shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR,		// filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);		// addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap = CD3DX12_STATIC_SAMPLER_DESC(
		4,										// shaderRegister
		D3D12_FILTER_ANISOTROPIC,				// filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,		// addressW
		0.0f,									// mipLODBias
		16);									// maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp = CD3DX12_STATIC_SAMPLER_DESC(
		5,										// shaderRegister
		D3D12_FILTER_ANISOTROPIC,				// filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressW
		0.0f,									// mipLODBias
		16);									// maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC shadow = CD3DX12_STATIC_SAMPLER_DESC(
		6,													// shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,	// filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,					// addressW
		0.0f,												// mipLODBias
		16,													// maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,					// comparision func
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);			// border color

	return { pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp, shadow };
}

void RootSignatures::Register(RenderDevice* pDevice)
{
	CORE_INFO("RootSignatures::Register invoked");
	auto staticSamplers = GetStaticSamplerStates();

	// Shadow RS
	{
		CD3DX12_ROOT_PARAMETER rootParameters[RootParameters::Shadow::ShadowRootParameter_Count];
		ZeroMemory(rootParameters, sizeof(rootParameters));

		rootParameters[RootParameters::Shadow::ShadowRootParameter_ObjectCBuffer].InitAsConstantBufferView(0); // Object cb view
		rootParameters[RootParameters::Shadow::ShadowRootParameter_ShadowPassCBuffer].InitAsConstantBufferView(1); // Shadow pass cb view

		RootSignature::Properties prop{};
		prop.Desc.NumParameters = ARRAYSIZE(rootParameters);
		prop.Desc.pParameters = rootParameters;
		prop.Desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		RootSignatures::Shadow = pDevice->CreateRootSignature(prop);
	}

	// PBR RS
	{
		CD3DX12_ROOT_PARAMETER rootParameters[RootParameters::PBR::PBRRootParameter_Count];
		ZeroMemory(rootParameters, sizeof(rootParameters));

		CD3DX12_DESCRIPTOR_RANGE textureSRVRange = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 5, 0);
		CD3DX12_DESCRIPTOR_RANGE shadowMapEnvMapSRVRange = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 5);

		rootParameters[RootParameters::PBR::PBRRootParameter_ObjectCBuffer].InitAsConstantBufferView(0);
		rootParameters[RootParameters::PBR::PBRRootParameter_MaterialCBuffer].InitAsConstantBufferView(1);
		rootParameters[RootParameters::PBR::PBRRootParameter_RenderPassCBuffer].InitAsConstantBufferView(2);
		rootParameters[RootParameters::PBR::PBRRootParameter_MaterialTexturesSRV].InitAsDescriptorTable(1, &textureSRVRange, D3D12_SHADER_VISIBILITY_PIXEL);
		rootParameters[RootParameters::PBR::PBRRootParameter_ShadowMapAndEnvironmentMapsSRV].InitAsDescriptorTable(1, &shadowMapEnvMapSRVRange, D3D12_SHADER_VISIBILITY_PIXEL);

		RootSignature::Properties prop{};
		prop.Desc.NumParameters = ARRAYSIZE(rootParameters);
		prop.Desc.pParameters = rootParameters;
		prop.Desc.NumStaticSamplers = staticSamplers.size();
		prop.Desc.pStaticSamplers = staticSamplers.data();
		prop.Desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

		RootSignatures::PBR = pDevice->CreateRootSignature(prop);
	}

	// Skybox RS
	{
		CD3DX12_ROOT_PARAMETER rootParameters[RootParameters::Skybox::SkyboxRootParameter_Count];
		ZeroMemory(rootParameters, sizeof(rootParameters));

		CD3DX12_DESCRIPTOR_RANGE srvRange = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

		rootParameters[RootParameters::Skybox::SkyboxRootParameter_RenderPassCBuffer].InitAsConstantBufferView(0);
		rootParameters[RootParameters::Skybox::SkyboxRootParameter_CubeMapSRV].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

		RootSignature::Properties prop{};
		prop.Desc.NumParameters = ARRAYSIZE(rootParameters);
		prop.Desc.pParameters = rootParameters;
		prop.Desc.NumStaticSamplers = staticSamplers.size();
		prop.Desc.pStaticSamplers = staticSamplers.data();
		prop.Desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		RootSignatures::Skybox = pDevice->CreateRootSignature(prop);
	}

	// BRDFIntegration RS
	{
		RootSignature::Properties prop{};
		prop.Desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		RootSignatures::BRDFIntegration = pDevice->CreateRootSignature(prop);
	}

	// Cubemap convolutions RS
	{
		for (int i = 0; i < CubemapConvolution::CubemapConvolution_Count; ++i)
		{
			CD3DX12_ROOT_PARAMETER rootParameters[RootParameters::CubemapConvolution::CubemapConvolutionRootParameter_Count];
			ZeroMemory(rootParameters, sizeof(rootParameters));

			CD3DX12_DESCRIPTOR_RANGE srvRange = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
			UINT num32bitValues = i == Irradiance ? sizeof(IrradianceConvolutionSetting) / 4 : sizeof(PrefilterConvolutionSetting) / 4;

			rootParameters[RootParameters::CubemapConvolution::CubemapConvolutionRootParameter_RenderPassCBuffer].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
			rootParameters[RootParameters::CubemapConvolution::CubemapConvolutionRootParameter_Setting].InitAsConstants(num32bitValues, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
			rootParameters[RootParameters::CubemapConvolution::CubemapConvolutionRootParameter_CubemapSRV].InitAsDescriptorTable(1, &srvRange, D3D12_SHADER_VISIBILITY_PIXEL);

			const CD3DX12_STATIC_SAMPLER_DESC  linearClamp = CD3DX12_STATIC_SAMPLER_DESC(
				0,										// shaderRegister
				D3D12_FILTER_MIN_MAG_MIP_LINEAR,		// filter
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressU
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP,		// addressV
				D3D12_TEXTURE_ADDRESS_MODE_CLAMP);		// addressW

			RootSignature::Properties prop{};
			prop.Desc.NumParameters = ARRAYSIZE(rootParameters);
			prop.Desc.pParameters = rootParameters;
			prop.Desc.NumStaticSamplers = 1;
			prop.Desc.pStaticSamplers = &linearClamp;
			prop.Desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
				D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

			switch (i)
			{
			case Irradiance:
				RootSignatures::IrradiaceConvolution = pDevice->CreateRootSignature(prop);
				break;
			case Prefilter:
				RootSignatures::PrefilterConvolution = pDevice->CreateRootSignature(prop);
				break;
			}
		}
	}

	// Generate mips RS
	{
		CD3DX12_ROOT_PARAMETER rootParameters[RootParameters::GenerateMips::GenerateMipsRootParameter_Count];

		CD3DX12_DESCRIPTOR_RANGE srcMip = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		CD3DX12_DESCRIPTOR_RANGE outMip = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 4, 0);

		rootParameters[RootParameters::GenerateMips::GenerateMipsRootParameter_GenerateMipsCB].InitAsConstants(sizeof(GenerateMipsCPUData) / 4, 0);
		rootParameters[RootParameters::GenerateMips::GenerateMipsRootParameter_SrcMip].InitAsDescriptorTable(1, &srcMip);
		rootParameters[RootParameters::GenerateMips::GenerateMipsRootParameter_OutMip].InitAsDescriptorTable(1, &outMip);

		CD3DX12_STATIC_SAMPLER_DESC linearClampSampler = CD3DX12_STATIC_SAMPLER_DESC(
			0,
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
			D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

		RootSignature::Properties prop{};
		prop.Desc.NumParameters = ARRAYSIZE(rootParameters);
		prop.Desc.pParameters = rootParameters;
		prop.Desc.NumStaticSamplers = 1;
		prop.Desc.pStaticSamplers = &linearClampSampler;
		prop.Desc.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		RootSignatures::GenerateMips = pDevice->CreateRootSignature(prop);
	}

	// Equirectangular to cubemap RS
	{
		CD3DX12_ROOT_PARAMETER rootParameters[RootParameters::EquirectangularToCubemap::EquirectangularToCubemapRootParameter_Count];

		CD3DX12_DESCRIPTOR_RANGE srcMip = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);
		CD3DX12_DESCRIPTOR_RANGE outMip = CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 5, 0);

		rootParameters[RootParameters::EquirectangularToCubemap::EquirectangularToCubemapRootParameter_PanoToCubemapCB].InitAsConstants(sizeof(EquirectangularToCubemapCPUData) / 4, 0);
		rootParameters[RootParameters::EquirectangularToCubemap::EquirectangularToCubemapRootParameter_SrcTexture].InitAsDescriptorTable(1, &srcMip);
		rootParameters[RootParameters::EquirectangularToCubemap::EquirectangularToCubemapRootParameter_DstMips].InitAsDescriptorTable(1, &outMip);

		CD3DX12_STATIC_SAMPLER_DESC linearRepeatSampler = CD3DX12_STATIC_SAMPLER_DESC(
			0,
			D3D12_FILTER_MIN_MAG_MIP_LINEAR,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP,
			D3D12_TEXTURE_ADDRESS_MODE_WRAP);

		RootSignature::Properties prop{};
		prop.Desc.NumParameters = ARRAYSIZE(rootParameters);
		prop.Desc.pParameters = rootParameters;
		prop.Desc.NumStaticSamplers = 1;
		prop.Desc.pStaticSamplers = &linearRepeatSampler;
		prop.Desc.Flags =
			D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
			D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

		RootSignatures::EquirectangularToCubemap = pDevice->CreateRootSignature(prop);
	}
	CORE_INFO("RootSignatures::Register complete");
}

void GraphicsPSOs::Register(RenderDevice* pDevice)
{
	CORE_INFO("GraphicsPSOs::Register invoked");
	std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout =
	{
		{"POSITION"  , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD"  , 0, DXGI_FORMAT_R32G32_FLOAT   , 0,  D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL"    , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TANGENT"   , 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,  D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
	};
	auto staticSamplers = GetStaticSamplerStates();

	// Shadow PSO
	{
		GraphicsPipelineState::Properties prop{};
		prop.Desc.pRootSignature = pDevice->GetRenderResource<RootSignature>(RootSignatures::Shadow)->GetD3DRootSignature();
		prop.Desc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
		prop.Desc.VS = pDevice->GetShaderManager().QueryShader(VS::VS_Shadow)->GetD3DShaderBytecode();
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

		GraphicsPSOs::Shadow = pDevice->CreateGraphicsPipelineState(prop);
	}

	// PBR PSO
	{
		GraphicsPipelineState::Properties prop{};
		prop.Desc.pRootSignature = pDevice->GetRenderResource<RootSignature>(RootSignatures::PBR)->GetD3DRootSignature();
		prop.Desc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
		prop.Desc.VS = pDevice->GetShaderManager().QueryShader(VS::VS_Default)->GetD3DShaderBytecode();
		prop.Desc.PS = pDevice->GetShaderManager().QueryShader(PS::PS_PBR)->GetD3DShaderBytecode();
		prop.Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		prop.Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		prop.Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		prop.Desc.SampleMask = UINT_MAX;
		prop.Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		prop.Desc.NumRenderTargets = 1;
		prop.Desc.RTVFormats[0] = RendererFormats::HDRBufferFormat;
		prop.Desc.SampleDesc = { .Count = 1, .Quality = 0 };
		prop.Desc.DSVFormat = RendererFormats::DepthStencilFormat;

		GraphicsPSOs::PBR = pDevice->CreateGraphicsPipelineState(prop);

		// PBR wire frame PSO
		prop.Desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;

		GraphicsPSOs::PBRWireFrame = pDevice->CreateGraphicsPipelineState(prop);
	}

	// Skybox PSO
	{
		GraphicsPipelineState::Properties prop{};
		prop.Desc.pRootSignature = pDevice->GetRenderResource<RootSignature>(RootSignatures::Skybox)->GetD3DRootSignature();
		prop.Desc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
		prop.Desc.VS = pDevice->GetShaderManager().QueryShader(VS::VS_Sky)->GetD3DShaderBytecode();
		prop.Desc.PS = pDevice->GetShaderManager().QueryShader(PS::PS_Sky)->GetD3DShaderBytecode();
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

		GraphicsPSOs::Skybox = pDevice->CreateGraphicsPipelineState(prop);
	}

	// BRDF Integration PSO
	{
		GraphicsPipelineState::Properties prop{};
		prop.Desc.pRootSignature = pDevice->GetRenderResource<RootSignature>(RootSignatures::BRDFIntegration)->GetD3DRootSignature();
		prop.Desc.VS = pDevice->GetShaderManager().QueryShader(VS::VS_Quad)->GetD3DShaderBytecode();
		prop.Desc.PS = pDevice->GetShaderManager().QueryShader(PS::PS_BRDFIntegration)->GetD3DShaderBytecode();
		prop.Desc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		prop.Desc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		prop.Desc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		prop.Desc.SampleMask = UINT_MAX;
		prop.Desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		prop.Desc.NumRenderTargets = 1;
		prop.Desc.RTVFormats[0] = RendererFormats::BRDFLUTFormat;
		prop.Desc.SampleDesc = { .Count = 1, .Quality = 0 };

		GraphicsPSOs::BRDFIntegration = pDevice->CreateGraphicsPipelineState(prop);
	}

	// Cubemap convolutions PSO
	{
		for (int i = 0; i < CubemapConvolution::CubemapConvolution_Count; ++i)
		{
			GraphicsPipelineState::Properties prop{};
			prop.Desc.pRootSignature = i == CubemapConvolution::Irradiance ? pDevice->GetRenderResource<RootSignature>(RootSignatures::IrradiaceConvolution)->GetD3DRootSignature()
				: pDevice->GetRenderResource<RootSignature>(RootSignatures::PrefilterConvolution)->GetD3DRootSignature();
			prop.Desc.InputLayout = { inputLayout.data(), (UINT)inputLayout.size() };
			prop.Desc.VS = pDevice->GetShaderManager().QueryShader(VS::VS_Sky)->GetD3DShaderBytecode();
			switch (i)
			{
			case Irradiance:
				prop.Desc.PS = pDevice->GetShaderManager().QueryShader(PS::PS_IrradianceConvolution)->GetD3DShaderBytecode();
				break;
			case Prefilter:
				prop.Desc.PS = pDevice->GetShaderManager().QueryShader(PS::PS_PrefilterConvolution)->GetD3DShaderBytecode();
				break;
			}
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
				GraphicsPSOs::IrradiaceConvolution = pDevice->CreateGraphicsPipelineState(prop);
				break;
			case Prefilter:
				GraphicsPSOs::PrefilterConvolution = pDevice->CreateGraphicsPipelineState(prop);
				break;
			}
		}
	}
	CORE_INFO("GraphicsPSOs::Register complete");
}

void ComputePSOs::Register(RenderDevice* pDevice)
{
	CORE_INFO("ComputePSOs::Register invoked");
	// Generate mips PSO
	{
		ComputePipelineState::Properties prop{};
		prop.Desc.pRootSignature = pDevice->GetRenderResource<RootSignature>(RootSignatures::GenerateMips)->GetD3DRootSignature();
		prop.Desc.CS = pDevice->GetShaderManager().QueryShader(CS::CS_GenerateMips)->GetD3DShaderBytecode();
		prop.Desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		ComputePSOs::GenerateMips = pDevice->CreateComputePipelineState(prop);
	}

	// Equirectangular to cubemap PSO
	{
		ComputePipelineState::Properties prop{};
		prop.Desc.pRootSignature = pDevice->GetRenderResource<RootSignature>(RootSignatures::EquirectangularToCubemap)->GetD3DRootSignature();
		prop.Desc.CS = pDevice->GetShaderManager().QueryShader(CS::CS_EquirectangularToCubeMap)->GetD3DShaderBytecode();
		prop.Desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

		ComputePSOs::EquirectangularToCubemap = pDevice->CreateComputePipelineState(prop);
	}
	CORE_INFO("ComputePSOs::Register complete");
}