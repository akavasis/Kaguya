#include "pch.h"
#include "RenderDevice.h"

RenderDevice::RenderDevice(IDXGIAdapter4* pAdapter)
	: Device(pAdapter),
	GraphicsQueue(&Device, D3D12_COMMAND_LIST_TYPE_DIRECT),
	ComputeQueue(&Device, D3D12_COMMAND_LIST_TYPE_COMPUTE),
	CopyQueue(&Device, D3D12_COMMAND_LIST_TYPE_COPY),
	FrameIndex(0),
	SwapChainTextures{},

	m_ImGuiDescriptorHeap(&Device, 0, 1, 0, true),
	m_NonShaderVisibleCBSRUADescriptorHeap(&Device, NumConstantBufferDescriptors, NumShaderResourceDescriptors, NumUnorderedAccessDescriptors, false),
	m_ShaderVisibleCBSRUADescriptorHeap(&Device, NumConstantBufferDescriptors, NumShaderResourceDescriptors, NumUnorderedAccessDescriptors, true),
	m_SamplerDescriptorHeap(&Device, NumSamplerDescriptors, true),
	m_RenderTargetDescriptorHeap(&Device, NumRenderTargetDescriptors),
	m_DepthStencilDescriptorHeap(&Device, NumDepthStencilDescriptors)
{
	// Initialize ImGui for d3d12
	ImGui_ImplDX12_Init(Device.GetApiHandle(), 1,
		RenderDevice::SwapChainBufferFormat, m_ImGuiDescriptorHeap.GetD3DDescriptorHeap(),
		m_ImGuiDescriptorHeap.GetD3DDescriptorHeap()->GetCPUDescriptorHandleForHeapStart(),
		m_ImGuiDescriptorHeap.GetD3DDescriptorHeap()->GetGPUDescriptorHandleForHeapStart());

	for (size_t i = 0; i < NumSwapChainBuffers; ++i)
	{
		SwapChainTextures[i] = InitializeRenderResourceHandle(RenderResourceType::Texture, "SwapChain Buffer[" + std::to_string(i) + "]");
	}
}

RenderDevice::~RenderDevice()
{
	ImGui_ImplDX12_Shutdown();
}

CommandContext* RenderDevice::AllocateContext(CommandContext::Type Type)
{
	CommandQueue* pCommandQueue = nullptr;
	switch (Type)
	{
	case CommandContext::Type::Direct:	pCommandQueue = &GraphicsQueue;								break;
	case CommandContext::Type::Compute: pCommandQueue = &ComputeQueue;								break;
	case CommandContext::Type::Copy:	pCommandQueue = &CopyQueue;									break;
	default:							assert(pCommandQueue && "Unsupported command list type");	break;
	}
	m_CommandContexts[Type].push_back(std::make_unique<CommandContext>(&Device, pCommandQueue, Type));
	return m_CommandContexts[Type].back().get();
}

void RenderDevice::BindGpuDescriptorHeap(CommandContext* pCommandContext)
{
	pCommandContext->SetDescriptorHeaps(&m_ShaderVisibleCBSRUADescriptorHeap, &m_SamplerDescriptorHeap);
}

void RenderDevice::ExecuteCommandContexts(CommandContext::Type Type, UINT NumCommandContexts, CommandContext* ppCommandContexts[])
{
	std::vector<ID3D12CommandList*> commandlistsToBeExecuted;
	commandlistsToBeExecuted.reserve(size_t(NumCommandContexts) * 2);
	for (UINT i = 0; i < NumCommandContexts; ++i)
	{
		CommandContext* pCommandContext = ppCommandContexts[i];
		if (pCommandContext->Close(GlobalResourceStateTracker))
		{
			commandlistsToBeExecuted.push_back(pCommandContext->m_pPendingCommandList.Get());
		}
		commandlistsToBeExecuted.push_back(pCommandContext->m_pCommandList.Get());
	}

	CommandQueue* pCommandQueue = nullptr;
	switch (Type)
	{
	case CommandContext::Type::Direct:	pCommandQueue = &GraphicsQueue;								break;
	case CommandContext::Type::Compute: pCommandQueue = &ComputeQueue;								break;
	case CommandContext::Type::Copy:	pCommandQueue = &CopyQueue;									break;
	default:							assert(pCommandQueue && "Unsupported command list type");	break;
	}

	pCommandQueue->GetD3DCommandQueue()->ExecuteCommandLists(commandlistsToBeExecuted.size(), commandlistsToBeExecuted.data());
	UINT64 fenceValue = pCommandQueue->Signal();
	for (UINT i = 0; i < NumCommandContexts; ++i)
	{
		CommandContext* pCommandContext = ppCommandContexts[i];
		pCommandContext->RequestNewAllocator(fenceValue);
		pCommandContext->Reset();
	}
	pCommandQueue->Flush();
}

