#pragma once
#include <wrl/client.h>
#include <wil/resource.h>
#include <atomic>
#include "Core/RenderSystem.h"

#include "RenderDevice.h"
#include "RenderGraph.h"
#include "GpuScene.h"

//----------------------------------------------------------------------------------------------------
class Renderer : public RenderSystem
{
public:
	Renderer();

protected:
	bool Initialize() override;
	void Update(const Time& Time) override;
	void HandleMouse(int32_t X, int32_t Y, float DeltaTime) override;
	void HandleKeyboard(const Keyboard& Keyboard, float DeltaTime) override;
	void Render() override;
	bool Resize(uint32_t Width, uint32_t Height) override;
	void Destroy() override;
private:
	void SetScene(Scene Scene);
	void RenderGui();

	static DWORD WINAPI AsyncComputeThreadProc(_In_ PVOID pParameter);
	static DWORD WINAPI AsyncCopyThreadProc(_In_ PVOID pParameter);
	
	RenderDevice*							m_pRenderDevice						= nullptr;
	RenderGraph*							m_pRenderGraph						= nullptr;
	RenderContext							m_GraphicsContext;

	Scene									m_Scene;
	GpuScene*								m_pGpuScene							= nullptr;

	wil::unique_event						BuildAccelerationStructureEvent;
	wil::unique_event						AccelerationStructureCompleteEvent;
	wil::unique_handle						AsyncComputeThread;
	wil::unique_handle						AsyncCopyThread;
	std::atomic<bool>						ExitAsyncQueuesThread				= false;
};