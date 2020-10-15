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
		inline static uint64_t	TotalFrameCount = 0;
		inline static uint64_t	FrameCount		= 0;
		inline static double	TimeElapsed		= 0.0;
		inline static double	FPS				= 0.0;
		inline static double	FPMS			= 0.0;
	};

	struct Settings
	{
		inline static bool		VSync = false;
	};

	Renderer(Window* pWindow);

	virtual void OnInitialize() override;
	virtual void OnHandleMouse(int32_t X, int32_t Y, float DeltaTime) override;
	virtual void OnHandleKeyboard(const Keyboard& Keyboard, float DeltaTime) override;
	virtual void OnUpdate(const Time& Time) override;
	virtual void OnRender() override;
	virtual void OnResize(uint32_t Width, uint32_t Height) override;
	virtual void OnDestroy() override;
private:
	void SetScene(Scene Scene);
	void RenderGui();
	
	DXGIManager								m_DXGIManager;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain;

	RenderDevice							m_RenderDevice;
	Gui										m_Gui;
	Scene									m_Scene;
	GpuScene								m_GpuScene;
	RenderGraph								m_RenderGraph;

	CommandContext*							m_pUploadCommandContext;
	RenderContext							m_UploadRenderContext;
};