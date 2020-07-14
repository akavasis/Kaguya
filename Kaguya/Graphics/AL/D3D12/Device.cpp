#include "pch.h"
#include "Device.h"
using Microsoft::WRL::ComPtr;

Device::Device(IDXGIAdapter4* pAdapter)
	: m_CBVAllocator(this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
	m_SRVAllocator(this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
	m_UAVAllocator(this, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
	m_RTVAllocator(this, D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
	m_DSVAllocator(this, D3D12_DESCRIPTOR_HEAP_TYPE_DSV),
	m_SamplerAllocator(this, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
{
#if defined(_DEBUG)
	constexpr BOOL gpuBasedValidation = FALSE; // Enabling this will cause texture copying to copy black pixels
	constexpr BOOL synchronizedCmdQueueValidation = TRUE;
	// NOTE: Enabling the debug layer after creating the ID3D12Device will cause the DX runtime to remove the device.
	ComPtr<ID3D12Debug1> pDebug1;
	ThrowCOMIfFailed(::D3D12GetDebugInterface(IID_PPV_ARGS(pDebug1.ReleaseAndGetAddressOf())));
	pDebug1->EnableDebugLayer();
	pDebug1->SetEnableGPUBasedValidation(gpuBasedValidation);
	pDebug1->SetEnableSynchronizedCommandQueueValidation(synchronizedCmdQueueValidation);
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
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	ThrowCOMIfFailed(m_pDevice->QueryInterface(IID_PPV_ARGS(pInfoQueue.ReleaseAndGetAddressOf())));
	ThrowCOMIfFailed(pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE));
	ThrowCOMIfFailed(pInfoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE));

	// Suppress messages based on their severity level
	D3D12_MESSAGE_SEVERITY severities[] =
	{
		D3D12_MESSAGE_SEVERITY_INFO
	};

	// Suppress individual messages by their ID
	D3D12_MESSAGE_ID denyIDs[] =
	{
		D3D12_MESSAGE_ID_MAP_INVALID_NULLRANGE,		// This warning occurs when using capture frame while graphics debugging.
		D3D12_MESSAGE_ID_UNMAP_INVALID_NULLRANGE,	// This warning occurs when using capture frame while graphics debugging.
		D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_DEPTHSTENCILVIEW_NOT_SET, // This warning occurs when setting DSV Format for PSO as DXGI_UNKNOWN for post processing
		D3D12_MESSAGE_ID_RESOURCE_BARRIER_BEFORE_AFTER_MISMATCH, // This warning occurs when subresources are in incorrect states but they will be resolved in resource state tracker
		D3D12_MESSAGE_ID_INVALID_SUBRESOURCE_STATE	// This warning occurs when subresources are in incorrect states but they will be resolved in resource state tracker
	};

	D3D12_INFO_QUEUE_FILTER infoQueueFilter = {};
	infoQueueFilter.DenyList.NumSeverities = ARRAYSIZE(severities);
	infoQueueFilter.DenyList.pSeverityList = severities;
	infoQueueFilter.DenyList.NumIDs = ARRAYSIZE(denyIDs);
	infoQueueFilter.DenyList.pIDList = denyIDs;

	ThrowCOMIfFailed(pInfoQueue->PushStorageFilter(&infoQueueFilter));
#endif
}

Device::~Device()
{
}