Shader RenderDevice::CompileShader(Shader::Type Type, const std::filesystem::path& Path, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	return ShaderCompiler.CompileShader(Type, Path.c_str(), pEntryPoint, ShaderDefines);
}

Library RenderDevice::CompileLibrary(const std::filesystem::path& Path)
{
	return ShaderCompiler.CompileLibrary(Path.c_str());
}

RenderResourceHandle RenderDevice::InitializeRenderResourceHandle(RenderResourceType Type, const std::string& Name)
{
	RenderResourceHandle Handle = {};
	Handle.Type = Type;
	switch (Type)
	{
	case RenderResourceType::Buffer:		Handle.Data = m_BufferHandleRegistry.AddNewHandle(Name); break;
	case RenderResourceType::Texture:		Handle.Data = m_TextureHandleRegistry.AddNewHandle(Name); break;
	case RenderResourceType::Heap:			Handle.Data = m_HeapHandleRegistry.AddNewHandle(Name); break;
	case RenderResourceType::RootSignature: Handle.Data = m_RootSignatureHandleRegistry.AddNewHandle(Name); break;
	case RenderResourceType::GraphicsPSO:	Handle.Data = m_GraphicsPipelineStateHandleRegistry.AddNewHandle(Name); break;
	case RenderResourceType::ComputePSO:	Handle.Data = m_ComputePipelineStateHandleRegistry.AddNewHandle(Name); break;
	case RenderResourceType::RaytracingPSO: Handle.Data = m_RaytracingPipelineStateHandleRegistry.AddNewHandle(Name); break;
	default: break;
	}
	return Handle;
}

