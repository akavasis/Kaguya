#pragma once
#include "RenderDevice.h"
#include "RendererRegistry.h"

class Gui
{
public:
	Gui(RenderDevice* pRenderDevice);
	~Gui();

	void BeginFrame();
	void EndFrame(Texture* pDestination, Descriptor DestinationRTV, CommandContext* pCommandContext);
private:
	CBSRUADescriptorHeap DescriptorHeap;
};