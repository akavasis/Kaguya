#pragma once
#include "../RenderResourceHandle.h"

class RenderGraph;
class IRenderPass;

class RenderGraphBuilder
{
public:
	RenderGraphBuilder(RenderGraph* pRenderGraph, IRenderPass* pRenderPass);

	void Read(RenderResourceHandle ResourceHandle);
	void Write(RenderResourceHandle ResourceHandle);
private:
	RenderGraph* m_pRenderGraph;
	IRenderPass* m_pRenderPass;
};