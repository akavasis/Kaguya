#pragma once

#include <memory>
#include <atomic>
#include <wil/resource.h>
#include "Core/RenderSystem.h"

#include "RenderDevice.h"
#include "RenderGraph.h"
#include "GpuScene.h"

//----------------------------------------------------------------------------------------------------
class Renderer : public RenderSystem
{
public:
	Renderer();

	void RequestCapture();

protected:
	bool Initialize() override;
	void Update(const Time& Time) override;
	void HandleMouse(bool CursorEnabled, int32_t X, int32_t Y, const Mouse& Mouse, float DeltaTime) override;
	void HandleKeyboard(const Keyboard& Keyboard, float DeltaTime) override;
	void Render() override;
	bool Resize(uint32_t Width, uint32_t Height) override;
	void Destroy() override;

private:
	void SetScene(Scene Scene);
	void RenderGui();

	static DWORD WINAPI AsyncComputeThreadProc(_In_ PVOID pParameter);
	static DWORD WINAPI AsyncCopyThreadProc(_In_ PVOID pParameter);
	
private:
	std::unique_ptr<RenderDevice>			m_pRenderDevice;
	std::unique_ptr<RenderGraph>			m_pRenderGraph;
	RenderContext							m_GraphicsContext;

	Scene									m_Scene;
	std::unique_ptr<GpuScene>				m_pGpuScene;
	INT										InstanceID							= -1; // Temporary, used to debug picking results

	wil::unique_event						BuildAccelerationStructureEvent;
	wil::unique_event						AccelerationStructureCompleteEvent;
	wil::unique_handle						AsyncComputeThread;
	wil::unique_handle						AsyncCopyThread;
	std::atomic<bool>						ExitAsyncQueuesThread				= false;
};