#include "pch.h"
#include "DXGIManager.h"
#include "../Core/Window.h"
#if defined(_DEBUG)
#include <dxgidebug.h>
#endif
#include <codecvt>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "dxguid.lib")

DXGIManager::DXGIManager()
	: m_pQueriedAdapter(nullptr)
{
	UINT FactoryFlags = 0;
#if defined (_DEBUG)
	FactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	ThrowCOMIfFailed(::CreateDXGIFactory2(FactoryFlags, IID_PPV_ARGS(m_pDXGIFactory.ReleaseAndGetAddressOf())));

	// Enumerate hardware
	// Enumerating iGPU, dGPU, xGPU
	Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter4;
	UINT i = 0;
	while (m_pDXGIFactory->EnumAdapterByGpuPreference(i++, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(pAdapter4.ReleaseAndGetAddressOf())) != DXGI_ERROR_NOT_FOUND)
	{
		m_pAdapters.push_back(std::move(pAdapter4));
	}

	// Check tearing support
	BOOL AllowTearing = FALSE;
	if (FAILED(m_pDXGIFactory->CheckFeatureSupport(
		DXGI_FEATURE_PRESENT_ALLOW_TEARING,
		&AllowTearing, sizeof(AllowTearing))))
	{
		AllowTearing = FALSE;
	}
	m_TearingSupport = AllowTearing == TRUE;
}

DXGIManager::~DXGIManager()
{
	m_pQueriedAdapter = nullptr;
	m_pAdapters.clear();
	m_pDXGIFactory = nullptr;
#if defined(_DEBUG)
	Microsoft::WRL::ComPtr<IDXGIDebug> pDXGIDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&pDXGIDebug))))
	{
		pDXGIDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_IGNORE_INTERNAL);
	}
#endif
}

Microsoft::WRL::ComPtr<IDXGISwapChain4> DXGIManager::CreateSwapChain(IUnknown* pUnknown, const Window& Window, DXGI_FORMAT Format, UINT BufferCount)
{
	Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain4;
	// For DX11 pUnknown should be ID3D11Device
	// For DX12 pUnknown should be ID3D12CommandQueue
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc1;
	swapChainDesc1.Width = Window.GetWindowWidth();
	swapChainDesc1.Height = Window.GetWindowHeight();
	swapChainDesc1.Format = Format;
	swapChainDesc1.Stereo = FALSE;
	swapChainDesc1.SampleDesc.Count = 1u;
	swapChainDesc1.SampleDesc.Quality = 0u;
	swapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc1.BufferCount = BufferCount;
	swapChainDesc1.Scaling = DXGI_SCALING_NONE;
	swapChainDesc1.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc1.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc1.Flags = m_TearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	Microsoft::WRL::ComPtr<IDXGISwapChain1> pSwapChain1;
	ThrowCOMIfFailed(m_pDXGIFactory->CreateSwapChainForHwnd(pUnknown, Window.GetWindowHandle(), &swapChainDesc1, nullptr, nullptr, pSwapChain1.ReleaseAndGetAddressOf()));
	ThrowCOMIfFailed(m_pDXGIFactory->MakeWindowAssociation(Window.GetWindowHandle(), DXGI_MWA_NO_ALT_ENTER)); // No full screen via alt + enter
	ThrowCOMIfFailed(pSwapChain1->QueryInterface(IID_PPV_ARGS(pSwapChain4.ReleaseAndGetAddressOf())));
	return pSwapChain4;
}

Microsoft::WRL::ComPtr<IDXGIAdapter4> DXGIManager::QueryAdapter(API API)
{
	if (m_pQueriedAdapter)
		return m_pQueriedAdapter;

	// If this fails fall back to WARP
	HRESULT hr = E_FAIL;
	// The adapter with the largest dedicated video memory is favored.
	DXGI_ADAPTER_DESC3 adapterDesc3;
	switch (API)
	{
	default:
		return nullptr;

	case API::API_D3D12:
	{
		for (UINT i = 0, size = static_cast<UINT>(m_pAdapters.size()); i < size; ++i)
		{
			ThrowCOMIfFailed(m_pAdapters[i]->GetDesc3(&adapterDesc3));
			if ((adapterDesc3.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
			{
				hr = ::D3D12CreateDevice(m_pAdapters[i].Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr);
				if (SUCCEEDED(hr))
				{
					m_pQueriedAdapter = m_pAdapters[i];
					break;
				}
				else // Fall back to WARP
				{
					Microsoft::WRL::ComPtr<IDXGIAdapter4> pAdapter4;
					ThrowCOMIfFailed(m_pDXGIFactory->EnumWarpAdapter(IID_PPV_ARGS(pAdapter4.ReleaseAndGetAddressOf())));
					ThrowCOMIfFailed(::D3D12CreateDevice(pAdapter4.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr));
					m_pQueriedAdapter = pAdapter4;
					break;
				}
			}
		}
	}
	break;
	}

	if (SUCCEEDED(m_pQueriedAdapter->GetDesc3(&adapterDesc3)))
	{
		m_QueriedAdapterDesc = adapterDesc3;

		std::string desc = UTF16ToUTF8(adapterDesc3.Description);
		CORE_INFO("Queried Adapter/GPU: {} \n\t\t\tVendor: {}", desc.data(), ((adapterDesc3.VendorId == 0x10DE) ? "Nvidia" : "Unknown"));
	}
	else
	{
		CORE_WARN("Failed To Obtain Queried Adapter / GPU's Description");
	}

	return m_pQueriedAdapter;
}

DXGI_QUERY_VIDEO_MEMORY_INFO DXGIManager::QueryLocalVideoMemoryInfo() const
{
	DXGI_QUERY_VIDEO_MEMORY_INFO localVideoMemoryInfo = {};
	if (m_pQueriedAdapter)
		m_pQueriedAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &localVideoMemoryInfo);
	return localVideoMemoryInfo;
}

DXGI_QUERY_VIDEO_MEMORY_INFO DXGIManager::QueryNonLocalVideoMemoryInfo() const
{
	DXGI_QUERY_VIDEO_MEMORY_INFO nonLocalVideoMemoryInfo = {};
	if (m_pQueriedAdapter)
		m_pQueriedAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_NON_LOCAL, &nonLocalVideoMemoryInfo);
	return nonLocalVideoMemoryInfo;
}