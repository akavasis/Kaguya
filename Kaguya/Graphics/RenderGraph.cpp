#include "pch.h"
#include "RenderGraph.h"

IRenderPass::IRenderPass(RenderPassType Type, RenderTargetProperties Properties)
	: Enabled(true),
	Type(Type),
	Properties(Properties)
{
}

RenderGraph::RenderGraph(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice),
	m_NumRenderPasses(0)
{
}

void RenderGraph::AddRenderPass(IRenderPass* pIRenderPass)
{
	m_RenderPasses.emplace_back(std::unique_ptr<IRenderPass>(pIRenderPass));
	switch (pIRenderPass->Type)
	{
	case RenderPassType::Graphics: m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(CommandContext::Direct)); break;
	case RenderPassType::Compute: m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(CommandContext::Compute)); break;
	case RenderPassType::Copy: m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(CommandContext::Copy)); break;
	}
	m_RenderPassDataIDs.emplace_back(typeid(*pIRenderPass));
	m_NumRenderPasses++;
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

void RenderGraph::Update()
{
	for (auto& renderPass : m_RenderPasses)
	{
		if (!renderPass->Enabled)
			continue;

		renderPass->Update();
	}
}

void RenderGraph::RenderGui()
{
	for (auto& renderPass : m_RenderPasses)
	{
		if (!renderPass->Enabled)
			continue;

		renderPass->RenderGui();
	}
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
	for (auto& renderPass : m_RenderPasses)
	{
		renderPass->Resize(Width, Height, pRenderDevice);
	}
}