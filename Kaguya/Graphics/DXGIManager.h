#pragma once
#include <dxgi1_6.h>
#include <vector>

class Window;

enum class API
{
	API_D3D12
};

class DXGIManager
{
public:
	DXGIManager();
	~DXGIManager();

	Microsoft::WRL::ComPtr<IDXGISwapChain4> CreateSwapChain(IUnknown* pUnknown, const Window* pWindow, DXGI_FORMAT Format, UINT BufferCount);
	Microsoft::WRL::ComPtr<IDXGIAdapter4> QueryAdapter(API API);

	DXGI_QUERY_VIDEO_MEMORY_INFO QueryLocalVideoMemoryInfo() const;
	DXGI_QUERY_VIDEO_MEMORY_INFO QueryNonLocalVideoMemoryInfo() const;

	inline auto TearingSupport() const { return m_TearingSupport; }
private:
	Microsoft::WRL::ComPtr<IDXGIFactory6> m_pDXGIFactory;
	std::vector<Microsoft::WRL::ComPtr<IDXGIAdapter4>> m_pAdapters;
	Microsoft::WRL::ComPtr<IDXGIAdapter4> m_pQueriedAdapter;
	bool m_TearingSupport;
};