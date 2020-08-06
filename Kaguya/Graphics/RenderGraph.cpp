#include "pch.h"
#include "RenderGraph.h"

RenderPassBase::RenderPassBase(RenderPassType Type)
	: Enabled(true),
	Type(Type)
{
}

RenderGraph::RenderGraph(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice),
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
		m_RenderPasses[i]->Setup(pRenderDevice);
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
			RenderGraphRegistry renderGraphRegistry(pRenderDevice, this);
			pRenderDevice->BindUniversalGpuDescriptorHeap(m_CommandContexts[i]);
			m_RenderPasses[i]->Execute(Scene, renderGraphRegistry, m_CommandContexts[i]);
		}
		break;

		case ExecutionPolicy::Parallel:
		{
			m_Futures[i] = m_ThreadPool->AddWork([this, i, &Scene = Scene]()
			{
				RenderGraphRegistry renderGraphRegistry(pRenderDevice, this);
				pRenderDevice->BindUniversalGpuDescriptorHeap(m_CommandContexts[i]);
				m_RenderPasses[i]->Execute(Scene, renderGraphRegistry, m_CommandContexts[i]);
			});
		}
		break;
		}
	}
}

void RenderGraph::ExecuteCommandContexts()
{
	pRenderDevice->ExecuteRenderCommandContexts(m_CommandContexts.size(), m_CommandContexts.data());
}

void RenderGraph::ThreadBarrier()
{
	for (auto& future : m_Futures)
	{
		if (future.valid())
			future.wait();
	}
}

void RenderGraph::Resize()
{
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		m_RenderPasses[i]->Resize(pRenderDevice);
	}
}