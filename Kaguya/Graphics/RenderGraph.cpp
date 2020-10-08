#include "pch.h"
#include "RenderGraph.h"

RenderPass::RenderPass(std::string Name, RenderTargetProperties Properties)
	: Enabled(true),
	Refresh(false),
	Name(Name),
	Properties(Properties)
{

}

RenderGraph::RenderGraph(RenderDevice* pRenderDevice)
	: pRenderDevice(pRenderDevice),
	m_ThreadPool(3)
{

}

void RenderGraph::AddRenderPass(RenderPass* pRenderPass)
{
	auto renderPass = std::unique_ptr<RenderPass>(pRenderPass);

	m_ResourceScheduler.m_pCurrentRenderPass = renderPass.get();

	renderPass->OnScheduleResource(&m_ResourceScheduler);

	m_RenderPasses.emplace_back(std::move(renderPass));
	m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(CommandContext::Direct));
	m_RenderPassIDs.emplace_back(typeid(*pRenderPass));
}

void RenderGraph::Initialize()
{
	m_CommandContexts.emplace_back(pRenderDevice->AllocateContext(CommandContext::Direct)); // This command context is for Gui

	m_GpuData = pRenderDevice->CreateBuffer([numRenderPasses = m_RenderPasses.size()](BufferProxy& Proxy)
	{
		Proxy.SetSizeInBytes(numRenderPasses* Math::AlignUp<UINT64>(RenderPass::GpuDataByteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		Proxy.SetStride(Math::AlignUp<UINT64>(RenderPass::GpuDataByteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT));
		Proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});

	CreaterResources();
	CreateResourceViews();
}

void RenderGraph::InitializeScene(GpuScene* pGpuScene)
{
	for (auto& renderPass : m_RenderPasses)
	{
		renderPass->OnInitializeScene(pGpuScene, pRenderDevice);
	}
}

void RenderGraph::RenderGui()
{
	if (ImGui::Begin("Render Pipeline"))
	{
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->OnRenderGui();
		}
	}
	ImGui::End();
}

void RenderGraph::Execute()
{
	bool stateRefresh = false;
	for (auto& renderPass : m_RenderPasses)
	{
		if (!renderPass->Enabled)
			continue;

		if (renderPass->Refresh)
		{
			stateRefresh = true;
			break;
		}
	}

	if (stateRefresh)
	{
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->OnStateRefresh();
		}
	}

	std::vector<std::future<void>> futures(m_RenderPasses.size());
	for (size_t i = 0; i < m_RenderPasses.size(); ++i)
	{
		if (!m_RenderPasses[i]->Enabled)
			continue;

		if constexpr (RenderGraph::Settings::MultiThreaded)
		{
			futures[i] = m_ThreadPool.AddWork([this, i]()
			{
				pRenderDevice->BindUniversalGpuDescriptorHeap(m_CommandContexts[i]);
				auto pBuffer = pRenderDevice->GetBuffer(m_GpuData);
				RenderContext context(i, pBuffer, pRenderDevice, m_CommandContexts[i]);
				m_RenderPasses[i]->OnExecute(context, this);
			});
		}
		else
		{
			pRenderDevice->BindUniversalGpuDescriptorHeap(m_CommandContexts[i]);
			auto pBuffer = pRenderDevice->GetBuffer(m_GpuData);
			RenderContext context(i, pBuffer, pRenderDevice, m_CommandContexts[i]);
			m_RenderPasses[i]->OnExecute(context, this);
		}
	}

	for (auto& future : futures)
	{
		if (future.valid())
			future.wait();
	}
}

void RenderGraph::ExecuteCommandContexts(Gui* pGui)
{
	auto frameIndex = pRenderDevice->FrameIndex;
	auto pDestination = pRenderDevice->GetTexture(pRenderDevice->SwapChainTextures[frameIndex]);
	auto destination = pRenderDevice->GetRenderTargetView(pRenderDevice->SwapChainTextures[frameIndex]);

	pGui->EndFrame(pDestination, destination, m_CommandContexts.back());
	pRenderDevice->ExecuteRenderCommandContexts(m_CommandContexts.size(), m_CommandContexts.data());
}

void RenderGraph::CreaterResources()
{
	for (auto& bufferRequest : m_ResourceScheduler.m_BufferRequests)
	{
		for (const auto& bufferProxy : bufferRequest.second)
		{
			bufferRequest.first->Resources.push_back(pRenderDevice->CreateBuffer([&](BufferProxy& proxy)
			{
				proxy = bufferProxy;
			}));
		}
	}

	for (auto& textureRequest : m_ResourceScheduler.m_TextureRequests)
	{
		for (const auto& textureProxy : textureRequest.second)
		{
			textureRequest.first->Resources.push_back(pRenderDevice->CreateTexture(Resource::Type::Unknown, [&](TextureProxy& proxy)
			{
				proxy = textureProxy;
			}));
		}
	}
}

void RenderGraph::CreateResourceViews()
{
	for (auto& renderPass : m_RenderPasses)
	{
		for (auto handle : renderPass->Resources)
		{
			if (handle.Type == RenderResourceType::Buffer)
				continue;

			pRenderDevice->CreateShaderResourceView(handle);

			auto texture = pRenderDevice->GetTexture(handle);
			if (EnumMaskBitSet(texture->GetBindFlags(), Resource::BindFlags::UnorderedAccess))
			{
				pRenderDevice->CreateUnorderedAccessView(handle);
			}
		}
	}
}