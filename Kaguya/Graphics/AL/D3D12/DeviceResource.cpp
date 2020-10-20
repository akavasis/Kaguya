#include "pch.h"
#include "DeviceResource.h"
#include "Device.h"
#include "../Proxy/DeviceResourceProxy.h"

DeviceResource::DeviceResource(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource)
	: m_pResource(ExistingID3D12Resource)
{
	ID3D12Device* pDevice = nullptr;
	ExistingID3D12Resource->GetDevice(IID_PPV_ARGS(&pDevice));

	const D3D12_RESOURCE_DESC				ResourceDesc		= m_pResource->GetDesc();
	const bool								IsArray				= (ResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D || 
																	ResourceDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) && 
																	ResourceDesc.DepthOrArraySize > 1;
	const D3D12_RESOURCE_ALLOCATION_INFO ResourceAllocationInfo = pDevice->GetResourceAllocationInfo(0, 1, &ResourceDesc);
	pDevice->Release();

	switch (ResourceDesc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D: m_Type = DeviceResource::Type::Texture1D; break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D: m_Type = DeviceResource::Type::Texture2D; break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D: m_Type = DeviceResource::Type::Texture3D; break;
	default: m_Type = DeviceResource::Type::Unknown; break;
	}
	m_BindFlags = {};
	m_NumSubresources = IsArray ? ResourceDesc.DepthOrArraySize * ResourceDesc.MipLevels : ResourceDesc.MipLevels;
	m_MemoryRequested = ResourceDesc.Width;
	m_SizeInBytes = ResourceAllocationInfo.SizeInBytes;
	m_Alignment = ResourceAllocationInfo.Alignment;
	m_HeapOffset = 0;
}

DeviceResource::DeviceResource(const Device* pDevice, DeviceResourceProxy& Proxy)
{
	Proxy.Link();

	const D3D12_HEAP_PROPERTIES				HeapProperties			= Proxy.BuildD3DHeapProperties();
	const D3D12_HEAP_FLAGS					HeapFlags				= D3D12_HEAP_FLAG_NONE;
	const D3D12_RESOURCE_DESC				ResourceDesc			= Proxy.BuildD3DDesc();
	const D3D12_RESOURCE_STATES				ResourceStates			= GetD3DResourceStates(Proxy.InitialState);
	const D3D12_CLEAR_VALUE*				pClearValue				= Proxy.m_ClearValue.has_value() ? &(Proxy.m_ClearValue.value()) : nullptr;
	const D3D12_RESOURCE_ALLOCATION_INFO	ResourceAllocationInfo	= pDevice->GetD3DDevice()->GetResourceAllocationInfo(0, 1, &ResourceDesc);

	m_Type				= Proxy.m_Type;
	m_BindFlags			= Proxy.BindFlags;
	m_NumSubresources	= Proxy.m_NumSubresources;
	m_MemoryRequested	= ResourceDesc.Width;
	m_SizeInBytes		= ResourceAllocationInfo.SizeInBytes;
	m_Alignment			= ResourceAllocationInfo.Alignment;
	m_HeapOffset		= 0;
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateCommittedResource(
		&HeapProperties,
		HeapFlags,
		&ResourceDesc,
		ResourceStates,
		pClearValue,
		IID_PPV_ARGS(&m_pResource)));
}

DeviceResource::DeviceResource(const Device* pDevice, const Heap* pHeap, UINT64 HeapOffset, DeviceResourceProxy& Proxy)
{
	Proxy.Link();

	const D3D12_RESOURCE_DESC				ResourceDesc			= Proxy.BuildD3DDesc();
	D3D12_RESOURCE_STATES					ResourceStates			= GetD3DResourceStates(Proxy.InitialState);
	const D3D12_CLEAR_VALUE*				pClearValue				= Proxy.m_ClearValue.has_value() ? &(Proxy.m_ClearValue.value()) : nullptr;
	const D3D12_RESOURCE_ALLOCATION_INFO	ResourceAllocationInfo	= pDevice->GetD3DDevice()->GetResourceAllocationInfo(0, 1, &ResourceDesc);

	m_Type				= Proxy.m_Type;
	m_BindFlags			= Proxy.BindFlags;
	m_NumSubresources	= Proxy.m_NumSubresources;
	m_MemoryRequested	= ResourceDesc.Width;
	m_SizeInBytes		= ResourceAllocationInfo.SizeInBytes;
	m_Alignment			= ResourceAllocationInfo.Alignment;
	m_HeapOffset		= HeapOffset;
	switch (pHeap->GetType())
	{
	case Heap::Type::Upload:	ResourceStates = D3D12_RESOURCE_STATE_GENERIC_READ; break;
	case Heap::Type::Readback:	ResourceStates = D3D12_RESOURCE_STATE_COPY_DEST;	break;
	}

	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreatePlacedResource(
		pHeap->GetD3DHeap(),
		HeapOffset,
		&ResourceDesc,
		ResourceStates,
		pClearValue,
		IID_PPV_ARGS(&m_pResource)));
}