std::string RenderDevice::GetRenderResourceHandleName(RenderResourceHandle RenderResourceHandle)
{
	switch (RenderResourceHandle.Type)
	{
	case RenderResourceType::Buffer:		return m_BufferHandleRegistry.GetName(RenderResourceHandle.Data); break;
	case RenderResourceType::Texture:		return m_TextureHandleRegistry.GetName(RenderResourceHandle.Data); break;
	case RenderResourceType::Heap:			return m_HeapHandleRegistry.GetName(RenderResourceHandle.Data); break;
	case RenderResourceType::RootSignature: return m_RootSignatureHandleRegistry.GetName(RenderResourceHandle.Data); break;
	case RenderResourceType::GraphicsPSO:	return m_GraphicsPipelineStateHandleRegistry.GetName(RenderResourceHandle.Data); break;
	case RenderResourceType::ComputePSO:	return m_ComputePipelineStateHandleRegistry.GetName(RenderResourceHandle.Data); break;
	case RenderResourceType::RaytracingPSO: return m_RaytracingPipelineStateHandleRegistry.GetName(RenderResourceHandle.Data); break;
	default: break;
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

	// No need to track resources that have preinitialized states
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

	// No need to track resources that have preinitialized states
	if (pBuffer->GetCpuAccess() == Buffer::CpuAccess::None)
		GlobalResourceStateTracker.AddResourceState(pBuffer->GetApiHandle(), GetD3DResourceStates(proxy.InitialState));
}

void RenderDevice::CreateDeviceTexture(RenderResourceHandle Handle, Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, Resource::State InitialState)
{
	auto pTexture = m_Textures.CreateResource(Handle, ExistingResource);
	auto Name = UTF8ToUTF16(GetRenderResourceHandleName(Handle));
	pTexture->GetApiHandle()->SetName(Name.data());

	GlobalResourceStateTracker.AddResourceState(ExistingResource.Get(), GetD3DResourceStates(InitialState));
}

void RenderDevice::CreateDeviceTexture(RenderResourceHandle Handle, Resource::Type Type, std::function<void(TextureProxy&)> Configurator)
{
	TextureProxy proxy(Type);
	Configurator(proxy);

	auto pTexture = m_Textures.CreateResource(Handle, &Device, proxy);
	auto Name = UTF8ToUTF16(GetRenderResourceHandleName(Handle));
	pTexture->GetApiHandle()->SetName(Name.data());

	GlobalResourceStateTracker.AddResourceState(pTexture->GetApiHandle(), GetD3DResourceStates(proxy.InitialState));
}

void RenderDevice::CreateDeviceTexture(RenderResourceHandle Handle, Resource::Type Type, RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(TextureProxy&)> Configurator)
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

void RenderDevice::Destroy(RenderResourceHandle RenderResourceHandle)
{
	if (!RenderResourceHandle)
		return;

	switch (RenderResourceHandle.Type)
	{
	case RenderResourceType::Buffer:
	{
		auto pBuffer = m_Buffers.GetResource(RenderResourceHandle);
		if (pBuffer)
		{
			// Remove from GRST
			GlobalResourceStateTracker.RemoveResourceState(pBuffer->GetApiHandle());

			// Remove all the resource views
			if (auto iter = m_RenderBuffers.find(RenderResourceHandle);
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
			m_Buffers.Destroy(RenderResourceHandle);
		}
	}
	break;
	case RenderResourceType::Texture:
	{
		auto pTexture = m_Textures.GetResource(RenderResourceHandle);
		if (pTexture)
		{
			// Remove from GRST
			GlobalResourceStateTracker.RemoveResourceState(pTexture->GetApiHandle());

			// Remove all the resource views
			if (auto iter = m_RenderTextures.find(RenderResourceHandle);
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
			m_Textures.Destroy(RenderResourceHandle);
		}
	}
	break;
	case RenderResourceType::Heap: m_Heaps.Destroy(RenderResourceHandle); break;
	case RenderResourceType::RootSignature: m_RootSignatures.Destroy(RenderResourceHandle); break;
	case RenderResourceType::GraphicsPSO: m_GraphicsPipelineStates.Destroy(RenderResourceHandle); break;
	case RenderResourceType::ComputePSO: m_ComputePipelineStates.Destroy(RenderResourceHandle); break;
	case RenderResourceType::RaytracingPSO: m_RaytracingPipelineStates.Destroy(RenderResourceHandle); break;
	}
}

void RenderDevice::CreateShaderResourceView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> MostDetailedMip, std::optional<UINT> MipLevels)
{
	switch (RenderResourceHandle.Type)
	{
	case RenderResourceType::Buffer:
	{
		if (auto iter = m_RenderBuffers.find(RenderResourceHandle);
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
			Buffer* pBuffer = GetBuffer(RenderResourceHandle);
			assert(pBuffer && "Could not find buffer given the handle");

			size_t HeapIndex = m_ShaderResourceDescriptorIndexPool.Allocate();
			m_ShaderVisibleCBSRUADescriptorHeap.AssignSRDescriptor(HeapIndex, pBuffer);

			RenderBuffer renderBuffer;
			renderBuffer.pBuffer = pBuffer;
			renderBuffer.ShaderResourceView = m_ShaderVisibleCBSRUADescriptorHeap.GetSRDescriptorAt(HeapIndex);

			m_RenderBuffers[RenderResourceHandle] = renderBuffer;
		}
	}
	break;

	case RenderResourceType::Texture:
	{
		if (auto iter = m_RenderTextures.find(RenderResourceHandle);
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
			Texture* pTexture = GetTexture(RenderResourceHandle);
			assert(pTexture && "Could not find texture given the handle");

			size_t HeapIndex = m_ShaderResourceDescriptorIndexPool.Allocate();
			UINT64 HashValue = m_ShaderVisibleCBSRUADescriptorHeap.AssignSRDescriptor(HeapIndex, pTexture, MostDetailedMip, MipLevels);

			RenderTexture renderTexture;
			renderTexture.pTexture = pTexture;
			renderTexture.ShaderResourceViews[HashValue] = m_ShaderVisibleCBSRUADescriptorHeap.GetSRDescriptorAt(HeapIndex);

			m_RenderTextures[RenderResourceHandle] = renderTexture;
		}
	}
	break;
	}
}

void RenderDevice::CreateUnorderedAccessView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice)
{
	switch (RenderResourceHandle.Type)
	{
	case RenderResourceType::Buffer:
	{
		if (auto iter = m_RenderBuffers.find(RenderResourceHandle);
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
			Buffer* pBuffer = GetBuffer(RenderResourceHandle);
			assert(pBuffer && "Could not find buffer given the handle");

			size_t HeapIndex = m_UnorderedAccessDescriptorIndexPool.Allocate();
			m_ShaderVisibleCBSRUADescriptorHeap.AssignUADescriptor(HeapIndex, pBuffer);

			RenderBuffer renderBuffer;
			renderBuffer.pBuffer = pBuffer;
			renderBuffer.UnorderedAccessView = m_ShaderVisibleCBSRUADescriptorHeap.GetUADescriptorAt(HeapIndex);

			m_RenderBuffers[RenderResourceHandle] = renderBuffer;
		}
	}
	break;

	case RenderResourceType::Texture:
	{
		if (auto iter = m_RenderTextures.find(RenderResourceHandle);
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
			Texture* pTexture = GetTexture(RenderResourceHandle);
			assert(pTexture && "Could not find texture given the handle");

			size_t HeapIndex = m_UnorderedAccessDescriptorIndexPool.Allocate();
			UINT64 HashValue = m_ShaderVisibleCBSRUADescriptorHeap.AssignUADescriptor(HeapIndex, pTexture, ArraySlice, MipSlice);

			RenderTexture renderTexture;
			renderTexture.pTexture = pTexture;
			renderTexture.UnorderedAccessViews[HashValue] = m_ShaderVisibleCBSRUADescriptorHeap.GetUADescriptorAt(HeapIndex);

			m_RenderTextures[RenderResourceHandle] = renderTexture;
		}
	}
	break;
	}
}

void RenderDevice::CreateRenderTargetView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize)
{
	if (auto iter = m_RenderTextures.find(RenderResourceHandle);
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
		Texture* pTexture = GetTexture(RenderResourceHandle);
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

		m_RenderTextures[RenderResourceHandle] = renderTexture;
	}
}

