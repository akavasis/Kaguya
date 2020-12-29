#include "pch.h"
#include "Device.h"

#define GPU_BASED_VALIDATION 0

void Device::Create(IDXGIAdapter4* pAdapter)
{
	/*
		ID3D12Device1					Windows 10 Anniversary Update	(14393)
		ID3D12Device2					Windows 10 Creators Update		(15063)
		ID3D12Device3					Windows 10 Fall Creators Update (16299)
		ID3D12Device4					Windows 10 April 2018 Update	(17134)
		ID3D12Device5					Windows 10 October 2018			(17763)
		ID3D12Device6					Windows 10 May 2019				(18362)
		ID3D12Device7, ID3D12Device8	Windows 10 May 2020 Update		(19041)
	*/
#if defined(_DEBUG)
	// NOTE: Enabling the debug layer after creating the ID3D12Device will cause the DX runtime to remove the device.
	Microsoft::WRL::ComPtr<ID3D12Debug1> pDebug1;
	ThrowCOMIfFailed(::D3D12GetDebugInterface(IID_PPV_ARGS(pDebug1.ReleaseAndGetAddressOf())));
	pDebug1->EnableDebugLayer();
#if GPU_BASED_VALIDATION
	// Enabling this will cause texture copying to copy black pixels, also disables you from binding acceleration structure because its in the wrong state
	pDebug1->SetEnableGPUBasedValidation(TRUE);
#endif
#endif

	constexpr D3D_FEATURE_LEVEL MinimumFeatureLevel = D3D_FEATURE_LEVEL_12_1;
	// Create our virtual device used for interacting with the GPU so we can create resources
	ThrowCOMIfFailed(::D3D12CreateDevice(pAdapter, MinimumFeatureLevel, IID_PPV_ARGS(m_ApiHandle.ReleaseAndGetAddressOf())));

	// Check for different features
	CheckRootSignature_1_1Support();
	CheckShaderModel6PlusSupport();
	CheckRaytracingSupport();
	CheckMeshShaderSupport();

#if defined(_DEBUG)
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> pInfoQueue;
	ThrowCOMIfFailed(m_ApiHandle->QueryInterface(IID_PPV_ARGS(pInfoQueue.ReleaseAndGetAddressOf())));
	ThrowCOMIfFailed(pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
	ThrowCOMIfFailed(pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
#endif

	// Check for UAV format support
	DXGI_FORMAT OptionalFormats[] =
	{
		DXGI_FORMAT_R16G16B16A16_UNORM,
		DXGI_FORMAT_R16G16B16A16_SNORM,
		DXGI_FORMAT_R32G32_FLOAT,
		DXGI_FORMAT_R32G32_UINT,
		DXGI_FORMAT_R32G32_SINT,
		DXGI_FORMAT_R10G10B10A2_UNORM,
		DXGI_FORMAT_R10G10B10A2_UINT,
		DXGI_FORMAT_R11G11B10_FLOAT,
		DXGI_FORMAT_R8G8B8A8_SNORM,
		DXGI_FORMAT_R16G16_FLOAT,
		DXGI_FORMAT_R16G16_UNORM,
		DXGI_FORMAT_R16G16_UINT,
		DXGI_FORMAT_R16G16_SNORM,
		DXGI_FORMAT_R16G16_SINT,
		DXGI_FORMAT_R8G8_UNORM,
		DXGI_FORMAT_R8G8_UINT,
		DXGI_FORMAT_R8G8_SNORM,
		DXGI_FORMAT_R8G8_SINT,
		DXGI_FORMAT_R16_UNORM,
		DXGI_FORMAT_R16_SNORM,
		DXGI_FORMAT_R8_SNORM,
		DXGI_FORMAT_A8_UNORM,
		DXGI_FORMAT_B5G6R5_UNORM,
		DXGI_FORMAT_B5G5R5A1_UNORM,
		DXGI_FORMAT_B4G4R4A4_UNORM
	};
	const int NumOptionalFormats = ARRAYSIZE(OptionalFormats);
	for (int i = 0; i < NumOptionalFormats; ++i)
	{
		D3D12_FEATURE_DATA_FORMAT_SUPPORT FeatureDataFormatSupport = {};
		FeatureDataFormatSupport.Format = OptionalFormats[i];

		ThrowCOMIfFailed(m_ApiHandle->CheckFeatureSupport(
			D3D12_FEATURE_FORMAT_SUPPORT,
			&FeatureDataFormatSupport,
			sizeof(D3D12_FEATURE_DATA_FORMAT_SUPPORT)));

		auto CheckFormatSupport1 = [&](D3D12_FORMAT_SUPPORT1 FormatSupport)
		{
			return (FeatureDataFormatSupport.Support1 & FormatSupport) != 0;
		};
		auto CheckFormatSupport2 = [&](D3D12_FORMAT_SUPPORT2 FormatSupport)
		{
			return (FeatureDataFormatSupport.Support2 & FormatSupport) != 0;
		};

		bool SupportUAV = CheckFormatSupport1(D3D12_FORMAT_SUPPORT1_TYPED_UNORDERED_ACCESS_VIEW) &&
			CheckFormatSupport2(D3D12_FORMAT_SUPPORT2_UAV_TYPED_LOAD) &&
			CheckFormatSupport2(D3D12_FORMAT_SUPPORT2_UAV_TYPED_STORE);

		if (SupportUAV)
		{
			m_UAVSupportedFormats.insert(OptionalFormats[i]);
		}
	}
}

bool Device::IsUAVCompatable(DXGI_FORMAT Format) const
{
	switch (Format)
	{
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32A32_SINT:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32_UINT:
	case DXGI_FORMAT_R32_SINT:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_R16_UINT:
	case DXGI_FORMAT_R16_SINT:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SINT:
		return true;
	}
	if (m_UAVSupportedFormats.find(Format) != m_UAVSupportedFormats.end())
		return true;
	return false;
}

void Device::CheckRootSignature_1_1Support()
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE RootSignature = {};
	RootSignature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	ThrowCOMIfFailed(m_ApiHandle->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &RootSignature, sizeof(RootSignature)));
	if (RootSignature.HighestVersion < D3D_ROOT_SIGNATURE_VERSION_1_1)
		throw std::runtime_error("RS 1.1 not supported on device");
}

void Device::CheckShaderModel6PlusSupport()
{
	D3D12_FEATURE_DATA_SHADER_MODEL ShaderModel = { D3D_SHADER_MODEL_6_5 };
	ThrowCOMIfFailed(m_ApiHandle->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &ShaderModel, sizeof(ShaderModel)));
	if (ShaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0)
		throw std::runtime_error("Shader Model 6+ not supported on device");
}

void Device::CheckRaytracingSupport()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 D3D12Options5 = {};
	ThrowCOMIfFailed(m_ApiHandle->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &D3D12Options5, sizeof(D3D12Options5)));
	if (D3D12Options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
		throw std::runtime_error("Raytracing not supported on device");
}

void Device::CheckMeshShaderSupport()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS7 D3D12Options7 = {};
	ThrowCOMIfFailed(m_ApiHandle->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS7, &D3D12Options7, sizeof(D3D12Options7)));
	if (D3D12Options7.MeshShaderTier < D3D12_MESH_SHADER_TIER_1)
		throw std::runtime_error("Mesh shader not supported on device");
}