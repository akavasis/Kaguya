#include "pch.h"
#include "RenderDevice.h"
using Microsoft::WRL::ComPtr;

RenderDevice::RenderDevice(IUnknown* pAdapter, BOOL GPUBasedValidation)
{
#if defined(_DEBUG)
	// NOTE: Enabling the debug layer after creating the ID3D12Device will cause the DX runtime to remove the device.
	ComPtr<ID3D12Debug1> pDebug1;
	ThrowCOMIfFailed(::D3D12GetDebugInterface(IID_PPV_ARGS(&pDebug1)));
	pDebug1->EnableDebugLayer();
	pDebug1->SetEnableGPUBasedValidation(GPUBasedValidation);
#endif

	const D3D_FEATURE_LEVEL minimumFeatureLevel = D3D_FEATURE_LEVEL_12_1;
	// Create our virtual device used for interacting with the GPU so we can create resources
	ThrowCOMIfFailed(::D3D12CreateDevice(pAdapter, minimumFeatureLevel, IID_PPV_ARGS(&m_pDevice)));

	// Check for different features
	D3D12_FEATURE_DATA_D3D12_OPTIONS features = {};
	m_pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS, &features, sizeof(features));

	// Get descriptor handle increment size and cache them (descriptor size vary based on GPU vendor)
	for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		m_DescriptorHandleIncrementSizeCache[i] = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE(i));
	}

#if defined(_DEBUG)
	ComPtr<ID3D12InfoQueue> pInfoQueue;
	ThrowCOMIfFailed(m_pDevice->QueryInterface(IID_PPV_ARGS(&pInfoQueue)));
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
		D3D12_MESSAGE_ID_CREATEGRAPHICSPIPELINESTATE_DEPTHSTENCILVIEW_NOT_SET // This warning occurs when setting DSV Format for PSO as DXGI_UNKNOWN for post processing
	};

	D3D12_INFO_QUEUE_FILTER infoQueueFilter = {};
	infoQueueFilter.DenyList.NumSeverities = ARRAYSIZE(severities);
	infoQueueFilter.DenyList.pSeverityList = severities;
	infoQueueFilter.DenyList.NumIDs = ARRAYSIZE(denyIDs);
	infoQueueFilter.DenyList.pIDList = denyIDs;

	ThrowCOMIfFailed(pInfoQueue->PushStorageFilter(&infoQueueFilter));
#endif
}
