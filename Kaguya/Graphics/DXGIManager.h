#pragma once
#include <dxgi1_6.h>
#include <vector>

class Window;

typedef enum class API
{
	API_D3D12
} API;

class DXGIManager
{
public:
	DXGIManager();
	~DXGIManager();

	IDXGISwapChain4* CreateSwapChain(IUnknown* pUnknown, const Window& Window, DXGI_FORMAT Format, UINT BufferCount);
	IDXGIAdapter4* QueryAdapter(API API);

	const DXGI_QUERY_VIDEO_MEMORY_INFO& QueryLocalVideoMemoryInfo() const;
	const DXGI_QUERY_VIDEO_MEMORY_INFO& QueryNonLocalVideoMemoryInfo() const;

	bool TearingSupport() const;
private:
	IDXGIFactory7* m_pDXGIFactory;
	std::vector<IDXGIAdapter4*> m_pAdapters;
	IDXGIAdapter4* m_pQueriedAdapter;

	mutable DXGI_QUERY_VIDEO_MEMORY_INFO m_LocalVideoMemoryInfo;
	mutable DXGI_QUERY_VIDEO_MEMORY_INFO m_NonLocalVideoMemoryInfo;

	bool m_tearingSupport;
};