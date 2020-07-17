#include "pch.h"
#include "Device.h"

Device::Device(IDXGIAdapter4* pAdapter)
{
#if defined(_DEBUG)
	constexpr BOOL gpuBasedValidation = TRUE; // Enabling this will cause texture copying to copy black pixels
	// NOTE: Enabling the debug layer after creating the ID3D12Device will cause the DX runtime to remove the device.
	Microsoft::WRL::ComPtr<ID3D12Debug1> pDebug1;
	ThrowCOMIfFailed(::D3D12GetDebugInterface(IID_PPV_ARGS(pDebug1.ReleaseAndGetAddressOf())));
	pDebug1->EnableDebugLayer();
	pDebug1->SetEnableGPUBasedValidation(gpuBasedValidation);
#endif

	const D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL_12_1;
	// Create our virtual device used for interacting with the GPU so we can create resources
	ThrowCOMIfFailed(::D3D12CreateDevice(pAdapter, minimumFeatureLevel, IID_PPV_ARGS(m_pDevice.ReleaseAndGetAddressOf())));

	// Check for different features
	D3D12_FEATURE_DATA_D3D12_OPTIONS features = {};
	ThrowCOMIfFailed(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &features, sizeof(features)));

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureDataRootSignature = {};
	featureDataRootSignature.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	ThrowCOMIfFailed(m_pDevice->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureDataRootSignature, sizeof(featureDataRootSignature)));

	// Query descriptor size (descriptor size vary based on GPU vendor) and create descriptor allocator
	for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		m_DescriptorHandleIncrementSizeCache[i] = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE(i));
	}

#if defined(_DEBUG)
	Microsoft::WRL::ComPtr<ID3D12InfoQueue> pInfoQueue;
	ThrowCOMIfFailed(m_pDevice->QueryInterface(IID_PPV_ARGS(pInfoQueue.ReleaseAndGetAddressOf())));
	ThrowCOMIfFailed(pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
	ThrowCOMIfFailed(pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));
#endif
}

Device::~Device()
{
}