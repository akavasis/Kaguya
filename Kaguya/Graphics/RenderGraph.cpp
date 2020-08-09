#include "pch.h"
#include "RenderGraph.h"

RenderPassBase::RenderPassBase(RenderPassType Type)
	: Enabled(true),
	Type(Type)
{
}

RenderGraph::RenderGraph(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice),
	m_NumRenderPasses(0)
{
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

void RenderGraph::Execute(UINT FrameIndex, Scene& Scene)
{
	m_FrameIndex = FrameIndex;
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		if (!m_RenderPasses[i]->Enabled)
			continue;

		if constexpr (RenderGraph::Settings::MultiThreaded)
		{
			m_Futures[i] = m_ThreadPool->AddWork([this, i, &Scene = Scene]()
			{
				RenderGraphRegistry renderGraphRegistry(pRenderDevice, this);
				pRenderDevice->BindUniversalGpuDescriptorHeap(m_CommandContexts[i]);
				m_RenderPasses[i]->Execute(Scene, renderGraphRegistry, m_CommandContexts[i]);
			});
		}
		else
		{
			RenderGraphRegistry renderGraphRegistry(pRenderDevice, this);
			pRenderDevice->BindUniversalGpuDescriptorHeap(m_CommandContexts[i]);
			m_RenderPasses[i]->Execute(Scene, renderGraphRegistry, m_CommandContexts[i]);
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

void RenderGraph::Resize(UINT Width, UINT Height)
{
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		m_RenderPasses[i]->Resize(Width, Height, pRenderDevice);
	}
}