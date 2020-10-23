#pragma once
#include <wrl/client.h>
#include "Core/RenderSystem.h"

#include "DXGIManager.h"
#include "RenderDevice.h"
#include "RenderGraph.h"
#include "GpuScene.h"

//----------------------------------------------------------------------------------------------------
class Window;

//----------------------------------------------------------------------------------------------------
class Renderer : public RenderSystem
{
public:
	Renderer();

protected:
	virtual void Initialize() override;
	virtual void HandleMouse(int32_t X, int32_t Y, float DeltaTime) override;
	virtual void HandleKeyboard(const Keyboard& Keyboard, float DeltaTime) override;
	virtual void Update(const Time& Time) override;
	virtual void Render() override;
	virtual void Resize(uint32_t Width, uint32_t Height) override;
	virtual void Destroy() override;
private:
	void SetScene(Scene Scene);
	void RenderGui();
	
	DXGIManager								m_DXGIManager;
	Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain;

	RenderDevice							m_RenderDevice;
	RenderGraph								m_RenderGraph;
	RenderContext							m_RenderContext; // Used exclusively by the renderer

	Scene									m_Scene;
	GpuScene								m_GpuScene;
};