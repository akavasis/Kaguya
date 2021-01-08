#include "pch.h"
#include "RenderGraph.h"

#include "RenderPass.h"

RenderGraph::RenderGraph(RenderDevice* pRenderDevice)
	: SV_pRenderDevice(pRenderDevice)
{
	m_SystemConstants = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render System Constants");
	m_GpuData = pRenderDevice->InitializeRenderResourceHandle(RenderResourceType::Buffer, "Render Graph Pass Data");
}

RenderGraph::~RenderGraph()
{
	ExitRenderPassThread = true;
	for (size_t i = 0; i < m_WorkerExecuteEvents.size(); ++i)
	{
		// Tell each worker to exit.
		m_WorkerExecuteEvents[i].SetEvent();
	}
	WaitForMultipleObjects(m_Threads.size(), m_Threads.data()->addressof(), TRUE, INFINITE);
}

RenderPass* RenderGraph::AddRenderPass(RenderPass* pRenderPass)
{
	auto renderPass = std::unique_ptr<RenderPass>(pRenderPass);

	m_ResourceScheduler.m_pCurrentRenderPass = renderPass.get();

	renderPass->SV_Index = m_RenderPasses.size();
	renderPass->OnInitializePipeline(SV_pRenderDevice);
	renderPass->OnScheduleResource(&m_ResourceScheduler, this);

	m_RenderPasses.emplace_back(std::move(renderPass));
	return m_RenderPasses.back().get();
}

