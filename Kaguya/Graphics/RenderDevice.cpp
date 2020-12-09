#include "pch.h"
#include "RenderDevice.h"

RenderDevice::RenderDevice(IDXGIAdapter4* pAdapter)
	: Device(pAdapter),
	GraphicsQueue(Device.GetApiHandle(), D3D12_COMMAND_LIST_TYPE_DIRECT),
	ComputeQueue(Device.GetApiHandle(), D3D12_COMMAND_LIST_TYPE_COMPUTE),
	CopyQueue(Device.GetApiHandle(), D3D12_COMMAND_LIST_TYPE_COPY),

	BackBufferIndex(0),
	BackBufferHandle{},

	m_NonShaderVisibleCBSRUADescriptorHeap(&Device, NumConstantBufferDescriptors, NumShaderResourceDescriptors, NumUnorderedAccessDescriptors, false),
	m_ShaderVisibleCBSRUADescriptorHeap(&Device, NumConstantBufferDescriptors, NumShaderResourceDescriptors, NumUnorderedAccessDescriptors, true),
	m_SamplerDescriptorHeap(&Device, NumSamplerDescriptors, true),
	m_RenderTargetDescriptorHeap(&Device, NumRenderTargetDescriptors),
	m_DepthStencilDescriptorHeap(&Device, NumDepthStencilDescriptors)
{
	GraphicsFenceValue = ComputeFenceValue = CopyFenceValue = 0;

	ThrowCOMIfFailed(Device.GetApiHandle()->CreateFence(GraphicsFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(GraphicsFence.ReleaseAndGetAddressOf())));
	ThrowCOMIfFailed(Device.GetApiHandle()->CreateFence(ComputeFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(ComputeFence.ReleaseAndGetAddressOf())));
	ThrowCOMIfFailed(Device.GetApiHandle()->CreateFence(CopyFenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(CopyFence.ReleaseAndGetAddressOf())));

	GraphicsFenceCompletionEvent.create();
	ComputeFenceCompletionEvent.create();
	CopyFenceCompletionEvent.create();

	auto HeapIndex = m_ShaderResourceDescriptorIndexPool.Allocate();
	Descriptor ImGuiDescriptor = m_ShaderVisibleCBSRUADescriptorHeap.GetSRDescriptorAt(HeapIndex);

	// Initialize ImGui for d3d12
	ImGui_ImplDX12_Init(Device.GetApiHandle(), 1,
		RenderDevice::SwapChainBufferFormat, m_ShaderVisibleCBSRUADescriptorHeap.GetApiHandle(),
		ImGuiDescriptor.CpuHandle,
		ImGuiDescriptor.GpuHandle);

	for (size_t i = 0; i < NumSwapChainBuffers; ++i)
	{
		BackBufferHandle[i] = InitializeRenderResourceHandle(RenderResourceType::Texture, "SwapChain Buffer[" + std::to_string(i) + "]");
	}
}

RenderDevice::~RenderDevice()
{
	ImGui_ImplDX12_Shutdown();
}

CommandContext* RenderDevice::AllocateContext(CommandContext::Type Type)
{
	m_CommandContexts[Type].push_back(std::make_unique<CommandContext>(Device.GetApiHandle(), Type));
	return m_CommandContexts[Type].back().get();
}

void RenderDevice::BindGpuDescriptorHeap(CommandContext* pCommandContext)
{
	pCommandContext->SetDescriptorHeaps(&m_ShaderVisibleCBSRUADescriptorHeap, &m_SamplerDescriptorHeap);
}

void RenderDevice::ExecuteCommandContexts(CommandContext::Type Type, UINT NumCommandContexts, CommandContext* ppCommandContexts[])
{
	ScopedWriteLock SWL(GlobalResourceStateTrackerRWLock);

	std::vector<ID3D12CommandList*> commandlistsToBeExecuted;
	commandlistsToBeExecuted.reserve(size_t(NumCommandContexts) * 2);
	for (UINT i = 0; i < NumCommandContexts; ++i)
	{
		CommandContext* pCommandContext = ppCommandContexts[i];
		if (pCommandContext->Close(&GlobalResourceStateTracker))
		{
			commandlistsToBeExecuted.push_back(pCommandContext->GetPendingCommandList());
		}
		commandlistsToBeExecuted.push_back(pCommandContext->GetApiHandle());
	}

	CommandQueue& pCommandQueue = GetApiCommandQueue(Type);
	pCommandQueue->ExecuteCommandLists(commandlistsToBeExecuted.size(), commandlistsToBeExecuted.data());
}

