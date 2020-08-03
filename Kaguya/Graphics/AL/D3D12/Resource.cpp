#include "pch.h"
#include "Resource.h"
#include "Device.h"

const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
{
	.Type					= D3D12_HEAP_TYPE_DEFAULT,
	.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask		= 0,
	.VisibleNodeMask		= 0
};

const D3D12_HEAP_PROPERTIES kUploadHeapProps =
{
	.Type					= D3D12_HEAP_TYPE_UPLOAD,
	.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask		= 0,
	.VisibleNodeMask		= 0,
};

const D3D12_HEAP_PROPERTIES kReadbackHeapProps =
{
	.Type					= D3D12_HEAP_TYPE_READBACK,
	.CPUPageProperty		= D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference	= D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask		= 0,
	.VisibleNodeMask		= 0
};

Resource::Properties Resource::Properties::Buffer(UINT64 SizeInBytes, D3D12_RESOURCE_FLAGS Flags)
{
	Resource::Properties properties;
	properties.m_Desc = CD3DX12_RESOURCE_DESC::Buffer(SizeInBytes, Flags);
	properties.m_ClearValue = std::nullopt;
	properties.m_NumSubresources = 1;
	return properties;
}

Resource::Properties Resource::Properties::Texture(Resource::Type Type, DXGI_FORMAT Format,
	UINT64 Width, UINT Height, UINT16 DepthOrArraySize, UINT16 MipLevels,
	D3D12_RESOURCE_FLAGS Flags, const D3D12_CLEAR_VALUE* pOptimizedClearValue)
{
	Resource::Properties properties;
	switch (Type)
	{
	case Resource::Type::Texture1D: properties.m_Desc = CD3DX12_RESOURCE_DESC::Tex1D(Format, Width, DepthOrArraySize, MipLevels, Flags); break;
	case Resource::Type::Texture2D: properties.m_Desc = CD3DX12_RESOURCE_DESC::Tex2D(Format, Width, Height, DepthOrArraySize, MipLevels, 1, 0, Flags); break;
	case Resource::Type::Texture3D: properties.m_Desc = CD3DX12_RESOURCE_DESC::Tex3D(Format, Width, Height, DepthOrArraySize, MipLevels, Flags); break;
	}
	properties.m_ClearValue = pOptimizedClearValue ? std::make_optional(*pOptimizedClearValue) : std::nullopt;
	bool isArray = (Type == Resource::Type::Texture1D || Type == Resource::Type::Texture2D) && DepthOrArraySize > 1;
	properties.m_NumSubresources = isArray ? DepthOrArraySize * MipLevels : MipLevels;
	return properties;
}

Resource::Resource(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource)
	: m_pResource(ExistingID3D12Resource),
	m_ResourceAllocationInfo{}
{
	D3D12_RESOURCE_DESC desc = m_pResource->GetDesc();
	bool isArray = (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D || desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) && desc.DepthOrArraySize > 1;
	m_NumSubresources = isArray ? desc.DepthOrArraySize * desc.MipLevels : desc.MipLevels;
}

Resource::Resource(const Device* pDevice, const Resource::Properties& Properties, D3D12_RESOURCE_STATES InitialState)
{
	const D3D12_HEAP_PROPERTIES heapProperties = kDefaultHeapProps;
	const D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
	const D3D12_RESOURCE_DESC resourceDesc = Properties.GetD3DResourceDesc();

	m_ClearValue = Properties.GetD3DClearValue();
	m_NumSubresources = Properties.GetNumSubresources();
	m_ResourceAllocationInfo = pDevice->GetD3DDevice()->GetResourceAllocationInfo(0, 1, &resourceDesc);
	m_Flags = resourceDesc.Flags;
	m_InitialState = InitialState;
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateCommittedResource(&heapProperties,
		heapFlags,
		&resourceDesc,
		m_InitialState,
		m_ClearValue.has_value() ? &(m_ClearValue.value()) : nullptr,
		IID_PPV_ARGS(&m_pResource)));
}

Resource::Resource(const Device* pDevice, const Resource::Properties& Properties, CPUAccessibleHeapType CPUAccessibleHeapType)
{
	D3D12_RESOURCE_STATES initialState{};
	D3D12_HEAP_PROPERTIES heapProperties{};
	const D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
	const D3D12_RESOURCE_DESC resourceDesc = Properties.GetD3DResourceDesc();

	switch (CPUAccessibleHeapType)
	{
	case CPUAccessibleHeapType::Upload:
		initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
		heapProperties = kUploadHeapProps;
		break;
	case CPUAccessibleHeapType::Readback:
		initialState = D3D12_RESOURCE_STATE_COPY_DEST;
		heapProperties = kReadbackHeapProps;
		break;
	}

	m_ClearValue = Properties.GetD3DClearValue();
	m_NumSubresources = Properties.GetNumSubresources();
	m_ResourceAllocationInfo = pDevice->GetD3DDevice()->GetResourceAllocationInfo(0, 1, &resourceDesc);
	m_Flags = resourceDesc.Flags;
	m_InitialState = initialState;
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateCommittedResource(&heapProperties,
		heapFlags,
		&resourceDesc,
		m_InitialState,
		m_ClearValue.has_value() ? &(m_ClearValue.value()) : nullptr,
		IID_PPV_ARGS(&m_pResource)));
}

Resource::Resource(const Device* pDevice, const Resource::Properties& Properties, D3D12_RESOURCE_STATES InitialState, const Heap* pHeap, UINT64 HeapOffset)
{
	D3D12_RESOURCE_STATES initialState = InitialState;
	const D3D12_RESOURCE_DESC resourceDesc = Properties.GetD3DResourceDesc();

	if (pHeap->GetCPUAccessibleHeapType())
	{
		switch (pHeap->GetCPUAccessibleHeapType().value())
		{
		case CPUAccessibleHeapType::Upload: initialState = D3D12_RESOURCE_STATE_GENERIC_READ; break;
		case CPUAccessibleHeapType::Readback: initialState = D3D12_RESOURCE_STATE_COPY_DEST; break;
		}
	}

	m_ClearValue = Properties.GetD3DClearValue();
	m_NumSubresources = Properties.GetNumSubresources();
	m_ResourceAllocationInfo = pDevice->GetD3DDevice()->GetResourceAllocationInfo(0, 1, &resourceDesc);
	m_Flags = resourceDesc.Flags;
	m_InitialState = initialState;
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreatePlacedResource(
		pHeap->GetD3DHeap(),
		HeapOffset,
		&resourceDesc,
		m_InitialState,
		m_ClearValue.has_value() ? &(m_ClearValue.value()) : nullptr,
		IID_PPV_ARGS(&m_pResource)));
}

Resource::~Resource()
{
}

void Resource::Release()
{
	m_pResource.Reset();
}