DeviceResource::~DeviceResource()
{
}

D3D12_RESOURCE_DIMENSION GetD3DResourceDimension(DeviceResource::Type Type)
{
	switch (Type)
	{
	case DeviceResource::Type::Buffer:	return D3D12_RESOURCE_DIMENSION_BUFFER;
	case DeviceResource::Type::Texture1D: return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
	case DeviceResource::Type::Texture2D: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	case DeviceResource::Type::Texture3D: return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	default:						return D3D12_RESOURCE_DIMENSION_UNKNOWN;
	}
}

D3D12_RESOURCE_FLAGS GetD3DResourceFlags(DeviceResource::BindFlags Flags)
{
	D3D12_RESOURCE_FLAGS ResourceFlags = D3D12_RESOURCE_FLAG_NONE;

	if (EnumMaskBitSet(Flags, DeviceResource::BindFlags::RenderTarget)) ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (EnumMaskBitSet(Flags, DeviceResource::BindFlags::DepthStencil)) ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (EnumMaskBitSet(Flags, DeviceResource::BindFlags::UnorderedAccess) ||
		EnumMaskBitSet(Flags, DeviceResource::BindFlags::AccelerationStructure)) // Acceleration structure must have UAV access
		ResourceFlags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	return ResourceFlags;
}

D3D12_RESOURCE_STATES GetD3DResourceStates(DeviceResource::State State)
{
	D3D12_RESOURCE_STATES ResourceStates = D3D12_RESOURCE_STATE_COMMON;
	if (EnumMaskBitSet(State, DeviceResource::State::Unknown) ||
		EnumMaskBitSet(State, DeviceResource::State::Common))
		ResourceStates |= D3D12_RESOURCE_STATE_COMMON;

	if (EnumMaskBitSet(State, DeviceResource::State::VertexBuffer) ||
		EnumMaskBitSet(State, DeviceResource::State::ConstantBuffer))
		ResourceStates |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

	if (EnumMaskBitSet(State, DeviceResource::State::IndexBuffer))				ResourceStates |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
	if (EnumMaskBitSet(State, DeviceResource::State::RenderTarget))				ResourceStates |= D3D12_RESOURCE_STATE_RENDER_TARGET;
	if (EnumMaskBitSet(State, DeviceResource::State::UnorderedAccess))			ResourceStates |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	if (EnumMaskBitSet(State, DeviceResource::State::DepthWrite))				ResourceStates |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
	if (EnumMaskBitSet(State, DeviceResource::State::DepthRead))				ResourceStates |= D3D12_RESOURCE_STATE_DEPTH_READ;
	if (EnumMaskBitSet(State, DeviceResource::State::NonPixelShaderResource))	ResourceStates |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	if (EnumMaskBitSet(State, DeviceResource::State::PixelShaderResource))		ResourceStates |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	if (EnumMaskBitSet(State, DeviceResource::State::StreamOut))				ResourceStates |= D3D12_RESOURCE_STATE_STREAM_OUT;
	if (EnumMaskBitSet(State, DeviceResource::State::IndirectArgument))			ResourceStates |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	if (EnumMaskBitSet(State, DeviceResource::State::CopyDest))					ResourceStates |= D3D12_RESOURCE_STATE_COPY_DEST;
	if (EnumMaskBitSet(State, DeviceResource::State::CopySource))				ResourceStates |= D3D12_RESOURCE_STATE_COPY_SOURCE;
	if (EnumMaskBitSet(State, DeviceResource::State::ResolveDest))				ResourceStates |= D3D12_RESOURCE_STATE_RESOLVE_DEST;
	if (EnumMaskBitSet(State, DeviceResource::State::ResolveSource))			ResourceStates |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
	if (EnumMaskBitSet(State, DeviceResource::State::AccelerationStructure))	ResourceStates |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	if (EnumMaskBitSet(State, DeviceResource::State::ShadingRateSource))		ResourceStates |= D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;
	if (EnumMaskBitSet(State, DeviceResource::State::GenericRead))				ResourceStates |= D3D12_RESOURCE_STATE_GENERIC_READ;
	if (EnumMaskBitSet(State, DeviceResource::State::Present))					ResourceStates |= D3D12_RESOURCE_STATE_PRESENT;
	if (EnumMaskBitSet(State, DeviceResource::State::Predication))				ResourceStates |= D3D12_RESOURCE_STATE_PREDICATION;

	return ResourceStates;
}