void RenderDevice::FlushGraphicsQueue()
{
	UINT64 Value = ++GraphicsFenceValue;
	ThrowCOMIfFailed(GraphicsQueue->Signal(GraphicsFence.Get(), Value));
	ThrowCOMIfFailed(GraphicsFence->SetEventOnCompletion(Value, GraphicsFenceCompletionEvent.get()));
	GraphicsFenceCompletionEvent.wait();
}

void RenderDevice::FlushComputeQueue()
{
	UINT64 Value = ++ComputeFenceValue;
	ThrowCOMIfFailed(ComputeQueue->Signal(ComputeFence.Get(), Value));
	ThrowCOMIfFailed(ComputeFence->SetEventOnCompletion(Value, ComputeFenceCompletionEvent.get()));
	ComputeFenceCompletionEvent.wait();
}

void RenderDevice::FlushCopyQueue()
{
	UINT64 Value = ++CopyFenceValue;
	ThrowCOMIfFailed(CopyQueue->Signal(CopyFence.Get(), Value));
	ThrowCOMIfFailed(CopyFence->SetEventOnCompletion(Value, CopyFenceCompletionEvent.get()));
	CopyFenceCompletionEvent.wait();
}

RenderResourceHandle RenderDevice::InitializeRenderResourceHandle(RenderResourceType Type, const std::string& Name)
{
	switch (Type)
	{
	case RenderResourceType::Buffer:		return m_BufferHandleRegistry.Allocate(Type, Name);						break;
	case RenderResourceType::Texture:		return m_TextureHandleRegistry.Allocate(Type, Name);					break;
	case RenderResourceType::Heap:			return m_HeapHandleRegistry.Allocate(Type, Name);						break;
	case RenderResourceType::RootSignature: return m_RootSignatureHandleRegistry.Allocate(Type, Name);				break;
	case RenderResourceType::GraphicsPSO:	return m_GraphicsPipelineStateHandleRegistry.Allocate(Type, Name);		break;
	case RenderResourceType::ComputePSO:	return m_ComputePipelineStateHandleRegistry.Allocate(Type, Name);		break;
	case RenderResourceType::RaytracingPSO: return m_RaytracingPipelineStateHandleRegistry.Allocate(Type, Name);	break;
	default:								assert(false && "Invalid RenderResourceType");							return {};
	}
}

std::string RenderDevice::GetRenderResourceHandleName(RenderResourceHandle Handle)
{
	switch (Handle.Type)
	{
	case RenderResourceType::Buffer:		return m_BufferHandleRegistry.GetName(Handle);					break;
	case RenderResourceType::Texture:		return m_TextureHandleRegistry.GetName(Handle);					break;
	case RenderResourceType::Heap:			return m_HeapHandleRegistry.GetName(Handle);					break;
	case RenderResourceType::RootSignature: return m_RootSignatureHandleRegistry.GetName(Handle);			break;
	case RenderResourceType::GraphicsPSO:	return m_GraphicsPipelineStateHandleRegistry.GetName(Handle);	break;
	case RenderResourceType::ComputePSO:	return m_ComputePipelineStateHandleRegistry.GetName(Handle);	break;
	case RenderResourceType::RaytracingPSO: return m_RaytracingPipelineStateHandleRegistry.GetName(Handle); break;
	default:																								break;
	}
	return std::string();
}

void RenderDevice::CreateBuffer(RenderResourceHandle Handle, std::function<void(BufferProxy&)> Configurator)
{
	BufferProxy proxy;
	Configurator(proxy);

	auto pBuffer = m_Buffers.CreateResource(Handle, &Device, proxy);
	auto Name = UTF8ToUTF16(GetRenderResourceHandleName(Handle));
	pBuffer->GetApiHandle()->SetName(Name.data());

	// No need to track resources that have constant resource state throughout their lifetime
	if (pBuffer->GetCpuAccess() == Buffer::CpuAccess::None)
		GlobalResourceStateTracker.AddResourceState(pBuffer->GetApiHandle(), GetD3DResourceStates(proxy.InitialState));
}

