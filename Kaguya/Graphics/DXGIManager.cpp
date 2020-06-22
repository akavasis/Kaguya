#include "pch.h"
#include "DXGIManager.h"
#include "../Core/Window.h"
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

DXGIManager::DXGIManager()
	: m_pDXGIFactory(nullptr), m_pQueriedAdapter(nullptr),
	m_LocalVideoMemoryInfo(), m_NonLocalVideoMemoryInfo()
{
	UINT FactoryFlags = 0;
#if defined (_DEBUG)
	FactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
#endif
	ThrowCOMIfFailed(::CreateDXGIFactory2(FactoryFlags, IID_PPV_ARGS(&m_pDXGIFactory)));

	// Enumerate hardware
	{
		// Enumerating iGPU, dGPU, xGPU
		IDXGIAdapter4* pAdapter4 = nullptr;
		for (UINT i = 0; m_pDXGIFactory->EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&pAdapter4)) != DXGI_ERROR_NOT_FOUND; ++i)
		{
			m_pAdapters.push_back(pAdapter4);
		}
	}

	// Check tearing support
	{
		BOOL AllowTearing = FALSE;
		if (FAILED(m_pDXGIFactory->CheckFeatureSupport(
			DXGI_FEATURE_PRESENT_ALLOW_TEARING,
			&AllowTearing, sizeof(AllowTearing))))
		{
			AllowTearing = FALSE;
		}
		m_tearingSupport = AllowTearing == TRUE;
	}
}

DXGIManager::~DXGIManager()
{
	m_pQueriedAdapter = nullptr;
	for (UINT i = 0, size = static_cast<UINT>(m_pAdapters.size()); i < size; ++i)
	{
		m_pAdapters[i]->Release();
	}
	m_pAdapters.clear();
	if (m_pDXGIFactory)
	{
		m_pDXGIFactory->Release();
		m_pDXGIFactory = nullptr;
	}
#if defined(_DEBUG)
	IDXGIDebug* pDXGIDebug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDXGIDebug))))
	{
		pDXGIDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
	}
#endif
}

IDXGISwapChain4* DXGIManager::CreateSwapChain(IUnknown* pUnknown, const Window& Window, DXGI_FORMAT Format, UINT BufferCount)
{
	IDXGISwapChain4* pSwapChain4 = nullptr;
	// For DX11 pUnknown should be ID3D11Device
	// For DX12 pUnknown should be ID3D12CommandQueue
	DXGI_SWAP_CHAIN_DESC1 SwapChainDesc1;
	SwapChainDesc1.Width = Window.GetWindowWidth();
	SwapChainDesc1.Height = Window.GetWindowHeight();
	SwapChainDesc1.Format = Format;
	SwapChainDesc1.Stereo = FALSE;
	SwapChainDesc1.SampleDesc.Count = 1u;
	SwapChainDesc1.SampleDesc.Quality = 0u;
	SwapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	SwapChainDesc1.BufferCount = BufferCount;
	SwapChainDesc1.Scaling = DXGI_SCALING_NONE;
	SwapChainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	SwapChainDesc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	SwapChainDesc1.Flags = m_tearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	IDXGISwapChain1* pSwapChain1 = nullptr;
	ThrowCOMIfFailed(m_pDXGIFactory->CreateSwapChainForHwnd(pUnknown, Window.GetWindowHandle(), &SwapChainDesc1, nullptr, nullptr, &pSwapChain1));
	ThrowCOMIfFailed(m_pDXGIFactory->MakeWindowAssociation(Window.GetWindowHandle(), DXGI_MWA_NO_ALT_ENTER));
	ThrowCOMIfFailed(pSwapChain1->QueryInterface(IID_PPV_ARGS(&pSwapChain4)));
	pSwapChain1->Release();
	return pSwapChain4;
}

IDXGIAdapter4* DXGIManager::QueryAdapter(API API)
{
	if (m_pQueriedAdapter)
		return m_pQueriedAdapter;

	// If this fails fall back to WARP
	HRESULT hr = E_FAIL;
	// The adapter with the largest dedicated video memory is favored.
	DXGI_ADAPTER_DESC3 AdapterDesc3;
	switch (API)
	{
	default:
		return nullptr;

	case API::API_D3D12:
	{
		for (UINT i = 0, size = static_cast<UINT>(m_pAdapters.size()); i < size; ++i)
		{
			ThrowCOMIfFailed(m_pAdapters[i]->GetDesc3(&AdapterDesc3));
			if ((AdapterDesc3.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
			{
				hr = ::D3D12CreateDevice(
					m_pAdapters[i],
					D3D_FEATURE_LEVEL_11_0,
					__uuidof(ID3D12Device), nullptr);
				if (SUCCEEDED(hr))
				{
					m_pQueriedAdapter = m_pAdapters[i];
					break;
				}
				else // Fall back to WARP
				{
					IDXGIAdapter4* pAdapter4 = nullptr;
					ThrowCOMIfFailed(m_pDXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(&pAdapter4)));
					ThrowCOMIfFailed(::D3D12CreateDevice(
						pAdapter4,
						D3D_FEATURE_LEVEL_11_0,
						__uuidof(ID3D12Device), nullptr));
					m_pQueriedAdapter = pAdapter4;
					break;
				}
			}
		}
	}
	break;
	}

	if (SUCCEEDED(m_pQueriedAdapter->GetDesc3(&AdapterDesc3)))
	{
		std::wstring gpuName = AdapterDesc3.Description;
		CORE_INFO("Queried Adapter/GPU: {} \n\t\t\tVendor: {}", std::string(gpuName.begin(), gpuName.end()).data(), ((AdapterDesc3.VendorId == 0x10DE) ? "Nvidia" : "Unknown"));
	}
	else
	{
		CORE_WARN("Failed To Obtain Queried Adapter / GPU's Description");
	}

	QueryLocalVideoMemoryInfo();
	QueryNonLocalVideoMemoryInfo();
	return m_pQueriedAdapter;
}

const DXGI_QUERY_VIDEO_MEMORY_INFO& DXGIManager::QueryLocalVideoMemoryInfo() const
{
	if (m_pQueriedAdapter)
		m_pQueriedAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &m_LocalVideoMemoryInfo);
	return m_LocalVideoMemoryInfo;
}

const DXGI_QUERY_VIDEO_MEMORY_INFO& DXGIManager::QueryNonLocalVideoMemoryInfo() const
{
	if (m_pQueriedAdapter)
		m_pQueriedAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &m_NonLocalVideoMemoryInfo);
	return m_NonLocalVideoMemoryInfo;
}

bool DXGIManager::TearingSupport() const
{
	return m_tearingSupport;
}