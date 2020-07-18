#include "pch.h"
#include "RenderGraph.h"

RenderPassBase::RenderPassBase(RenderPassType RenderPassType)
	: Enabled(true),
	eRenderPassType(RenderPassType)
{
}

RenderGraph::RenderGraph(RenderDevice& RefRenderDevice)
	: m_RefRenderDevice(RefRenderDevice),
	m_ExecutionPolicy(Sequenced),
	m_NumRenderPasses(0)
{
}

void RenderGraph::SetExecutionPolicy(ExecutionPolicy ExecutionPolicy)
{
	m_ExecutionPolicy = ExecutionPolicy;
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

void RenderGraph::Execute(Scene& Scene)
{
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		if (!m_RenderPasses[i]->Enabled)
			continue;

		switch (m_ExecutionPolicy)
		{
		case ExecutionPolicy::Sequenced:
		{
			RenderGraphRegistry renderGraphRegistry(m_RefRenderDevice, *this);
			m_RefRenderDevice.BindUniversalGpuDescriptorHeap(m_RenderContexts[i]);
			m_RenderPasses[i]->Execute(Scene, renderGraphRegistry, m_RenderContexts[i]);
		}
		break;

		case ExecutionPolicy::Parallel:
		{
			m_Futures[i] = m_ThreadPool->AddWork([this, i, &Scene = Scene]()
			{
				RenderGraphRegistry renderGraphRegistry(m_RefRenderDevice, *this);
				m_RefRenderDevice.BindUniversalGpuDescriptorHeap(m_RenderContexts[i]);
				m_RenderPasses[i]->Execute(Scene, renderGraphRegistry, m_RenderContexts[i]);
			});
		}
		break;
		}
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