void RenderDevice::CreateBuffer(RenderResourceHandle Handle, RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(BufferProxy&)> Configurator)
{
	BufferProxy proxy;
	Configurator(proxy);

	const auto pHeap = this->GetHeap(HeapHandle);
	auto pBuffer = m_Buffers.CreateResource(Handle, &Device, pHeap, HeapOffset, proxy);
	auto Name = UTF8ToUTF16(GetRenderResourceHandleName(Handle));
	pBuffer->GetApiHandle()->SetName(Name.data());

	// No need to track resources that have constant resource state throughout their lifetime
	if (pBuffer->GetCpuAccess() == Buffer::CpuAccess::None)
		GlobalResourceStateTracker.AddResourceState(pBuffer->GetApiHandle(), GetD3DResourceStates(proxy.InitialState));
}

void RenderDevice::CreateTexture(RenderResourceHandle Handle, Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, Resource::State InitialState)
{
	auto pTexture = m_Textures.CreateResource(Handle, ExistingResource);
	auto Name = UTF8ToUTF16(GetRenderResourceHandleName(Handle));
	pTexture->GetApiHandle()->SetName(Name.data());

	GlobalResourceStateTracker.AddResourceState(ExistingResource.Get(), GetD3DResourceStates(InitialState));
}

void RenderDevice::CreateTexture(RenderResourceHandle Handle, Resource::Type Type, std::function<void(TextureProxy&)> Configurator)
{
	TextureProxy proxy(Type);
	Configurator(proxy);

	auto pTexture = m_Textures.CreateResource(Handle, &Device, proxy);
	auto Name = UTF8ToUTF16(GetRenderResourceHandleName(Handle));
	pTexture->GetApiHandle()->SetName(Name.data());

	GlobalResourceStateTracker.AddResourceState(pTexture->GetApiHandle(), GetD3DResourceStates(proxy.InitialState));
}

void RenderDevice::CreateTexture(RenderResourceHandle Handle, Resource::Type Type, RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(TextureProxy&)> Configurator)
{
	TextureProxy proxy(Type);
	Configurator(proxy);

	const auto pHeap = this->GetHeap(HeapHandle);
	assert(pHeap->GetType() != Heap::Type::Upload && "Heap cannot be type upload");
	assert(pHeap->GetType() != Heap::Type::Readback && "Heap cannot be type readback");

	auto pTexture = m_Textures.CreateResource(Handle, &Device, pHeap, HeapOffset, proxy);
	auto Name = UTF8ToUTF16(GetRenderResourceHandleName(Handle));
	pTexture->GetApiHandle()->SetName(Name.data());

	GlobalResourceStateTracker.AddResourceState(pTexture->GetApiHandle(), GetD3DResourceStates(proxy.InitialState));
}

void RenderDevice::CreateHeap(RenderResourceHandle Handle, std::function<void(HeapProxy&)> Configurator)
{
	HeapProxy proxy;
	Configurator(proxy);

	auto pHeap = m_Heaps.CreateResource(Handle, &Device, proxy);
	auto Name = UTF8ToUTF16(GetRenderResourceHandleName(Handle));
	pHeap->GetApiHandle()->SetName(Name.data());
}

void RenderDevice::CreateRootSignature(RenderResourceHandle Handle, std::function<void(RootSignatureProxy&)> Configurator, bool AddShaderLayoutRootParameters)
{
	RootSignatureProxy proxy;
	Configurator(proxy);
	if (AddShaderLayoutRootParameters)
		AddShaderLayoutRootParameter(proxy);

	auto pRootSignature = m_RootSignatures.CreateResource(Handle, &Device, proxy);
	auto Name = UTF8ToUTF16(GetRenderResourceHandleName(Handle));
	pRootSignature->GetApiHandle()->SetName(Name.data());
}

void RenderDevice::CreateGraphicsPipelineState(RenderResourceHandle Handle, std::function<void(GraphicsPipelineStateProxy&)> Configurator)
{
	GraphicsPipelineStateProxy proxy;
	Configurator(proxy);

	auto pGraphicsPSO = m_GraphicsPipelineStates.CreateResource(Handle, &Device, proxy);
	auto Name = UTF8ToUTF16(GetRenderResourceHandleName(Handle));
	pGraphicsPSO->GetApiHandle()->SetName(Name.data());
}

void RenderDevice::CreateComputePipelineState(RenderResourceHandle Handle, std::function<void(ComputePipelineStateProxy&)> Configurator)
{
	ComputePipelineStateProxy proxy;
	Configurator(proxy);

	auto pComputePSO = m_ComputePipelineStates.CreateResource(Handle, &Device, proxy);
	auto Name = UTF8ToUTF16(GetRenderResourceHandleName(Handle));
	pComputePSO->GetApiHandle()->SetName(Name.data());
}

