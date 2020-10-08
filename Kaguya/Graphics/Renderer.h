#pragma once
#include <wrl/client.h>
#include "Core/RenderSystem.h"

#include "DXGIManager.h"
#include "GpuScene.h"
#include "RenderDevice.h"
#include "Gui.h"
#include "RenderGraph.h"

//----------------------------------------------------------------------------------------------------
class Window;
class Time;

//----------------------------------------------------------------------------------------------------
class Renderer : public RenderSystem
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

	Renderer(Window* pWindow);

	void SetScene(Scene* pScene);

	virtual void OnInitialize() override;
	virtual void OnHandleMouse(int X, int Y, float DeltaTime) override;
	virtual void OnHandleKeyboard(const Keyboard& Keyboard, float DeltaTime) override;
	virtual void OnUpdate(const Time& Time) override;
	virtual void OnRender() override;
	virtual void OnResize(uint32_t Width, uint32_t Height) override;
	virtual void OnDestroy() override;
private:
	void RenderGui();
	
	DXGIManager m_DXGIManager;

	// Swapchain resources
	static constexpr DXGI_FORMAT SwapChainBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain;

	std::string AdapterDescription;

	RenderDevice m_RenderDevice;
	Gui m_Gui;
	GpuScene m_GpuScene;
	RenderGraph m_RenderGraph;

	CommandContext* pUploadCommandContext;
	RenderContext UploadRenderContext;
};