#include "pch.h"
#include "Resource.h"
#include "Device.h"

D3D12_RESOURCE_STATES GetD3DResourceStates(ResourceStates ResourceState)
{
	D3D12_RESOURCE_STATES states = D3D12_RESOURCE_STATE_COMMON;
	if (EnumMaskBitSet(ResourceState, ResourceStates::VertexAndConstantBuffer)) states |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
	if (EnumMaskBitSet(ResourceState, ResourceStates::IndexBuffer)) states |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
	if (EnumMaskBitSet(ResourceState, ResourceStates::RenderTarget)) states |= D3D12_RESOURCE_STATE_RENDER_TARGET;
	if (EnumMaskBitSet(ResourceState, ResourceStates::UnorderedAccess)) states |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	if (EnumMaskBitSet(ResourceState, ResourceStates::DepthWrite)) states |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
	if (EnumMaskBitSet(ResourceState, ResourceStates::DepthRead)) states |= D3D12_RESOURCE_STATE_DEPTH_READ;
	if (EnumMaskBitSet(ResourceState, ResourceStates::NonPixelShaderResource)) states |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	if (EnumMaskBitSet(ResourceState, ResourceStates::PixelShaderResource)) states |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	if (EnumMaskBitSet(ResourceState, ResourceStates::StreamOut)) states |= D3D12_RESOURCE_STATE_STREAM_OUT;
	if (EnumMaskBitSet(ResourceState, ResourceStates::IndirectArgument)) states |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	if (EnumMaskBitSet(ResourceState, ResourceStates::CopyDest)) states |= D3D12_RESOURCE_STATE_COPY_DEST;
	if (EnumMaskBitSet(ResourceState, ResourceStates::CopySource)) states |= D3D12_RESOURCE_STATE_COPY_SOURCE;
	if (EnumMaskBitSet(ResourceState, ResourceStates::ResolveDest)) states |= D3D12_RESOURCE_STATE_RESOLVE_DEST;
	if (EnumMaskBitSet(ResourceState, ResourceStates::ResolveSource)) states |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
	if (EnumMaskBitSet(ResourceState, ResourceStates::RayTracingAccelerationStructure)) states |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	if (EnumMaskBitSet(ResourceState, ResourceStates::ShadingRateSource)) states |= D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;
	if (EnumMaskBitSet(ResourceState, ResourceStates::Present)) states |= D3D12_RESOURCE_STATE_PRESENT;
	return states;
}

D3D12_RESOURCE_BARRIER GetD3DResourceBarrier(const ResourceBarrier& ResourceBarrier)
{
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	switch (ResourceBarrier.Type)
	{
	case ResourceBarrierType::Transition:
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		barrier.Transition.pResource = ResourceBarrier.Transition.pResource->GetD3DResource();
		barrier.Transition.Subresource = ResourceBarrier.Transition.Subresource;
		barrier.Transition.StateBefore = GetD3DResourceStates(ResourceBarrier.Transition.StateBefore);
		barrier.Transition.StateAfter = GetD3DResourceStates(ResourceBarrier.Transition.StateAfter);
		break;

	case ResourceBarrierType::Aliasing:
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
		barrier.Aliasing.pResourceBefore = ResourceBarrier.Aliasing.pResourceBefore->GetD3DResource();
		barrier.Aliasing.pResourceAfter = ResourceBarrier.Aliasing.pResourceAfter->GetD3DResource();
		break;

	case ResourceBarrierType::UAV:
		barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
		barrier.UAV.pResource = ResourceBarrier.UAV.pResource->GetD3DResource();
		break;
	}
	return barrier;
}

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

Resource::Resource(const Device* pDevice, const Resource::Properties& Desc, ResourceStates InitialState)
{
	const D3D12_HEAP_PROPERTIES heapProperties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	const D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE;
	const D3D12_RESOURCE_DESC resourceDesc = Desc.GetD3DResourceDesc();

	m_ClearValue = Desc.GetD3DClearValue();
	m_NumSubresources = Desc.GetNumSubresources();
	m_ResourceAllocationInfo = pDevice->GetD3DDevice()->GetResourceAllocationInfo(0, 1, &resourceDesc);
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateCommittedResource(&heapProperties,
		heapFlags,
		&resourceDesc,
		GetD3DResourceStates(InitialState),
		m_ClearValue.has_value() ? &(m_ClearValue.value()) : nullptr,
		IID_PPV_ARGS(&m_pResource)));
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
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateCommittedResource(&heapProperties,
		heapFlags,
		&resourceDesc,
		initialState,
		m_ClearValue.has_value() ? &(m_ClearValue.value()) : nullptr,
		IID_PPV_ARGS(&m_pResource)));
}

Resource::Resource(const Device* pDevice, const Resource::Properties& Desc, ResourceStates InitialState, const Heap* pHeap, UINT64 HeapOffset)
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
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreatePlacedResource(
		pHeap->GetD3DHeap(),
		HeapOffset,
		&resourceDesc,
		initialState,
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