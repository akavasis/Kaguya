#pragma once

class RenderGraph;
class IRenderPass;

#include "RenderResourceHandle.h"

class RenderGraphBuilder
{
public:
	RenderGraphBuilder(RenderGraph* pRenderGraph, IRenderPass* pRenderPass);

	void Read(RenderResourceHandle ResourceHandle);
	void Write(RenderResourceHandle ResourceHandle);
private:
	RenderGraph* m_pRenderGraph;
	IRenderPass* m_pRenderPass;
private:
};