#include "pch.h"
#include "RenderGraph.h"

RenderPass::RenderPass(std::string Name, RenderTargetProperties Properties)
	: Enabled(true),
	Refresh(false),
	Name(std::move(Name)),
	Properties(Properties)
{

}

RenderGraph::RenderGraph(RenderDevice* pRenderDevice)
	: SV_pRenderDevice(pRenderDevice),
	m_ThreadPool(3)
{

}

void RenderGraph::AddRenderPass(RenderPass* pRenderPass)
{
	auto renderPass = std::unique_ptr<RenderPass>(pRenderPass);

	m_ResourceScheduler.m_pCurrentRenderPass = renderPass.get();

	renderPass->OnInitializePipeline(SV_pRenderDevice);
	renderPass->OnScheduleResource(&m_ResourceScheduler);

	m_RenderPasses.emplace_back(std::move(renderPass));
	m_CommandContexts.emplace_back(SV_pRenderDevice->AllocateContext(CommandContext::Direct));
	m_RenderPassIDs.emplace_back(typeid(*pRenderPass));
}

void RenderGraph::Initialize()
{
	m_Futures.resize(m_RenderPasses.size());

	m_SystemConstants = SV_pRenderDevice->CreateDeviceBuffer([](DeviceBufferProxy& Proxy)
	{
		constexpr UINT64 AlignedStride = Math::AlignUp<UINT64>(sizeof(HostSystemConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		Proxy.SetSizeInBytes(AlignedStride);
		Proxy.SetStride(AlignedStride);
		Proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
	});

	m_GpuData = SV_pRenderDevice->CreateDeviceBuffer([numRenderPasses = m_RenderPasses.size()](DeviceBufferProxy& Proxy)
	{
		constexpr UINT64 AlignedStride = Math::AlignUp<UINT64>(RenderPass::GpuDataByteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		Proxy.SetSizeInBytes(numRenderPasses * AlignedStride);
		Proxy.SetStride(AlignedStride);
		Proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
	});

	CreaterResources();
	CreateResourceViews();
}

void RenderGraph::InitializeScene(GpuScene* pGpuScene)
{
	for (auto& renderPass : m_RenderPasses)
	{
		renderPass->OnInitializeScene(pGpuScene, SV_pRenderDevice);
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

void RenderGraph::UpdateSystemConstants(const HostSystemConstants& HostSystemConstants)
{
	auto pSystemConstants = SV_pRenderDevice->GetBuffer(m_SystemConstants);

	pSystemConstants->Map();
	pSystemConstants->Update<::HostSystemConstants>(0, HostSystemConstants);
}

void RenderGraph::Execute()
{
	bool stateRefresh = false;
	for (const auto& renderPass : m_RenderPasses)
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

	auto pSystemConstants = SV_pRenderDevice->GetBuffer(m_SystemConstants);
	auto pBuffer = SV_pRenderDevice->GetBuffer(m_GpuData);
	for (size_t i = 0; i < m_RenderPasses.size(); ++i)
	{
		if (!m_RenderPasses[i]->Enabled)
			continue;

		if constexpr (RenderGraph::Settings::MultiThreaded)
		{
			m_Futures[i] = m_ThreadPool.AddWork([this, i, pSystemConstants, pBuffer]()
			{
				SV_pRenderDevice->BindGpuDescriptorHeap(m_CommandContexts[i]);
				m_RenderPasses[i]->OnExecute(RenderContext(i, pSystemConstants, pBuffer, SV_pRenderDevice, m_CommandContexts[i]), this);
			});
		}
		else
		{
			SV_pRenderDevice->BindGpuDescriptorHeap(m_CommandContexts[i]);
			m_RenderPasses[i]->OnExecute(RenderContext(i, pSystemConstants, pBuffer, SV_pRenderDevice, m_CommandContexts[i]), this);
		}
	}

	for (auto& future : m_Futures)
	{
		if (future.valid())
			future.wait();
	}
}

void RenderGraph::ExecuteCommandContexts(RenderContext& RendererRenderContext)
{
	std::vector<CommandContext*> CommandContexts(m_CommandContexts.size() + 1);
	CommandContexts[0] = RendererRenderContext.GetCommandContext();
	for (size_t i = 1; i < CommandContexts.size(); ++i)
	{
		CommandContexts[i] = m_CommandContexts[i - 1];
	}

	SV_pRenderDevice->ExecuteRenderCommandContexts(CommandContexts.size(), CommandContexts.data());
}

void RenderGraph::CreaterResources()
{
	for (auto& bufferRequest : m_ResourceScheduler.m_BufferRequests)
	{
		for (const auto& bufferProxy : bufferRequest.second)
		{
			bufferRequest.first->Resources.push_back(SV_pRenderDevice->CreateDeviceBuffer([&](DeviceBufferProxy& proxy)
			{
				proxy = bufferProxy;
			}));
		}
	}

	for (auto& textureRequest : m_ResourceScheduler.m_TextureRequests)
	{
		for (const auto& textureProxy : textureRequest.second)
		{
			textureRequest.first->Resources.push_back(SV_pRenderDevice->CreateDeviceTexture(DeviceResource::Type::Unknown, [&](DeviceTextureProxy& proxy)
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
			if (handle.Type == RenderResourceType::DeviceBuffer)
				continue;

			SV_pRenderDevice->CreateShaderResourceView(handle);

			auto texture = SV_pRenderDevice->GetTexture(handle);
			if (EnumMaskBitSet(texture->GetBindFlags(), DeviceResource::BindFlags::UnorderedAccess))
			{
				SV_pRenderDevice->CreateUnorderedAccessView(handle);
			}

			if (EnumMaskBitSet(texture->GetBindFlags(), DeviceResource::BindFlags::RenderTarget))
			{
				SV_pRenderDevice->CreateRenderTargetView(handle);
			}

			if (EnumMaskBitSet(texture->GetBindFlags(), DeviceResource::BindFlags::DepthStencil))
			{
				SV_pRenderDevice->CreateDepthStencilView(handle);
			}
		}
	}
}