void RenderDevice::CreateRaytracingPipelineState(RenderResourceHandle Handle, std::function<void(RaytracingPipelineStateProxy&)> Configurator)
{
	RaytracingPipelineStateProxy proxy;
	Configurator(proxy);

	auto pRaytracingPSO = m_RaytracingPipelineStates.CreateResource(Handle, &Device, proxy);
	auto Name = UTF8ToUTF16(GetRenderResourceHandleName(Handle));
	pRaytracingPSO->GetApiHandle()->SetName(Name.data());
}

void RenderDevice::Destroy(RenderResourceHandle Handle)
{
	if (!Handle)
		return;

	switch (Handle.Type)
	{
	case RenderResourceType::Buffer:
	{
		auto pBuffer = m_Buffers.GetResource(Handle);
		if (pBuffer)
		{
			// Remove from GRST
			GlobalResourceStateTracker.RemoveResourceState(pBuffer->GetApiHandle());

			// Remove all the resource views
			if (auto iter = m_RenderBuffers.find(Handle);
				iter != m_RenderBuffers.end())
			{
				if (iter->second.ShaderResourceView.IsValid())
				{
					m_ShaderResourceDescriptorIndexPool.Free(iter->second.ShaderResourceView.HeapIndex);
				}
				if (iter->second.UnorderedAccessView.IsValid())
				{
					m_UnorderedAccessDescriptorIndexPool.Free(iter->second.UnorderedAccessView.HeapIndex);
				}
				m_RenderBuffers.erase(iter);
			}

			// Finally, remove the actual resource
			m_Buffers.Destroy(Handle);
		}
	}
	break;
	case RenderResourceType::Texture:
	{
		auto pTexture = m_Textures.GetResource(Handle);
		if (pTexture)
		{
			// Remove from GRST
			GlobalResourceStateTracker.RemoveResourceState(pTexture->GetApiHandle());

			// Remove all the resource views
			if (auto iter = m_RenderTextures.find(Handle);
				iter != m_RenderTextures.end())
			{
				for (const auto& srv : iter->second.ShaderResourceViews)
				{
					m_ShaderResourceDescriptorIndexPool.Free(srv.second.HeapIndex);
				}
				for (const auto& uav : iter->second.UnorderedAccessViews)
				{
					m_UnorderedAccessDescriptorIndexPool.Free(uav.second.HeapIndex);
				}
				for (const auto& rtv : iter->second.RenderTargetViews)
				{
					m_RenderTargetDescriptorIndexPool.Free(rtv.second.HeapIndex);
				}
				for (const auto& dsv : iter->second.DepthStencilViews)
				{
					m_RenderTargetDescriptorIndexPool.Free(dsv.second.HeapIndex);
				}
				m_RenderTextures.erase(iter);
			}

			// Finally, remove the actual resource
			m_Textures.Destroy(Handle);
		}
	}
	break;
	case RenderResourceType::Heap:			m_Heaps.Destroy(Handle);						break;
	case RenderResourceType::RootSignature: m_RootSignatures.Destroy(Handle);				break;
	case RenderResourceType::GraphicsPSO:	m_GraphicsPipelineStates.Destroy(Handle);		break;
	case RenderResourceType::ComputePSO:	m_ComputePipelineStates.Destroy(Handle);		break;
	case RenderResourceType::RaytracingPSO: m_RaytracingPipelineStates.Destroy(Handle);	break;
	}
}

