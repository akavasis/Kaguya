#include "pch.h"
#include "DXGIManager.h"
#include "Core/Window.h"
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

Microsoft::WRL::ComPtr<IDXGISwapChain4> DXGIManager::CreateSwapChain(IUnknown* pUnknown, const Window* pWindow, DXGI_FORMAT Format, UINT BufferCount)
{
	Microsoft::WRL::ComPtr<IDXGISwapChain4> pSwapChain4;
	// For DX11 pUnknown should be ID3D11Device
	// For DX12 pUnknown should be ID3D12CommandQueue
	DXGI_SWAP_CHAIN_DESC1 Desc	= {};
	Desc.Width					= pWindow->GetWindowWidth();
	Desc.Height					= pWindow->GetWindowHeight();
	Desc.Format					= Format;
	Desc.Stereo					= FALSE;
	Desc.SampleDesc				= { 1, 0 };
	Desc.BufferUsage			= DXGI_USAGE_RENDER_TARGET_OUTPUT;
	Desc.BufferCount			= BufferCount;
	Desc.Scaling				= DXGI_SCALING_NONE;
	Desc.SwapEffect				= DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	Desc.AlphaMode				= DXGI_ALPHA_MODE_UNSPECIFIED;
	Desc.Flags					= m_TearingSupport ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	Microsoft::WRL::ComPtr<IDXGISwapChain1> pSwapChain1;
	ThrowCOMIfFailed(m_pDXGIFactory->CreateSwapChainForHwnd(pUnknown, pWindow->GetWindowHandle(), &Desc, nullptr, nullptr, pSwapChain1.ReleaseAndGetAddressOf()));
	ThrowCOMIfFailed(m_pDXGIFactory->MakeWindowAssociation(pWindow->GetWindowHandle(), DXGI_MWA_NO_ALT_ENTER)); // No full screen via alt + enter
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
	DXGI_ADAPTER_DESC3 Desc = {};
	switch (API)
	{
	default:
		return nullptr;

	case API::API_D3D12:
	{
		for (const auto& adapter : m_pAdapters)
		{
			ThrowCOMIfFailed(adapter->GetDesc3(&Desc));
			if ((Desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) == 0)
			{
				hr = ::D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr);
				if (SUCCEEDED(hr))
				{
					m_pQueriedAdapter = adapter;
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

	if (SUCCEEDED(m_pQueriedAdapter->GetDesc3(&Desc)))
	{
		m_QueriedAdapterDesc = Desc;

		std::string desc = UTF16ToUTF8(Desc.Description);
		LOG_INFO("Queried Adapter/GPU: {} \n\t\t\tVendor: {}", desc.data(), ((Desc.VendorId == 0x10DE) ? "Nvidia" : "Unknown"));
	}
	else
	{
		LOG_WARN("Failed To Obtain Queried Adapter / GPU's Description");
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