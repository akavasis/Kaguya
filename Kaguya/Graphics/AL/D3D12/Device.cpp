#include "pch.h"
#include "Device.h"

Device::Device(IDXGIAdapter4* pAdapter)
{
#if defined(_DEBUG)
	constexpr BOOL gpuBasedValidation = FALSE; // Enabling this will cause texture copying to copy black pixels, also disables you from binding acceleration structure because its in the wrong state
	// NOTE: Enabling the debug layer after creating the ID3D12Device will cause the DX runtime to remove the device.
	Microsoft::WRL::ComPtr<ID3D12Debug1> pDebug1;
	ThrowCOMIfFailed(::D3D12GetDebugInterface(IID_PPV_ARGS(pDebug1.ReleaseAndGetAddressOf())));
	pDebug1->EnableDebugLayer();
	pDebug1->SetEnableGPUBasedValidation(gpuBasedValidation);
#endif

	constexpr D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL_12_1;
	// Create our virtual device used for interacting with the GPU so we can create resources
	ThrowCOMIfFailed(::D3D12CreateDevice(pAdapter, minimumFeatureLevel, IID_PPV_ARGS(m_pDevice5.ReleaseAndGetAddressOf())));

	// Check for different features
	CheckRS_1_1Support();
	CheckSM6PlusSupport();
	CheckRaytracingSupport();

	// Query descriptor size (descriptor size vary based on GPU vendor)
	for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		m_DescriptorHandleIncrementSizeCache[i] = m_pDevice5->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE(i));
	}

#if defined(_DEBUG)
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> pInfoQueue;
	ThrowCOMIfFailed(m_pDevice5->QueryInterface(IID_PPV_ARGS(pInfoQueue.ReleaseAndGetAddressOf())));
	ThrowCOMIfFailed(pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
	ThrowCOMIfFailed(pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
#endif
}

Device::~Device()
{
}

void Device::CheckRS_1_1Support()
{
	D3D12_FEATURE_DATA_ROOT_SIGNATURE rootSignature = {};
	rootSignature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	ThrowCOMIfFailed(m_pDevice5->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &rootSignature, sizeof(rootSignature)));
	if (rootSignature.HighestVersion < D3D_ROOT_SIGNATURE_VERSION_1_1)
		throw std::runtime_error("RS 1.1 not supported on device");
}

void Device::CheckSM6PlusSupport()
{
	D3D12_FEATURE_DATA_SHADER_MODEL shaderModel = {};
	shaderModel.HighestShaderModel = D3D_SHADER_MODEL_6_5;
	ThrowCOMIfFailed(m_pDevice5->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &shaderModel, sizeof(shaderModel)));
	if (shaderModel.HighestShaderModel < D3D_SHADER_MODEL_6_0)
		throw std::runtime_error("SM6+ not supported on device");
}

void Device::CheckRaytracingSupport()
{
	D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
	ThrowCOMIfFailed(m_pDevice5->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));
	if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
		throw std::runtime_error("Raytracing not supported on device");
}