void RenderDevice::CreateShaderResourceView(RenderResourceHandle Handle, std::optional<UINT> MostDetailedMip, std::optional<UINT> MipLevels)
{
	switch (Handle.Type)
	{
	case RenderResourceType::Buffer:
	{
		if (auto iter = m_RenderBuffers.find(Handle);
			iter != m_RenderBuffers.end())
		{
			auto& renderBuffer = iter->second;

			// Re-use descriptor
			if (renderBuffer.ShaderResourceView.IsValid())
			{
				m_ShaderVisibleCBSRUADescriptorHeap.AssignSRDescriptor(renderBuffer.ShaderResourceView.HeapIndex,
					renderBuffer.pBuffer);
			}
			// Create new one
			else
			{
				size_t HeapIndex = m_ShaderResourceDescriptorIndexPool.Allocate();
				m_ShaderVisibleCBSRUADescriptorHeap.AssignSRDescriptor(HeapIndex, renderBuffer.pBuffer);
				renderBuffer.ShaderResourceView = m_ShaderVisibleCBSRUADescriptorHeap.GetUADescriptorAt(HeapIndex);
			}
		}
		else
		{
			// First-time request
			Buffer* pBuffer = GetBuffer(Handle);
			assert(pBuffer && "Could not find buffer given the handle");

			size_t HeapIndex = m_ShaderResourceDescriptorIndexPool.Allocate();
			m_ShaderVisibleCBSRUADescriptorHeap.AssignSRDescriptor(HeapIndex, pBuffer);

			RenderBuffer renderBuffer;
			renderBuffer.pBuffer = pBuffer;
			renderBuffer.ShaderResourceView = m_ShaderVisibleCBSRUADescriptorHeap.GetSRDescriptorAt(HeapIndex);

			m_RenderBuffers[Handle] = renderBuffer;
		}
	}
	break;

	case RenderResourceType::Texture:
	{
		if (auto iter = m_RenderTextures.find(Handle);
			iter != m_RenderTextures.end())
		{
			auto& renderTexture = iter->second;

			UINT64 HashValue = CBSRUADescriptorHeap::GetHashValue(CBSRUADescriptorHeap::GetShaderResourceViewDesc(renderTexture.pTexture, MostDetailedMip, MipLevels));
			if (auto srvIter = renderTexture.ShaderResourceViews.find(HashValue);
				srvIter != renderTexture.ShaderResourceViews.end())
			{
				// Re-use descriptor
				m_ShaderVisibleCBSRUADescriptorHeap.AssignSRDescriptor(srvIter->second.HeapIndex, renderTexture.pTexture, MostDetailedMip, MipLevels);
			}
			else
			{
				// Add a new descriptor
				size_t HeapIndex = m_ShaderResourceDescriptorIndexPool.Allocate();
				UINT64 HashValue = m_ShaderVisibleCBSRUADescriptorHeap.AssignSRDescriptor(HeapIndex, renderTexture.pTexture, MostDetailedMip, MipLevels);

				renderTexture.ShaderResourceViews[HashValue] = m_ShaderVisibleCBSRUADescriptorHeap.GetSRDescriptorAt(HeapIndex);
			}
		}
		else
		{
			// First-time request
			Texture* pTexture = GetTexture(Handle);
			assert(pTexture && "Could not find texture given the handle");

			size_t HeapIndex = m_ShaderResourceDescriptorIndexPool.Allocate();
			UINT64 HashValue = m_ShaderVisibleCBSRUADescriptorHeap.AssignSRDescriptor(HeapIndex, pTexture, MostDetailedMip, MipLevels);

			RenderTexture renderTexture;
			renderTexture.pTexture = pTexture;
			renderTexture.ShaderResourceViews[HashValue] = m_ShaderVisibleCBSRUADescriptorHeap.GetSRDescriptorAt(HeapIndex);

			m_RenderTextures[Handle] = renderTexture;
		}
	}
	break;
	}
}

