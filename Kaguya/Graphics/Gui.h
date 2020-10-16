#pragma once
#include "RenderDevice.h"
#include "RendererRegistry.h"

class Gui
{
public:
	Gui(RenderDevice* pRenderDevice);
	~Gui();

	void BeginFrame();
	void EndFrame(DeviceTexture* pDestination, Descriptor DestinationRTV, CommandContext* pCommandContext);
private:
	CBSRUADescriptorHeap DescriptorHeap;
};