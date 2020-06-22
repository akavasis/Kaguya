#pragma once
#include "../Core/EventReceiver.h"

#include "DXGIManager.h"
#include "RenderDevice.h"

class Window;
class Time;

class RenderEngine
{
public:
	RenderEngine(Window& Window);
	~RenderEngine();

	inline const auto& GetAssociatedWindow() const { return m_Window; }

	void Update(const Time& Time);
	void Render();
	void Present();
private:
	Window& m_Window;
	EventReceiver m_EventReceiver;

	// In FLIP swap chains the compositor owns one of your buffers at any given point
	// Thus to run unconstrained (>vsync) frame rates, you need at least 3 buffers
	enum { NumSwapChainBuffers = 3 };
	enum { NodeMask = 0 };
	//enum { NumFramesToBuffer = 3 }; // Not used yet
	enum { NumCascades = 4 };
	static constexpr DXGI_FORMAT SwapChainBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	static constexpr DXGI_FORMAT HDRBufferFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
	static constexpr DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	static constexpr DXGI_FORMAT BRDFLUTFormat = DXGI_FORMAT_R16G16_FLOAT;
	static constexpr DXGI_FORMAT IrradianceFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;
	static constexpr DXGI_FORMAT PrefilterFormat = DXGI_FORMAT_R32G32B32A32_FLOAT;

	DXGIManager m_DXGIManager;
	RenderDevice m_RenderDevice;

	// Swapchain resources
	float m_AspectRatio;
};