void RenderDevice::CreateUnorderedAccessView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice)
{
	switch (Handle.Type)
	{
	case RenderResourceType::Buffer:
	{
		if (auto iter = m_RenderBuffers.find(Handle);
			iter != m_RenderBuffers.end())
		{
			auto& renderBuffer = iter->second;

			// Re-use descriptor
			if (renderBuffer.UnorderedAccessView.IsValid())
			{
				m_ShaderVisibleCBSRUADescriptorHeap.AssignUADescriptor(renderBuffer.UnorderedAccessView.HeapIndex,
					renderBuffer.pBuffer);
			}
			// Create new one
			else
			{
				size_t HeapIndex = m_UnorderedAccessDescriptorIndexPool.Allocate();
				m_ShaderVisibleCBSRUADescriptorHeap.AssignUADescriptor(HeapIndex, renderBuffer.pBuffer);
				renderBuffer.UnorderedAccessView = m_ShaderVisibleCBSRUADescriptorHeap.GetUADescriptorAt(HeapIndex);
			}
		}
		else
		{
			// First-time request
			Buffer* pBuffer = GetBuffer(Handle);
			assert(pBuffer && "Could not find buffer given the handle");

			size_t HeapIndex = m_UnorderedAccessDescriptorIndexPool.Allocate();
			m_ShaderVisibleCBSRUADescriptorHeap.AssignUADescriptor(HeapIndex, pBuffer);

			RenderBuffer renderBuffer;
			renderBuffer.pBuffer = pBuffer;
			renderBuffer.UnorderedAccessView = m_ShaderVisibleCBSRUADescriptorHeap.GetUADescriptorAt(HeapIndex);

			m_RenderBuffers[Handle] = renderBuffer;
		}
	}
	break;

	case RenderResourceType::Texture:
	{
		if (auto iter = m_RenderTextures.find(Handle);
			iter != m_RenderTextures.end())
		{
			auto& renderTexture = iter->second;

			UINT64 HashValue = CBSRUADescriptorHeap::GetHashValue(CBSRUADescriptorHeap::GetUnorderedAccessViewDesc(renderTexture.pTexture, ArraySlice, MipSlice));
			if (auto uavIter = renderTexture.UnorderedAccessViews.find(HashValue);
				uavIter != renderTexture.UnorderedAccessViews.end())
			{
				// Re-use descriptor
				m_ShaderVisibleCBSRUADescriptorHeap.AssignUADescriptor(uavIter->second.HeapIndex, renderTexture.pTexture, ArraySlice, MipSlice);
			}
			else
			{
				// Add a new descriptor
				size_t HeapIndex = m_UnorderedAccessDescriptorIndexPool.Allocate();
				UINT64 HashValue = m_ShaderVisibleCBSRUADescriptorHeap.AssignUADescriptor(HeapIndex, renderTexture.pTexture, ArraySlice, MipSlice);

				renderTexture.UnorderedAccessViews[HashValue] = m_ShaderVisibleCBSRUADescriptorHeap.GetUADescriptorAt(HeapIndex);
			}
		}
		else
		{
			// First-time request
			Texture* pTexture = GetTexture(Handle);
			assert(pTexture && "Could not find texture given the handle");

			size_t HeapIndex = m_UnorderedAccessDescriptorIndexPool.Allocate();
			UINT64 HashValue = m_ShaderVisibleCBSRUADescriptorHeap.AssignUADescriptor(HeapIndex, pTexture, ArraySlice, MipSlice);

			RenderTexture renderTexture;
			renderTexture.pTexture = pTexture;
			renderTexture.UnorderedAccessViews[HashValue] = m_ShaderVisibleCBSRUADescriptorHeap.GetUADescriptorAt(HeapIndex);

			m_RenderTextures[Handle] = renderTexture;
		}
	}
	break;
	}
}

void RenderDevice::CreateRenderTargetView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize)
{
	if (auto iter = m_RenderTextures.find(Handle);
		iter != m_RenderTextures.end())
	{
		auto& renderTexture = iter->second;

		UINT64 HashValue = RenderTargetDescriptorHeap::GetHashValue(RenderTargetDescriptorHeap::GetRenderTargetViewDesc(renderTexture.pTexture, ArraySlice, MipSlice, ArraySize));
		if (auto rtvIter = renderTexture.RenderTargetViews.find(HashValue);
			rtvIter != renderTexture.RenderTargetViews.end())
		{
			// Re-use descriptor
			auto entry = m_RenderTargetDescriptorHeap[rtvIter->second.HeapIndex];
			entry.ArraySlice = ArraySlice;
			entry.MipSlice = MipSlice;
			entry.ArraySize = ArraySize;
			entry = renderTexture.pTexture;
		}
		else
		{
			// Add a new descriptor
			size_t HeapIndex = m_RenderTargetDescriptorIndexPool.Allocate();
			auto entry = m_RenderTargetDescriptorHeap[HeapIndex];
			entry.ArraySlice = ArraySlice;
			entry.MipSlice = MipSlice;
			entry.ArraySize = ArraySize;
			entry = renderTexture.pTexture;

			UINT64 HashValue = entry.GetHashValue();
			renderTexture.RenderTargetViews[HashValue] = m_RenderTargetDescriptorHeap.GetDescriptorAt(HeapIndex);
		}
	}
	else
	{
		// First-time request
		Texture* pTexture = GetTexture(Handle);
		assert(pTexture && "Could not find texture given the handle");

		size_t HeapIndex = m_RenderTargetDescriptorIndexPool.Allocate();
		auto entry = m_RenderTargetDescriptorHeap[HeapIndex];
		entry.ArraySlice = ArraySlice;
		entry.MipSlice = MipSlice;
		entry.ArraySize = ArraySize;
		entry = pTexture;

		UINT64 HashValue = entry.GetHashValue();

		RenderTexture renderTexture;
		renderTexture.pTexture = pTexture;
		renderTexture.RenderTargetViews[HashValue] = m_RenderTargetDescriptorHeap.GetDescriptorAt(HeapIndex);

		m_RenderTextures[Handle] = renderTexture;
	}
}

