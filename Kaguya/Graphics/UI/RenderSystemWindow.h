#pragma once
#include "UIWindow.h"

class RenderSystem;

class RenderSystemWindow : public UIWindow
{
public:
	RenderSystemWindow()
		: UIWindow(false)
	{
		pRenderSystem = nullptr;
	}

	void SetContext(RenderSystem* pRenderSystem)
	{
		this->pRenderSystem = pRenderSystem;
	}

	void RenderGui();
private:
	RenderSystem* pRenderSystem;
};