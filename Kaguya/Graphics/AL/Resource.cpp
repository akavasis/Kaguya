#include "pch.h"
#include "Resource.h"

Resource::Properties Resource::Properties::Buffer(UINT64 Width, D3D12_RESOURCE_FLAGS Flags)
{
	Resource::Properties properties;
	properties.m_Desc = CD3DX12_RESOURCE_DESC::Buffer(Width, Flags);
	properties.m_ClearValue = std::nullopt;
	properties.m_NumSubresources = 1;
	return properties;
}

Resource::Properties Resource::Properties::Texture(Resource::Type Type,
	DXGI_FORMAT Format,
	UINT64 Width,
	UINT Height,
	UINT16 DepthOrArraySize,
	UINT16 MipLevels,
	D3D12_RESOURCE_FLAGS Flags,
	const D3D12_CLEAR_VALUE* pOptimizedClearValue)
{
	Resource::Properties properties;
	switch (Type)
	{
	case Resource::Type::Texture1D: properties.m_Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE1D; break;
	case Resource::Type::Texture2D: properties.m_Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D; break;
	case Resource::Type::Texture3D: properties.m_Desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D; break;
	}
	properties.m_Desc.Alignment = 0;
	properties.m_Desc.Width = Width;
	properties.m_Desc.Height = Height;
	properties.m_Desc.DepthOrArraySize = DepthOrArraySize;
	properties.m_Desc.MipLevels = MipLevels;
	properties.m_Desc.Format = Format;
	properties.m_Desc.SampleDesc = { .Count = 1, .Quality = 0 };
	properties.m_Desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	properties.m_Desc.Flags = Flags;
	properties.m_ClearValue = pOptimizedClearValue ? std::make_optional(*pOptimizedClearValue) : std::nullopt;
	bool isArray = (Type == Resource::Type::Texture1D || Type == Resource::Type::Texture2D) && DepthOrArraySize > 1;
	properties.m_NumSubresources = isArray ? DepthOrArraySize * MipLevels : MipLevels;
	return properties;
}

Resource::Resource(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource)
	: m_pResource(ExistingID3D12Resource),
	m_ResourceAllocationInfo{}
{
}

Resource::Resource(const Device* pDevice, const Resource::Properties& Desc, D3D12_RESOURCE_STATES InitialState)
{
	const D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
	const D3D12_RESOURCE_DESC resourceDesc = Desc.GetD3DResourceDesc();

	m_ClearValue = Desc.GetD3DClearValue();
	m_NumSubresources = Desc.GetNumSubresources();
	m_ResourceAllocationInfo = pDevice->GetD3DDevice()->GetResourceAllocationInfo(0, 1, &resourceDesc);
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateCommittedResource(&heapProperties, heapFlags, &resourceDesc, InitialState, &m_ClearValue.value_or(nullptr), IID_PPV_ARGS(&m_pResource)));
}

Resource::Resource(const Device* pDevice, const Resource::Properties& Desc, CPUAccessibleHeapType CPUAccessibleHeapType)
{
	D3D12_RESOURCE_STATES initialState{};
	D3D12_HEAP_PROPERTIES heapProperties{};
	const D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
	const D3D12_RESOURCE_DESC resourceDesc = Desc.GetD3DResourceDesc();

	switch (CPUAccessibleHeapType)
	{
	case CPUAccessibleHeapType::Upload:
		initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
		heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		break;
	case CPUAccessibleHeapType::Readback:
		initialState = D3D12_RESOURCE_STATE_COPY_DEST;
		heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK);
		break;
	}

	m_ClearValue = Desc.GetD3DClearValue();
	m_NumSubresources = Desc.GetNumSubresources();
	m_ResourceAllocationInfo = pDevice->GetD3DDevice()->GetResourceAllocationInfo(0, 1, &resourceDesc);
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateCommittedResource(&heapProperties, heapFlags, &resourceDesc, initialState, &m_ClearValue.value_or(nullptr), IID_PPV_ARGS(&m_pResource)));
}

Resource::Resource(const Device* pDevice, const Resource::Properties& Desc, D3D12_RESOURCE_STATES InitialState, const Heap* pHeap, UINT64 HeapOffset)
{
	D3D12_RESOURCE_STATES initialState{};
	const D3D12_RESOURCE_DESC resourceDesc = Desc.GetD3DResourceDesc();

	if (pHeap->GetCPUAccessibleHeapType())
	{
		switch (pHeap->GetCPUAccessibleHeapType().value())
		{
		case CPUAccessibleHeapType::Upload:
			initialState = D3D12_RESOURCE_STATE_GENERIC_READ;
			break;
		case CPUAccessibleHeapType::Readback:
			initialState = D3D12_RESOURCE_STATE_COPY_DEST;
			break;
		}
	}

	m_ClearValue = Desc.GetD3DClearValue();
	m_NumSubresources = Desc.GetNumSubresources();
	m_ResourceAllocationInfo = pDevice->GetD3DDevice()->GetResourceAllocationInfo(0, 1, &resourceDesc);
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreatePlacedResource(pHeap->GetD3DHeap(), HeapOffset, &resourceDesc, initialState, &m_ClearValue.value_or(nullptr), IID_PPV_ARGS(&m_pResource)));
}

Resource::~Resource()
{
}