void RenderDevice::CreateDepthStencilView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize)
{
	if (auto iter = m_RenderTextures.find(Handle);
		iter != m_RenderTextures.end())
	{
		auto& renderTexture = iter->second;

		UINT64 HashValue = DepthStencilDescriptorHeap::GetHashValue(DepthStencilDescriptorHeap::GetDepthStencilViewDesc(renderTexture.pTexture, ArraySlice, MipSlice, ArraySize));
		if (auto rtvIter = renderTexture.DepthStencilViews.find(HashValue);
			rtvIter != renderTexture.DepthStencilViews.end())
		{
			// Re-use descriptor
			auto entry = m_DepthStencilDescriptorHeap[rtvIter->second.HeapIndex];
			entry.ArraySlice = ArraySlice;
			entry.MipSlice = MipSlice;
			entry.ArraySize = ArraySize;
			entry = renderTexture.pTexture;
		}
		else
		{
			// Add a new descriptor
			size_t HeapIndex = m_DepthStencilDescriptorIndexPool.Allocate();
			auto entry = m_DepthStencilDescriptorHeap[HeapIndex];
			entry.ArraySlice = ArraySlice;
			entry.MipSlice = MipSlice;
			entry.ArraySize = ArraySize;
			entry = renderTexture.pTexture;

			UINT64 HashValue = entry.GetHashValue();
			renderTexture.DepthStencilViews[HashValue] = m_DepthStencilDescriptorHeap.GetDescriptorAt(HeapIndex);
		}
	}
	else
	{
		// First-time request
		Texture* pTexture = GetTexture(Handle);
		assert(pTexture && "Could not find texture given the handle");

		size_t HeapIndex = m_DepthStencilDescriptorIndexPool.Allocate();
		auto entry = m_DepthStencilDescriptorHeap[HeapIndex];
		entry.ArraySlice = ArraySlice;
		entry.MipSlice = MipSlice;
		entry.ArraySize = ArraySize;
		entry = pTexture;

		UINT64 HashValue = entry.GetHashValue();

		RenderTexture renderTexture;
		renderTexture.pTexture = pTexture;
		renderTexture.DepthStencilViews[HashValue] = m_DepthStencilDescriptorHeap.GetDescriptorAt(HeapIndex);

		m_RenderTextures[Handle] = renderTexture;
	}
}

Descriptor RenderDevice::GetShaderResourceView(RenderResourceHandle Handle, std::optional<UINT> MostDetailedMip /*= {}*/, std::optional<UINT> MipLevels /*= {}*/) const
{
	switch (Handle.Type)
	{
	case RenderResourceType::Buffer:
	{
		if (auto iter = m_RenderBuffers.find(Handle);
			iter != m_RenderBuffers.end())
		{
			auto& renderBuffer = iter->second;

			if (renderBuffer.ShaderResourceView.IsValid())
				return renderBuffer.ShaderResourceView;
		}
	}
	break;

	case RenderResourceType::Texture:
	{
		if (auto iter = m_RenderTextures.find(Handle);
			iter != m_RenderTextures.end())
		{
			auto& renderTexture = iter->second;

			UINT64 HashValue = CBSRUADescriptorHeap::GetHashValue(CBSRUADescriptorHeap::GetShaderResourceViewDesc(renderTexture.pTexture, MostDetailedMip, MipLevels));
			if (auto srvIter = renderTexture.ShaderResourceViews.find(HashValue);
				srvIter != renderTexture.ShaderResourceViews.end())
				return srvIter->second;
		}
	}
	break;
	}
	return Descriptor();
}

Descriptor RenderDevice::GetUnorderedAccessView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice /*= {}*/, std::optional<UINT> MipSlice /*= {}*/) const
{
	switch (Handle.Type)
	{
	case RenderResourceType::Buffer:
	{
		if (auto iter = m_RenderBuffers.find(Handle);
			iter != m_RenderBuffers.end())
		{
			auto& renderBuffer = iter->second;

			if (renderBuffer.UnorderedAccessView.IsValid())
				return renderBuffer.UnorderedAccessView;
		}
	}
	break;

	case RenderResourceType::Texture:
	{
		if (auto iter = m_RenderTextures.find(Handle);
			iter != m_RenderTextures.end())
		{
			auto& renderTexture = iter->second;

			UINT64 HashValue = CBSRUADescriptorHeap::GetHashValue(CBSRUADescriptorHeap::GetUnorderedAccessViewDesc(renderTexture.pTexture, ArraySlice, MipSlice));
			if (auto uavIter = renderTexture.UnorderedAccessViews.find(HashValue);
				uavIter != renderTexture.UnorderedAccessViews.end())
				return uavIter->second;
		}
	}
	break;
	}
	return Descriptor();
}

