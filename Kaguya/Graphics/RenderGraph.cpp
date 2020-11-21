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
	: SV_pRenderDevice(pRenderDevice)
{

}

RenderGraph::~RenderGraph()
{
	ExitRenderPassThread = true;
	for (size_t i = 0; i < m_RenderPasses.size(); ++i)
	{
		::CloseHandle(m_WorkerExecuteSignals[i]);
		::CloseHandle(m_WorkerCompleteSignals[i]);
		::CloseHandle(m_Threads[i]);
	}
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
	m_WorkerExecuteSignals.resize(m_RenderPasses.size());
	m_WorkerCompleteSignals.resize(m_RenderPasses.size());
	m_ThreadParameters.resize(m_RenderPasses.size());
	m_Threads.resize(m_RenderPasses.size());

	// Create system constants
	m_SystemConstants = SV_pRenderDevice->CreateDeviceBuffer([](DeviceBufferProxy& Proxy)
	{
		constexpr UINT64 AlignedStride = Math::AlignUp<UINT64>(sizeof(HLSL::SystemConstants), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		Proxy.SetSizeInBytes(AlignedStride);
		Proxy.SetStride(AlignedStride);
		Proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
	});

	// Create render pass upload data
	m_GpuData = SV_pRenderDevice->CreateDeviceBuffer([numRenderPasses = m_RenderPasses.size()](DeviceBufferProxy& Proxy)
	{
		constexpr UINT64 AlignedStride = Math::AlignUp<UINT64>(RenderPass::GpuDataByteSize, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);

		Proxy.SetSizeInBytes(numRenderPasses * AlignedStride);
		Proxy.SetStride(AlignedStride);
		Proxy.SetCpuAccess(DeviceBuffer::CpuAccess::Write);
	});

	// Launch our threads
	auto pSystemConstants			= SV_pRenderDevice->GetBuffer(m_SystemConstants);
	auto pGpuData					= SV_pRenderDevice->GetBuffer(m_GpuData);
	for (size_t i = 0; i < m_RenderPasses.size(); ++i)
	{
		m_WorkerExecuteSignals[i]	= ::CreateEvent(NULL, FALSE, FALSE, NULL);
		m_WorkerCompleteSignals[i]	= ::CreateEvent(NULL, FALSE, FALSE, NULL);

		RenderPassThreadProcParameter Parameter;
		Parameter.pRenderGraph		= this;
		Parameter.ThreadID			= i;
		Parameter.ExecuteSignal		= m_WorkerExecuteSignals[i];
		Parameter.CompleteSignal	= m_WorkerCompleteSignals[i];
		Parameter.pRenderPass		= m_RenderPasses[i].get();
		Parameter.pRenderDevice		= SV_pRenderDevice;
		Parameter.pCommandContext	= m_CommandContexts[i];
		Parameter.pSystemConstants	= pSystemConstants;
		Parameter.pGpuData			= pGpuData;

		m_ThreadParameters[i]		= Parameter;
		m_Threads[i]				= ::CreateThread(NULL, 0, RenderGraph::RenderPassThreadProc, reinterpret_cast<LPVOID>(&m_ThreadParameters[i]), 0, nullptr);
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
	if (ImGui::Begin("Render Pipeline"))
	{
		for (auto& renderPass : m_RenderPasses)
		{
			renderPass->OnRenderGui();
		}
	}
	ImGui::End();
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
		SetEvent(m_WorkerExecuteSignals[i]); // Tell each worker to start executing.
	}

	// Fork-join all the events
	WaitForMultipleObjects(m_RenderPasses.size(), m_WorkerCompleteSignals.data(), TRUE, INFINITE);
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

	SetThreadDescription(GetCurrentThread(), UTF8ToUTF16(pRenderPass->Name).data());

	while (!ExitRenderPassThread)
	{
		// Wait for parent thread to tell us to execute
		WaitForSingleObject(ExecuteSignal, INFINITE);

		pRenderDevice->BindGpuDescriptorHeap(pCommandContext);
		pRenderPass->OnExecute(RenderContext(ThreadID, pSystemConstants, pGpuData, pRenderDevice, pCommandContext), pRenderGraph);

		// Tell paraent thread that we are done
		SetEvent(pRenderPassThreadProcParameter->CompleteSignal);
	}

	return EXIT_SUCCESS;
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