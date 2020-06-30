#include "pch.h"
#include "RenderGraph.h"

RenderPassBase::RenderPassBase(RenderPassType RenderPassType)
	: Enabled(true),
	eRenderPassType(RenderPassType)
{
}

RenderPassBase::~RenderPassBase()
{
}

RenderGraph::RenderGraph(RenderDevice& RefRenderDevice)
	: m_RefRenderDevice(RefRenderDevice),
	m_RenderGraphRegistry(RefRenderDevice),
	m_NumRenderPasses(0)
{
}

void RenderGraph::Setup()
{
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		m_RenderPasses[i]->Setup(m_RefRenderDevice);
	}
}

void RenderGraph::Execute()
{
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		m_RenderPasses[i]->Execute(m_RenderGraphRegistry, m_RenderContexts[i]);
	}
}