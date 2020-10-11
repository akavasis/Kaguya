#include "pch.h"
#include "RenderDevice.h"

RenderDevice::RenderDevice(IDXGIAdapter4* pAdapter)
	: Device(pAdapter),
	GraphicsQueue(&Device, D3D12_COMMAND_LIST_TYPE_DIRECT),
	ComputeQueue(&Device, D3D12_COMMAND_LIST_TYPE_COMPUTE),
	CopyQueue(&Device, D3D12_COMMAND_LIST_TYPE_COPY),
	FrameIndex(0),
	SwapChainTextures{},
	m_CBSRUADescriptorHeap(&Device, NumConstantBufferDescriptors, NumShaderResourceDescriptors, NumUnorderedAccessDescriptors, true),
	m_SamplerDescriptorHeap(&Device, NumSamplerDescriptors, true),
	m_RenderTargetDescriptorHeap(&Device, NumRenderTargetDescriptors),
	m_DepthStencilDescriptorHeap(&Device, NumDepthStencilDescriptors)
{
}

CommandContext* RenderDevice::AllocateContext(CommandContext::Type Type)
{
	CommandQueue* pCommandQueue = nullptr;
	switch (Type)
	{
	case CommandContext::Type::Direct: pCommandQueue = &GraphicsQueue; break;
	case CommandContext::Type::Compute: pCommandQueue = &ComputeQueue; break;
	case CommandContext::Type::Copy: pCommandQueue = &CopyQueue; break;
	default: throw std::logic_error("Unsupported command list type"); break;
	}
	m_RenderCommandContexts[Type].push_back(std::make_unique<CommandContext>(&Device, pCommandQueue, Type));
	return m_RenderCommandContexts[Type].back().get();
}

void RenderDevice::BindUniversalGpuDescriptorHeap(CommandContext* pCommandContext)
{
	pCommandContext->SetDescriptorHeaps(&m_CBSRUADescriptorHeap, &m_SamplerDescriptorHeap);
}

void RenderDevice::ExecuteRenderCommandContexts(UINT NumCommandContexts, CommandContext* ppCommandContexts[])
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

	GraphicsQueue.GetD3DCommandQueue()->ExecuteCommandLists(commandlistsToBeExecuted.size(), commandlistsToBeExecuted.data());
	UINT64 fenceValue = GraphicsQueue.Signal();
	for (UINT i = 0; i < NumCommandContexts; ++i)
	{
		CommandContext* pCommandContext = ppCommandContexts[i];
		pCommandContext->RequestNewAllocator(fenceValue);
		pCommandContext->Reset();
	}
	GraphicsQueue.Flush();
}

void RenderDevice::ResetDescriptor()
{
	m_ConstantBufferDescriptorIndexPool.Reset();
	m_ShaderResourceDescriptorIndexPool.Reset();
	m_UnorderedAccessDescriptorIndexPool.Reset();
	m_SamplerDescriptorIndexPool.Reset();
	m_RenderTargetDescriptorIndexPool.Reset();
	m_DepthStencilDescriptorIndexPool.Reset();
}

Shader RenderDevice::CompileShader(Shader::Type Type, const std::filesystem::path& Path, LPCWSTR pEntryPoint, const std::vector<DxcDefine>& ShaderDefines)
{
	return m_ShaderCompiler.CompileShader(Type, Path.c_str(), pEntryPoint, ShaderDefines);
}

Library RenderDevice::CompileLibrary(const std::filesystem::path& Path)
{
	return m_ShaderCompiler.CompileLibrary(Path.c_str());
}

RenderResourceHandle RenderDevice::CreateBuffer(std::function<void(BufferProxy&)> Configurator)
{
	BufferProxy proxy;
	Configurator(proxy);

	auto [handle, buffer] = m_Buffers.CreateResource(&Device, proxy);

	// No need to track resources that have preinitialized states
	if (buffer->GetCpuAccess() == Buffer::CpuAccess::None)
		GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), GetD3DResourceStates(proxy.InitialState));
	return handle;
}

