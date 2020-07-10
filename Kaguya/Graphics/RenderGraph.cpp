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
	m_NumRenderPasses(0)
{
}

void RenderGraph::Setup()
{
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		m_RenderPasses[i]->Setup(m_RefRenderDevice);
	}
	m_ThreadPool = std::make_unique<ThreadPool>(m_NumRenderPasses);
	m_Futures.resize(m_NumRenderPasses);
}

void RenderGraph::Execute()
{
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		if (!m_RenderPasses[i]->Enabled)
			continue;

		m_Futures[i] = m_ThreadPool->AddWork([this, i]()
		{
			RenderGraphRegistry renderGraphRegistry(m_RefRenderDevice);
			m_RenderPasses[i]->Execute(renderGraphRegistry, m_RenderContexts[i]);
		});
	}
}

void RenderGraph::ExecuteCommandContexts()
{
	m_RefRenderDevice.ExecuteRenderCommandContexts(m_RenderContexts.size(), m_RenderContexts.data());
}

void RenderGraph::ThreadBarrier()
{
	for (auto& future : m_Futures)
	{
		if (future.valid())
			future.wait();
	}
}