void RenderGraph::Initialize()
{
	m_CommandContexts.resize(NumRenderPass());
	m_WorkerExecuteEvents.resize(NumRenderPass());
	m_WorkerCompleteEvents.resize(NumRenderPass());
	m_ThreadParameters.resize(NumRenderPass());
	m_Threads.resize(NumRenderPass());

	// Create system constants
	SV_pRenderDevice->CreateBuffer(m_SystemConstants, [](BufferProxy& Proxy)
	{
		constexpr UINT64 AlignedStride = Math::AlignUp<UINT64>(sizeof(HLSL::SystemConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		Proxy.SetSizeInBytes(AlignedStride);
		Proxy.SetStride(AlignedStride);
		Proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});

	// Create render pass upload data
	SV_pRenderDevice->CreateBuffer(m_GpuData, [numRenderPasses = m_RenderPasses.size()](BufferProxy& Proxy)
	{
		constexpr UINT64 AlignedStride = Math::AlignUp<UINT64>(RenderPass::GpuDataByteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		Proxy.SetSizeInBytes(numRenderPasses * AlignedStride);
		Proxy.SetStride(AlignedStride);
		Proxy.SetCpuAccess(Buffer::CpuAccess::Write);
	});

	// Launch our threads
	auto pSystemConstants			= SV_pRenderDevice->GetBuffer(m_SystemConstants);
	auto pGpuData					= SV_pRenderDevice->GetBuffer(m_GpuData);
	for (size_t i = 0; i < m_RenderPasses.size(); ++i)
	{
		m_CommandContexts[i] = SV_pRenderDevice->GetGraphicsContext(i + 1);
		m_WorkerExecuteEvents[i].create();
		m_WorkerCompleteEvents[i].create();

		RenderPassThreadProcParameter Parameter = {};
		Parameter.pRenderGraph		= this;
		Parameter.ThreadID			= i;
		Parameter.ExecuteSignal		= m_WorkerExecuteEvents[i].get();
		Parameter.CompleteSignal	= m_WorkerCompleteEvents[i].get();
		Parameter.pRenderPass		= m_RenderPasses[i].get();
		Parameter.pRenderDevice		= SV_pRenderDevice;
		Parameter.pCommandContext	= m_CommandContexts[i];
		Parameter.pSystemConstants	= pSystemConstants;
		Parameter.pGpuData			= pGpuData;

		m_ThreadParameters[i]		= Parameter;
		m_Threads[i]				= wil::unique_handle(::CreateThread(NULL, 0, RenderGraph::RenderPassThreadProc, reinterpret_cast<LPVOID>(&m_ThreadParameters[i]), 0, nullptr));
	}

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
	for (auto& renderPass : m_RenderPasses)
	{
		renderPass->OnRenderGui();
	}
}

void RenderGraph::UpdateSystemConstants(const HLSL::SystemConstants& HostSystemConstants)
{
	auto pSystemConstants = SV_pRenderDevice->GetBuffer(m_SystemConstants);

	pSystemConstants->Map();
	pSystemConstants->Update<HLSL::SystemConstants>(0, HostSystemConstants);
}

void RenderGraph::Execute(bool Refresh)
{
	// Before we begin executing render passes, check to see if any render pass requested for
	// a state request
	for (const auto& renderPass : m_RenderPasses)
	{
		if (!renderPass->Enabled)
			continue;

		if (renderPass->Refresh)
		{
			Refresh = true;
			break;
		}
	}

	if (Refresh)
	{
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->OnStateRefresh();
		}
	}

	for (size_t i = 0; i < m_RenderPasses.size(); ++i)
	{
		// Tell each worker to start executing.
		m_WorkerExecuteEvents[i].SetEvent(); 
	}

	// Fork-join all the events
	WaitForMultipleObjects(m_WorkerCompleteEvents.size(), m_WorkerCompleteEvents.data()->addressof(), TRUE, INFINITE);
}

std::vector<CommandContext*>& RenderGraph::GetCommandContexts()
{
	return m_CommandContexts;
}

DWORD WINAPI RenderGraph::RenderPassThreadProc(_In_ PVOID pParameter)
{
	auto pRenderPassThreadProcParameter = reinterpret_cast<RenderPassThreadProcParameter*>(pParameter);

	auto pRenderGraph		= pRenderPassThreadProcParameter->pRenderGraph;
	auto ThreadID			= pRenderPassThreadProcParameter->ThreadID;
	auto ExecuteSignal		= pRenderPassThreadProcParameter->ExecuteSignal;
	auto CompleteSignal		= pRenderPassThreadProcParameter->CompleteSignal;
	auto pRenderPass		= pRenderPassThreadProcParameter->pRenderPass;
	auto pRenderDevice		= pRenderPassThreadProcParameter->pRenderDevice;
	auto pCommandContext	= pRenderPassThreadProcParameter->pCommandContext;
	auto pSystemConstants	= pRenderPassThreadProcParameter->pSystemConstants;
	auto pGpuData			= pRenderPassThreadProcParameter->pGpuData;
	auto Name				= UTF8ToUTF16(pRenderPass->Name);

	SetThreadDescription(GetCurrentThread(), Name.data());

	while (true)
	{
		// Wait for parent thread to tell us to execute
		WaitForSingleObject(ExecuteSignal, INFINITE);

		if (ExitRenderPassThread)
		{
			break;
		}

		RenderContext RenderContext(ThreadID, pSystemConstants, pGpuData, pRenderDevice, pCommandContext);
		{
			PIXScopedEvent(RenderContext->GetApiHandle(), 0, Name.data());

			pRenderDevice->BindGpuDescriptorHeap(pCommandContext);

			if (!pRenderPass->ExplicitResourceTransition)
			{
				for (auto& resource : pRenderPass->Resources)
				{
					auto pTexture = pRenderDevice->GetTexture(resource);

					if (EnumMaskBitSet(pTexture->GetBindFlags(), Resource::Flags::RenderTarget))
					{
						RenderContext.TransitionBarrier(resource, Resource::State::RenderTarget);
					}

					if (EnumMaskBitSet(pTexture->GetBindFlags(), Resource::Flags::DepthStencil))
					{
						RenderContext.TransitionBarrier(resource, Resource::State::DepthWrite);
					}

					// Transition any UAVs to UnorderedAccess state
					if (EnumMaskBitSet(pTexture->GetBindFlags(), Resource::Flags::UnorderedAccess))
					{
						RenderContext.TransitionBarrier(resource, Resource::State::UnorderedAccess);
					}
				}
			}

			pRenderPass->OnExecute(RenderContext, pRenderGraph);

			if (!pRenderPass->ExplicitResourceTransition)
			{
				for (auto& resource : pRenderPass->Resources)
				{
					auto pTexture = pRenderDevice->GetTexture(resource);

					if (EnumMaskBitSet(pTexture->GetBindFlags(), Resource::Flags::RenderTarget))
					{
						RenderContext.TransitionBarrier(resource, Resource::State::NonPixelShaderResource | Resource::State::PixelShaderResource);
					}

					if (EnumMaskBitSet(pTexture->GetBindFlags(), Resource::Flags::DepthStencil))
					{
						RenderContext.TransitionBarrier(resource, Resource::State::NonPixelShaderResource | Resource::State::PixelShaderResource);
					}

					// Ensure UAV writes are complete
					if (EnumMaskBitSet(pTexture->GetBindFlags(), Resource::Flags::UnorderedAccess))
					{
						RenderContext.UAVBarrier(resource);
					}
				}
			}
		}

		// Tell parent thread that we are done
		SetEvent(CompleteSignal);
	}

	return EXIT_SUCCESS;
}

void RenderGraph::CreaterResources()
{
	for (auto& textureRequest : m_ResourceScheduler.m_TextureRequests)
	{
		for (size_t i = 0; i < textureRequest.second.size(); ++i)
		{
			SV_pRenderDevice->CreateTexture(textureRequest.first->Resources[i], Resource::Type::Unknown, [&](TextureProxy& proxy)
			{
				proxy = textureRequest.second[i];
			});
		}
	}
}

void RenderGraph::CreateResourceViews()
{
	for (auto& renderPass : m_RenderPasses)
	{
		for (auto handle : renderPass->Resources)
		{
			SV_pRenderDevice->CreateShaderResourceView(handle);

			auto pTexture = SV_pRenderDevice->GetTexture(handle);

			if (EnumMaskBitSet(pTexture->GetBindFlags(), Resource::Flags::RenderTarget))
			{
				SV_pRenderDevice->CreateRenderTargetView(handle);
			}

			if (EnumMaskBitSet(pTexture->GetBindFlags(), Resource::Flags::DepthStencil))
			{
				SV_pRenderDevice->CreateDepthStencilView(handle);
			}

			if (EnumMaskBitSet(pTexture->GetBindFlags(), Resource::Flags::UnorderedAccess))
			{
				SV_pRenderDevice->CreateUnorderedAccessView(handle);
			}
		}
	}
}

void ResourceScheduler::AllocateTexture(Resource::Type Type, std::function<void(TextureProxy&)> Configurator)
{
	TextureProxy proxy(Type);
	Configurator(proxy);

	m_TextureRequests[m_pCurrentRenderPass].push_back(proxy);
}

void ResourceScheduler::Read(RenderResourceHandle Resource)
{
	assert(Resource.Type == RenderResourceType::Buffer || Resource.Type == RenderResourceType::Texture);

	m_pCurrentRenderPass->Reads.insert(Resource);
}

void ResourceScheduler::Write(RenderResourceHandle Resource)
{
	assert(Resource.Type == RenderResourceType::Buffer || Resource.Type == RenderResourceType::Texture);

	m_pCurrentRenderPass->Writes.insert(Resource);
}