RenderResourceHandle RenderDevice::CreateBuffer(RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(BufferProxy&)> Configurator)
{
	BufferProxy proxy;
	Configurator(proxy);

	const auto pHeap = this->GetHeap(HeapHandle);
	auto [handle, buffer] = m_Buffers.CreateResource(&Device, pHeap, HeapOffset, proxy);
	GlobalResourceStateTracker.AddResourceState(buffer->GetD3DResource(), GetD3DResourceStates(proxy.InitialState));
	return handle;
}

RenderResourceHandle RenderDevice::CreateTexture(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingResource, Resource::State InitialState)
{
	auto [handle, texture] = m_Textures.CreateResource(ExistingResource);
	GlobalResourceStateTracker.AddResourceState(ExistingResource.Get(), GetD3DResourceStates(InitialState));
	return handle;
}

RenderResourceHandle RenderDevice::CreateTexture(Resource::Type Type, std::function<void(TextureProxy&)> Configurator)
{
	TextureProxy proxy(Type);
	Configurator(proxy);

	auto [handle, texture] = m_Textures.CreateResource(&Device, proxy);
	GlobalResourceStateTracker.AddResourceState(texture->GetD3DResource(), GetD3DResourceStates(proxy.InitialState));
	return handle;
}

RenderResourceHandle RenderDevice::CreateTexture(Resource::Type Type, RenderResourceHandle HeapHandle, UINT64 HeapOffset, std::function<void(TextureProxy&)> Configurator)
{
	TextureProxy proxy(Type);
	Configurator(proxy);

	const auto pHeap = this->GetHeap(HeapHandle);
	assert(pHeap->GetType() != Heap::Type::Upload && "Heap cannot be type upload");
	assert(pHeap->GetType() != Heap::Type::Readback && "Heap cannot be type readback");

	auto [handle, texture] = m_Textures.CreateResource(&Device, pHeap, HeapOffset, proxy);
	GlobalResourceStateTracker.AddResourceState(texture->GetD3DResource(), GetD3DResourceStates(proxy.InitialState));
	return handle;
}

RenderResourceHandle RenderDevice::CreateHeap(std::function<void(HeapProxy&)> Configurator)
{
	HeapProxy proxy;
	Configurator(proxy);

	auto [handle, heap] = m_Heaps.CreateResource(&Device, proxy);
	return handle;
}

RenderResourceHandle RenderDevice::CreateRootSignature(std::function<void(RootSignatureProxy&)> Configurator, bool AddShaderLayoutRootParameters)
{
	RootSignatureProxy proxy;
	Configurator(proxy);
	if (AddShaderLayoutRootParameters)
		AddShaderLayoutRootParameter(proxy);

	auto [handle, rootSignature] = m_RootSignatures.CreateResource(&Device, proxy);
	return handle;
}

RenderResourceHandle RenderDevice::CreateGraphicsPipelineState(std::function<void(GraphicsPipelineStateProxy&)> Configurator)
{
	GraphicsPipelineStateProxy proxy;
	Configurator(proxy);

	auto [handle, graphicsPSO] = m_GraphicsPipelineStates.CreateResource(&Device, proxy);
	return handle;
}

RenderResourceHandle RenderDevice::CreateComputePipelineState(std::function<void(ComputePipelineStateProxy&)> Configurator)
{
	ComputePipelineStateProxy proxy;
	Configurator(proxy);

	auto [handle, computePSO] = m_ComputePipelineStates.CreateResource(&Device, proxy);
	return handle;
}

RenderResourceHandle RenderDevice::CreateRaytracingPipelineState(std::function<void(RaytracingPipelineStateProxy&)> Configurator)
{
	RaytracingPipelineStateProxy proxy;
	Configurator(proxy);

	auto [handle, raytracingPSO] = m_RaytracingPipelineStates.CreateResource(&Device, proxy);
	return handle;
}