void RenderDevice::CreateDepthStencilView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice, std::optional<UINT> MipSlice, std::optional<UINT> ArraySize)
{
	if (auto iter = m_RenderTextures.find(RenderResourceHandle);
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
		Texture* pTexture = GetTexture(RenderResourceHandle);
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

		m_RenderTextures[RenderResourceHandle] = renderTexture;
	}
}

Descriptor RenderDevice::GetShaderResourceView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> MostDetailedMip /*= {}*/, std::optional<UINT> MipLevels /*= {}*/) const
{
	switch (RenderResourceHandle.Type)
	{
	case RenderResourceType::Buffer:
	{
		if (auto iter = m_RenderBuffers.find(RenderResourceHandle);
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
		if (auto iter = m_RenderTextures.find(RenderResourceHandle);
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

Descriptor RenderDevice::GetUnorderedAccessView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice /*= {}*/, std::optional<UINT> MipSlice /*= {}*/) const
{
	switch (RenderResourceHandle.Type)
	{
	case RenderResourceType::Buffer:
	{
		if (auto iter = m_RenderBuffers.find(RenderResourceHandle);
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
		if (auto iter = m_RenderTextures.find(RenderResourceHandle);
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

Descriptor RenderDevice::GetRenderTargetView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice /*= {}*/, std::optional<UINT> MipSlice /*= {}*/, std::optional<UINT> ArraySize /*= {}*/) const
{
	if (auto iter = m_RenderTextures.find(RenderResourceHandle);
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

Descriptor RenderDevice::GetDepthStencilView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice /*= {}*/, std::optional<UINT> MipSlice /*= {}*/, std::optional<UINT> ArraySize /*= {}*/) const
{
	if (auto iter = m_RenderTextures.find(RenderResourceHandle);
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

void RenderDevice::AddShaderLayoutRootParameter(RootSignatureProxy& RootSignatureProxy)
{
	// SystemConstants
	RootSignatureProxy.AddRootCBVParameter(RootCBV(0, 100));

	// RenderPassDataCB
	RootSignatureProxy.AddRootCBVParameter(RootCBV(1, 100));

	/* Descriptor Tables */

	// ShaderResource
	RootDescriptorTable ShaderResourceDescriptorTable;
	{
		constexpr D3D12_DESCRIPTOR_RANGE_FLAGS Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 100, Flags, 0)); // Texture2DTable
		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 101, Flags, 0)); // Texture2DUINT4Table
		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 102, Flags, 0)); // Texture2DArrayTable
		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 103, Flags, 0)); // TextureCubeTable
		ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 104, Flags, 0)); // RawBufferTable
	}
	RootSignatureProxy.AddRootDescriptorTableParameter(ShaderResourceDescriptorTable);

	// UnorderedAccess
	RootDescriptorTable UnorderedAccessDescriptorTable;
	{
		constexpr D3D12_DESCRIPTOR_RANGE_FLAGS Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE | D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE;

		UnorderedAccessDescriptorTable.AddDescriptorRange(DescriptorRange::Type::UAV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 100, Flags, 0)); // RWTexture2DTable
		UnorderedAccessDescriptorTable.AddDescriptorRange(DescriptorRange::Type::UAV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 101, Flags, 0)); // RWTexture2DArrayTable
	}
	RootSignatureProxy.AddRootDescriptorTableParameter(UnorderedAccessDescriptorTable);

	// Sampler
	RootDescriptorTable SamplerDescriptorTable;
	{
		constexpr D3D12_DESCRIPTOR_RANGE_FLAGS Flags = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

		SamplerDescriptorTable.AddDescriptorRange(DescriptorRange::Type::Sampler, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 100, Flags, 0)); // SamplerTable
	}
	RootSignatureProxy.AddRootDescriptorTableParameter(SamplerDescriptorTable);
}