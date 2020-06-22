#include "pch.h"
#include "RenderDevice.h"

RenderDevice::RenderDevice(IUnknown* pAdapter)
	: m_Device(pAdapter),

	m_CBVAllocator(&m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
	m_SRVAllocator(&m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
	m_UAVAllocator(&m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
	m_RTVAllocator(&m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV),
	m_DSVAllocator(&m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV),
	m_SamplerAllocator(&m_Device, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER),

	m_GraphicsQueue(&m_Device, D3D12_COMMAND_LIST_TYPE_DIRECT),
	m_ComputeQueue(&m_Device, D3D12_COMMAND_LIST_TYPE_COMPUTE),
	m_CopyQueue(&m_Device, D3D12_COMMAND_LIST_TYPE_COPY)
{
}

RenderDevice::~RenderDevice()
{
	m_GraphicsQueue.WaitForIdle();
	m_ComputeQueue.WaitForIdle();
	m_CopyQueue.WaitForIdle();
}

RenderResourceHandle RenderDevice::CreateTexture(const Texture::Properties& Properties)
{
	Texture* texture = new Texture(&m_Device, Properties);
	return this->CreateTexture(texture, Properties.Type, Properties.InitialState);
}

RenderResourceHandle RenderDevice::CreateTexture(const Texture::Properties& Properties, RenderResourceHandle HeapHandle, UINT64 HeapOffset)
{
	const auto pHeap = this->GetHeap(HeapHandle);
	Texture* texture = new Texture(&m_Device, Properties, pHeap, HeapOffset);
	return this->CreateTexture(texture, Properties.Type, Properties.InitialState);
}

RenderResourceHandle RenderDevice::CreateHeap(const Heap::Properties& Properties)
{
	std::scoped_lock lk(m_HeapAllocMutex);
	RenderResourceHandle handle = m_Heaps.first.GetNewHandle();
	m_Heaps.second[handle] = std::make_unique<Heap>(&m_Device, Properties);
	return handle;
}

RenderResourceHandle RenderDevice::CreateRootSignature(const RootSignature::Properties& Properties)
{
	std::scoped_lock lk(m_RootSignatureAllocMutex);
	RenderResourceHandle handle = m_RootSignatures.first.GetNewHandle();
	m_RootSignatures.second[handle] = std::make_unique<RootSignature>(&m_Device, Properties);
	return handle;
}

RenderResourceHandle RenderDevice::CreateGraphicsPipelineState(const GraphicsPipelineState::Properties& Properties)
{
	std::scoped_lock lk(m_GraphicsPipelineStatesAllocMutex);
	RenderResourceHandle handle = m_GraphicsPipelineStates.first.GetNewHandle();
	m_GraphicsPipelineStates.second[handle] = std::make_unique<GraphicsPipelineState>(&m_Device, Properties);
	return handle;
}

RenderResourceHandle RenderDevice::CreateComputePipelineState(const ComputePipelineState::Properties& Properties)
{
	std::scoped_lock lk(m_ComputePipelineStatesAllocMutex);
	RenderResourceHandle handle = m_ComputePipelineStates.first.GetNewHandle();
	m_ComputePipelineStates.second[handle] = std::make_unique<ComputePipelineState>(&m_Device, Properties);
	return handle;
}

void RenderDevice::Destroy(RenderResourceHandle& RenderResourceHandle)
{
	if (RenderResourceHandle._Flag == RenderResourceHandle::Flags::Destroyed)
	{
		CORE_INFO("RenderDevice::Destroy Info: Resource is already destroyed");
		return;
	}

	switch (RenderResourceHandle._Type)
	{
	case RenderResourceHandle::Type::Buffer:
	{
		auto iter = m_Buffers.second.find(RenderResourceHandle);
		if (iter != m_Buffers.second.end())
			m_Buffers.second.erase(iter);
	}
	break;

	case RenderResourceHandle::Type::Texture:
	{
		auto iter = m_Textures.second.find(RenderResourceHandle);
		if (iter != m_Textures.second.end())
			m_Textures.second.erase(iter);
	}
	break;

	case RenderResourceHandle::Type::RootSignature:
	{
		auto iter = m_RootSignatures.second.find(RenderResourceHandle);
		if (iter != m_RootSignatures.second.end())
			m_RootSignatures.second.erase(iter);
	}
	break;

	case RenderResourceHandle::Type::GraphicsPSO:
	{
		auto iter = m_GraphicsPipelineStates.second.find(RenderResourceHandle);
		if (iter != m_GraphicsPipelineStates.second.end())
			m_GraphicsPipelineStates.second.erase(iter);
	}
	break;

	case RenderResourceHandle::Type::ComputePSO:
	{
		auto iter = m_ComputePipelineStates.second.find(RenderResourceHandle);
		if (iter != m_ComputePipelineStates.second.end())
			m_ComputePipelineStates.second.erase(iter);
	}
	break;

	default:
		CORE_INFO("RenderDevice::Destroy Warning: Resource not found");
		break;
	}
	RenderResourceHandle._Type = RenderResourceHandle::Type::Unknown;
	RenderResourceHandle._Flag = RenderResourceHandle::Flags::Destroyed;
	RenderResourceHandle._Data = 0;
}

template<typename ResourceType, typename Container>
inline ResourceType* GetRawPtrResource(const RenderResourceHandle& ResourceHandle, Container& Container)
{
	auto iter = Container.find(ResourceHandle);
	if (iter != Container.end())
		return iter->second.get();
	return nullptr;
}

Resource* RenderDevice::GetResource(const RenderResourceHandle& RenderResourceHandle) const
{
	switch (RenderResourceHandle._Type)
	{
	case RenderResourceHandle::Type::Buffer:
		return (Resource*)GetBuffer(RenderResourceHandle);
	case RenderResourceHandle::Type::Texture:
		return (Resource*)GetTexture(RenderResourceHandle);
	}
	return nullptr;
}

Buffer* RenderDevice::GetBuffer(const RenderResourceHandle& RenderResourceHandle) const
{
	return GetRawPtrResource<Buffer>(RenderResourceHandle, m_Buffers.second);
}

Texture* RenderDevice::GetTexture(const RenderResourceHandle& RenderResourceHandle) const
{
	return GetRawPtrResource<Texture>(RenderResourceHandle, m_Textures.second);
}

Heap* RenderDevice::GetHeap(const RenderResourceHandle& RenderResourceHandle) const
{
	return GetRawPtrResource<Heap>(RenderResourceHandle, m_Heaps.second);
}

RootSignature* RenderDevice::GetRootSignature(const RenderResourceHandle& RenderResourceHandle) const
{
	return GetRawPtrResource<RootSignature>(RenderResourceHandle, m_RootSignatures.second);
}

GraphicsPipelineState* RenderDevice::GetGraphicsPSO(const RenderResourceHandle& RenderResourceHandle) const
{
	return GetRawPtrResource<GraphicsPipelineState>(RenderResourceHandle, m_GraphicsPipelineStates.second);
}

ComputePipelineState* RenderDevice::GetComputePSO(const RenderResourceHandle& RenderResourceHandle) const
{
	return GetRawPtrResource<ComputePipelineState>(RenderResourceHandle, m_ComputePipelineStates.second);
}

RenderResourceHandle RenderDevice::CreateBuffer(Buffer* pAllocatedBuffer, ResourceStates InitialState)
{
	m_GlobalResourceTracker.AddResourceState(pAllocatedBuffer, InitialState);
	auto deleter = [&globalResourceTracker = m_GlobalResourceTracker](Buffer* pBuffer)
	{
		globalResourceTracker.RemoveResourceState(pBuffer);
		delete pBuffer;
	};

	RenderResourceHandle handle = m_Buffers.first.GetNewHandle();
	m_Buffers.second[handle] = { pAllocatedBuffer, deleter };
	return handle;
}

RenderResourceHandle RenderDevice::CreateTexture(Texture* pAllocatedTexture, Resource::Type Type, ResourceStates InitialState)
{
	m_GlobalResourceTracker.AddResourceState(pAllocatedTexture, InitialState);
	auto deleter = [&globalResourceTracker = m_GlobalResourceTracker](Texture* pTexture)
	{
		globalResourceTracker.RemoveResourceState(pTexture);
		delete pTexture;
	};

	RenderResourceHandle handle = m_Textures.first.GetNewHandle();
	m_Textures.second[handle] = { pAllocatedTexture, deleter };
	return handle;
}