void RenderDevice::Destroy(RenderResourceHandle* pRenderResourceHandle)
{
	if (!pRenderResourceHandle ||
		pRenderResourceHandle->Type == RenderResourceType::Unknown ||
		pRenderResourceHandle->Flags == RenderResourceFlags::Destroyed)
	{
		return;
	}

	switch (pRenderResourceHandle->Type)
	{
	case RenderResourceType::Buffer:
	{
		auto buffer = m_Buffers.GetResource(*pRenderResourceHandle);
		// Remove from GRST
		GlobalResourceStateTracker.RemoveResourceState(buffer->GetD3DResource());

		// Remove all the resource views
		if (auto iter = m_RenderBuffers.find(*pRenderResourceHandle);
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
		m_Buffers.Destroy(pRenderResourceHandle);
	}
	break;
	case RenderResourceType::Texture:
	{
		auto texture = m_Textures.GetResource(*pRenderResourceHandle);
		// Remove from GRST
		GlobalResourceStateTracker.RemoveResourceState(texture->GetD3DResource());

		// Remove all the resource views
		if (auto iter = m_RenderTextures.find(*pRenderResourceHandle);
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
		m_Textures.Destroy(pRenderResourceHandle);
	}
	break;
	case RenderResourceType::Heap: m_Heaps.Destroy(pRenderResourceHandle); break;
	case RenderResourceType::RootSignature: m_RootSignatures.Destroy(pRenderResourceHandle); break;
	case RenderResourceType::GraphicsPSO: m_GraphicsPipelineStates.Destroy(pRenderResourceHandle); break;
	case RenderResourceType::ComputePSO: m_ComputePipelineStates.Destroy(pRenderResourceHandle); break;
	case RenderResourceType::RaytracingPSO: m_RaytracingPipelineStates.Destroy(pRenderResourceHandle); break;
	}
	pRenderResourceHandle->Type = RenderResourceType::Unknown;
	pRenderResourceHandle->Flags = RenderResourceFlags::Destroyed;
	pRenderResourceHandle->Data = 0;
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
				m_CBSRUADescriptorHeap.AssignSRDescriptor(renderBuffer.ShaderResourceView.HeapIndex,
					renderBuffer.pBuffer);
			}
			// Create new one
			else
			{
				size_t HeapIndex = m_ShaderResourceDescriptorIndexPool.Allocate();
				m_CBSRUADescriptorHeap.AssignSRDescriptor(HeapIndex, renderBuffer.pBuffer);
				renderBuffer.ShaderResourceView = m_CBSRUADescriptorHeap.GetUADescriptorAt(HeapIndex);
			}
		}
		else
		{
			// First-time request
			Buffer* pBuffer = GetBuffer(RenderResourceHandle);
			assert(pBuffer && "Could not find buffer given the handle");

			size_t HeapIndex = m_ShaderResourceDescriptorIndexPool.Allocate();
			m_CBSRUADescriptorHeap.AssignSRDescriptor(HeapIndex, pBuffer);

			RenderBuffer renderBuffer;
			renderBuffer.pBuffer = pBuffer;
			renderBuffer.ShaderResourceView = m_CBSRUADescriptorHeap.GetSRDescriptorAt(HeapIndex);

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
				m_CBSRUADescriptorHeap.AssignSRDescriptor(srvIter->second.HeapIndex, renderTexture.pTexture, MostDetailedMip, MipLevels);
			}
			else
			{
				// Add a new descriptor
				size_t HeapIndex = m_ShaderResourceDescriptorIndexPool.Allocate();
				UINT64 HashValue = m_CBSRUADescriptorHeap.AssignSRDescriptor(HeapIndex, renderTexture.pTexture, MostDetailedMip, MipLevels);

				renderTexture.ShaderResourceViews[HashValue] = m_CBSRUADescriptorHeap.GetSRDescriptorAt(HeapIndex);
			}
		}
		else
		{
			// First-time request
			Texture* pTexture = GetTexture(RenderResourceHandle);
			assert(pTexture && "Could not find texture given the handle");

			size_t HeapIndex = m_ShaderResourceDescriptorIndexPool.Allocate();
			UINT64 HashValue = m_CBSRUADescriptorHeap.AssignSRDescriptor(HeapIndex, pTexture, MostDetailedMip, MipLevels);

			RenderTexture renderTexture;
			renderTexture.pTexture = pTexture;
			renderTexture.ShaderResourceViews[HashValue] = m_CBSRUADescriptorHeap.GetSRDescriptorAt(HeapIndex);

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
				m_CBSRUADescriptorHeap.AssignUADescriptor(renderBuffer.UnorderedAccessView.HeapIndex,
					renderBuffer.pBuffer);
			}
			// Create new one
			else
			{
				size_t HeapIndex = m_UnorderedAccessDescriptorIndexPool.Allocate();
				m_CBSRUADescriptorHeap.AssignUADescriptor(HeapIndex, renderBuffer.pBuffer);
				renderBuffer.UnorderedAccessView = m_CBSRUADescriptorHeap.GetUADescriptorAt(HeapIndex);
			}
		}
		else
		{
			// First-time request
			Buffer* pBuffer = GetBuffer(RenderResourceHandle);
			assert(pBuffer && "Could not find buffer given the handle");

			size_t HeapIndex = m_UnorderedAccessDescriptorIndexPool.Allocate();
			m_CBSRUADescriptorHeap.AssignUADescriptor(HeapIndex, pBuffer);

			RenderBuffer renderBuffer;
			renderBuffer.pBuffer = pBuffer;
			renderBuffer.UnorderedAccessView = m_CBSRUADescriptorHeap.GetUADescriptorAt(HeapIndex);

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
				m_CBSRUADescriptorHeap.AssignUADescriptor(uavIter->second.HeapIndex, renderTexture.pTexture, ArraySlice, MipSlice);
			}
			else
			{
				// Add a new descriptor
				size_t HeapIndex = m_UnorderedAccessDescriptorIndexPool.Allocate();
				UINT64 HashValue = m_CBSRUADescriptorHeap.AssignUADescriptor(HeapIndex, renderTexture.pTexture, ArraySlice, MipSlice);

				renderTexture.UnorderedAccessViews[HashValue] = m_CBSRUADescriptorHeap.GetUADescriptorAt(HeapIndex);
			}
		}
		else
		{
			// First-time request
			Texture* pTexture = GetTexture(RenderResourceHandle);
			assert(pTexture && "Could not find texture given the handle");

			size_t HeapIndex = m_UnorderedAccessDescriptorIndexPool.Allocate();
			UINT64 HashValue = m_CBSRUADescriptorHeap.AssignUADescriptor(HeapIndex, pTexture, ArraySlice, MipSlice);

			RenderTexture renderTexture;
			renderTexture.pTexture = pTexture;
			renderTexture.UnorderedAccessViews[HashValue] = m_CBSRUADescriptorHeap.GetUADescriptorAt(HeapIndex);

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
	// RenderPassDataCB
	RootSignatureProxy.AddRootCBVParameter(RootCBV(0, 100));

	/* Descriptor Tables */
	constexpr D3D12_DESCRIPTOR_RANGE_FLAGS flag = D3D12_DESCRIPTOR_RANGE_FLAG_DESCRIPTORS_VOLATILE;

	// ShaderResource
	RootDescriptorTable ShaderResourceDescriptorTable;
	ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 100, flag, 0)); // Texture2DTable
	ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 101, flag, 0)); // Texture2DArrayTable
	ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 102, flag, 0)); // TextureCubeTable
	ShaderResourceDescriptorTable.AddDescriptorRange(DescriptorRange::Type::SRV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 103, flag, 0)); // RawBufferTable
	RootSignatureProxy.AddRootDescriptorTableParameter(ShaderResourceDescriptorTable);

	// UnorderedAccess
	RootDescriptorTable UnorderedAccessDescriptorTable;
	UnorderedAccessDescriptorTable.AddDescriptorRange(DescriptorRange::Type::UAV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 100, flag, 0)); // RWTexture2DTable
	UnorderedAccessDescriptorTable.AddDescriptorRange(DescriptorRange::Type::UAV, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 101, flag, 0)); // RWTexture2DArrayTable
	RootSignatureProxy.AddRootDescriptorTableParameter(UnorderedAccessDescriptorTable);

	// Sampler
	RootDescriptorTable SamplerDescriptorTable;
	SamplerDescriptorTable.AddDescriptorRange(DescriptorRange::Type::Sampler, DescriptorRange(RootSignature::UnboundDescriptorSize, 0, 100, flag, 0)); // SamplerTable
	RootSignatureProxy.AddRootDescriptorTableParameter(SamplerDescriptorTable);
}