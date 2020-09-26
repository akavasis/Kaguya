#pragma once
#include <wrl/client.h>
#include "Core/EventReceiver.h"
#include "DXGIManager.h"
#include "Scene/Scene.h"
#include "GpuScene.h"
#include "RenderDevice.h"
#include "Gui.h"
#include "RenderGraph.h"
#include "RendererRegistry.h"
#include "GpuTextureAllocator.h"

class Application;
class Window;
class Time;

class Renderer
{
public:
	struct Statistics
	{
		inline static UINT64 TotalFrameCount = 0;
		inline static UINT64 FrameCount = 0;
		inline static DOUBLE TimeElapsed = 0.0;
		inline static DOUBLE FPS = 0.0;
		inline static DOUBLE FPMS = 0.0;
	};

	struct Settings
	{
		inline static bool VSync = false;
	};

	Renderer(const Application& Application, Window& Window);
	~Renderer();

	void SetScene(Scene* pScene);

	void Update(const Time& Time);
	void RenderGui();
	void Render();
private:
	void Resize(UINT Width, UINT Height);

	const Window* pWindow;
	EventReceiver m_EventReceiver;

	// Swapchain resources
	static constexpr DXGI_FORMAT SwapChainBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	float m_AspectRatio;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain;

	DXGIManager m_DXGIManager;

	RenderDevice m_RenderDevice;
	Gui m_Gui;
	GpuScene m_GpuScene;
	RenderGraph m_RenderGraph;
};