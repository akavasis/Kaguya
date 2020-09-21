#include "pch.h"
#include "RenderGraph.h"

IRenderPass::IRenderPass(RenderPassType Type, RenderTargetProperties Properties)
	: Enabled(true),
	Type(Type),
	Properties(Properties)
{
}

RenderGraph::RenderGraph(RenderDevice* pRenderDevice, GpuScene* pGpuScene)
	: pRenderDevice(pRenderDevice),
	pGpuScene(pGpuScene),
	m_NumRenderPasses(0)
{
}

void RenderGraph::AddRenderPass(IRenderPass* pIRenderPass)
{
	auto pRenderPass = std::unique_ptr<IRenderPass>(pIRenderPass);
	if (!pRenderPass->OnInitialize(pGpuScene, pRenderDevice))
	{
		throw std::logic_error("Failed to initialize render pass");
	}

	m_RenderPasses.emplace_back(std::move(pRenderPass));
	switch (pIRenderPass->Type)
	{
	case RenderPassType::Graphics: m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(CommandContext::Direct)); break;
	case RenderPassType::Compute: m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(CommandContext::Compute)); break;
	case RenderPassType::Copy: m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(CommandContext::Copy)); break;
	}
	m_RenderPassDataIDs.emplace_back(typeid(*pIRenderPass));
	m_NumRenderPasses++;
}

void RenderGraph::Initialize()
{
	m_ThreadPool = std::make_unique<ThreadPool>(m_NumRenderPasses);
	m_Futures.resize(m_NumRenderPasses);
	m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(CommandContext::Direct)); // This command context is for Gui
}

void RenderGraph::Update()
{
	for (auto& renderPass : m_RenderPasses)
	{
		if (!renderPass->Enabled)
			continue;

		renderPass->OnUpdate(pGpuScene, pRenderDevice);
	}
}

void RenderGraph::RenderGui()
{
	for (auto& renderPass : m_RenderPasses)
	{
		renderPass->OnRenderGui();
	}
}

void RenderGraph::Execute()
{
	for (decltype(m_NumRenderPasses) i = 0; i < m_NumRenderPasses; ++i)
	{
		if (!m_RenderPasses[i]->Enabled)
			continue;

		if constexpr (RenderGraph::Settings::MultiThreaded)
		{
			m_Futures[i] = m_ThreadPool->AddWork([this, i]()
			{
				RenderGraphRegistry renderGraphRegistry(pRenderDevice, this);
				pRenderDevice->BindUniversalGpuDescriptorHeap(m_CommandContexts[i]);
				m_RenderPasses[i]->OnExecute(renderGraphRegistry, m_CommandContexts[i]);
			});
		}
		else
		{
			RenderGraphRegistry renderGraphRegistry(pRenderDevice, this);
			pRenderDevice->BindUniversalGpuDescriptorHeap(m_CommandContexts[i]);
			m_RenderPasses[i]->OnExecute(renderGraphRegistry, m_CommandContexts[i]);
		}
	}
}

void RenderGraph::ExecuteCommandContexts(Gui* pGui)
{
	auto frameIndex = pRenderDevice->FrameIndex;
	auto pDestination = pRenderDevice->GetTexture(pRenderDevice->SwapChainTextures[frameIndex]);
	auto destination = pRenderDevice->SwapChainRenderTargetViews[frameIndex];

	pGui->EndFrame(pDestination, destination, m_CommandContexts.back());
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
		renderPass->OnResize(Width, Height, pRenderDevice);
	}
}