Descriptor RenderDevice::GetRenderTargetView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice /*= {}*/, std::optional<UINT> MipSlice /*= {}*/, std::optional<UINT> ArraySize /*= {}*/) const
{
	if (auto iter = m_RenderTextures.find(Handle);
		iter != m_RenderTextures.end())
	{
		auto& renderTexture = iter->second;

		UINT64 HashValue = RenderTargetDescriptorHeap::GetHashValue(RenderTargetDescriptorHeap::GetRenderTargetViewDesc(renderTexture.pTexture, ArraySlice, MipSlice, ArraySize));
		if (auto rtvIter = renderTexture.RenderTargetViews.find(HashValue);
			rtvIter != renderTexture.RenderTargetViews.end())
		{
			return rtvIter->second;
		}
	}

	return Descriptor();
}

Descriptor RenderDevice::GetDepthStencilView(RenderResourceHandle Handle, std::optional<UINT> ArraySlice /*= {}*/, std::optional<UINT> MipSlice /*= {}*/, std::optional<UINT> ArraySize /*= {}*/) const
{
	if (auto iter = m_RenderTextures.find(Handle);
		iter != m_RenderTextures.end())
	{
		auto& renderTexture = iter->second;

		UINT64 HashValue = DepthStencilDescriptorHeap::GetHashValue(DepthStencilDescriptorHeap::GetDepthStencilViewDesc(renderTexture.pTexture, ArraySlice, MipSlice, ArraySize));
		if (auto dsvIter = renderTexture.DepthStencilViews.find(HashValue);
			dsvIter != renderTexture.DepthStencilViews.end())
		{
			return dsvIter->second;
		}
	}

	return Descriptor();
}

CommandQueue& RenderDevice::GetApiCommandQueue(CommandContext::Type Type)
{
	switch (Type)
	{
	case CommandContext::Type::Graphics:	return GraphicsQueue;
	case CommandContext::Type::Compute:		return ComputeQueue;
	case CommandContext::Type::Copy:		return CopyQueue;
	default:								return GraphicsQueue;
	}
}

void RenderDevice::AddShaderLayoutRootParameter(RootSignatureProxy& RootSignatureProxy)
{
	RootSignatureProxy.AddRootCBVParameter(RootCBV(0, 100)); // g_SystemConstants
	RootSignatureProxy.AddRootCBVParameter(RootCBV(1, 100)); // g_RenderPassData

	/* Descriptor Tables */

	// ShaderResource
	RootDescriptorTable ShaderResourceDescriptorTable;
	{
		constexpr D3D12_DESCRIPTOR_RANGE_FLAGS Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 100, Flags, 0)); // g_Texture2DTable
		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 101, Flags, 0)); // g_Texture2DUINT4Table
		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 102, Flags, 0)); // g_Texture2DArrayTable
		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 103, Flags, 0)); // g_TextureCubeTable
		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 104, Flags, 0)); // g_ByteAddressBufferTable
	}
	RootSignatureProxy.AddRootDescriptorTableParameter(ShaderResourceDescriptorTable);

	// UnorderedAccess
	RootDescriptorTable UnorderedAccessDescriptorTable;
	{
		constexpr D3D12_DESCRIPTOR_RANGE_FLAGS Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

		UnorderedAccessDescriptorTable.AddDescriptorRange(DescriptorRange::Type::UAV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 100, Flags, 0)); // g_RWTexture2DTable
		UnorderedAccessDescriptorTable.AddDescriptorRange(DescriptorRange::Type::UAV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 101, Flags, 0)); // g_RWTexture2DArrayTable
	}
	RootSignatureProxy.AddRootDescriptorTableParameter(UnorderedAccessDescriptorTable);

	// Sampler
	RootDescriptorTable SamplerDescriptorTable;
	{
		constexpr D3D12_DESCRIPTOR_RANGE_FLAGS Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

		SamplerDescriptorTable.AddDescriptorRange(DescriptorRange::Type::Sampler, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 100, Flags, 0)); // g_SamplerTable
	}
	RootSignatureProxy.AddRootDescriptorTableParameter(SamplerDescriptorTable);
}