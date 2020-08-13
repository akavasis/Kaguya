#include "pch.h"
#include "Resource.h"
#include "Device.h"
#include "../Proxy/ResourceProxy.h"

Resource::Resource(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource)
	: m_pResource(ExistingID3D12Resource)
{
	const D3D12_RESOURCE_DESC desc = m_pResource->GetDesc();
	bool isArray = (desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE1D || desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D) && desc.DepthOrArraySize > 1;
	ID3D12Device* pDevice = nullptr;
	ExistingID3D12Resource->GetDevice(IID_PPV_ARGS(&pDevice));
	const D3D12_RESOURCE_ALLOCATION_INFO allocInfo = pDevice->GetResourceAllocationInfo(0, 1, &desc);
	pDevice->Release();

	switch (desc.Dimension)
	{
	case D3D12_RESOURCE_DIMENSION_TEXTURE1D: m_Type = Resource::Type::Texture1D; break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE2D: m_Type = Resource::Type::Texture2D; break;
	case D3D12_RESOURCE_DIMENSION_TEXTURE3D: m_Type = Resource::Type::Texture3D; break;
	default: m_Type = Resource::Type::Unknown; break;
	}
	m_BindFlags = {};
	m_NumSubresources = isArray ? desc.DepthOrArraySize * desc.MipLevels : desc.MipLevels;
	m_MemoryRequested = desc.Width;
	m_SizeInBytes = allocInfo.SizeInBytes;
	m_Alignment = allocInfo.Alignment;
	m_HeapOffset = 0;
}

Resource::Resource(const Device* pDevice, ResourceProxy& Proxy)
{
	Proxy.Link();

	const D3D12_HEAP_PROPERTIES prop = Proxy.BuildD3DHeapProperties();
	const D3D12_HEAP_FLAGS flag = D3D12_HEAP_FLAG_NONE;
	const D3D12_RESOURCE_DESC desc = Proxy.BuildD3DDesc();
	const D3D12_RESOURCE_STATES state = GetD3DResourceStates(Proxy.InitialState);
	const D3D12_CLEAR_VALUE* clearValue = Proxy.m_ClearValue.has_value() ? &(Proxy.m_ClearValue.value()) : nullptr;
	const D3D12_RESOURCE_ALLOCATION_INFO allocInfo = pDevice->GetD3DDevice()->GetResourceAllocationInfo(0, 1, &desc);

	m_Type = Proxy.m_Type;
	m_BindFlags = Proxy.BindFlags;
	m_NumSubresources = Proxy.m_NumSubresources;
	m_MemoryRequested = desc.Width;
	m_SizeInBytes = allocInfo.SizeInBytes;
	m_Alignment = allocInfo.Alignment;
	m_HeapOffset = 0;
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateCommittedResource(
		&prop,
		flag,
		&desc,
		state,
		clearValue,
		IID_PPV_ARGS(&m_pResource)));
}

Resource::Resource(const Device* pDevice, const Heap* pHeap, UINT64 HeapOffset, ResourceProxy& Proxy)
{
	Proxy.Link();

	const D3D12_RESOURCE_DESC desc = Proxy.BuildD3DDesc();
	D3D12_RESOURCE_STATES state = GetD3DResourceStates(Proxy.InitialState);
	const D3D12_CLEAR_VALUE* clearValue = Proxy.m_ClearValue.has_value() ? &(Proxy.m_ClearValue.value()) : nullptr;
	const D3D12_RESOURCE_ALLOCATION_INFO allocInfo = pDevice->GetD3DDevice()->GetResourceAllocationInfo(0, 1, &desc);

	m_Type = Proxy.m_Type;
	m_BindFlags = Proxy.BindFlags;
	m_NumSubresources = Proxy.m_NumSubresources;
	m_MemoryRequested = desc.Width;
	m_SizeInBytes = allocInfo.SizeInBytes;
	m_Alignment = allocInfo.Alignment;
	m_HeapOffset = HeapOffset;
	switch (pHeap->GetType())
	{
	case Heap::Type::Upload: state = D3D12_RESOURCE_STATE_GENERIC_READ; break;
	case Heap::Type::Readback: state = D3D12_RESOURCE_STATE_COPY_DEST; break;
	}

	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreatePlacedResource(
		pHeap->GetD3DHeap(),
		HeapOffset,
		&desc,
		state,
		clearValue,
		IID_PPV_ARGS(&m_pResource)));
}

Resource::~Resource()
{
}

void Resource::Release()
{
	m_pResource.Reset();
	m_Type = Resource::Type::Unknown;
	m_BindFlags = BindFlags::None;
}

D3D12_RESOURCE_DIMENSION GetD3DResourceDimension(Resource::Type Type)
{
	switch (Type)
	{
	case Resource::Type::Buffer: return D3D12_RESOURCE_DIMENSION_BUFFER;
	case Resource::Type::Texture1D: return D3D12_RESOURCE_DIMENSION_TEXTURE1D;
	case Resource::Type::Texture2D: return D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	case Resource::Type::Texture3D: return D3D12_RESOURCE_DIMENSION_TEXTURE3D;
	default: return D3D12_RESOURCE_DIMENSION_UNKNOWN;
	}
}

D3D12_RESOURCE_FLAGS GetD3DResourceFlags(Resource::BindFlags Flags)
{
	D3D12_RESOURCE_FLAGS ret = D3D12_RESOURCE_FLAG_NONE;

	if (EnumMaskBitSet(Flags, Resource::BindFlags::RenderTarget)) ret |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
	if (EnumMaskBitSet(Flags, Resource::BindFlags::DepthStencil)) ret |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	if (EnumMaskBitSet(Flags, Resource::BindFlags::UnorderedAccess) ||
		EnumMaskBitSet(Flags, Resource::BindFlags::AccelerationStructure)) // Acceleration structure must have UAV access
		ret |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	return ret;
}

D3D12_RESOURCE_STATES GetD3DResourceStates(Resource::State State)
{
	D3D12_RESOURCE_STATES ret = D3D12_RESOURCE_STATE_COMMON;
	if (EnumMaskBitSet(State, Resource::State::Unknown) ||
		EnumMaskBitSet(State, Resource::State::Common))
		ret |= D3D12_RESOURCE_STATE_COMMON;

	if (EnumMaskBitSet(State, Resource::State::VertexBuffer) ||
		EnumMaskBitSet(State, Resource::State::ConstantBuffer))
		ret |= D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;

	if (EnumMaskBitSet(State, Resource::State::IndexBuffer)) ret |= D3D12_RESOURCE_STATE_INDEX_BUFFER;
	if (EnumMaskBitSet(State, Resource::State::RenderTarget)) ret |= D3D12_RESOURCE_STATE_RENDER_TARGET;
	if (EnumMaskBitSet(State, Resource::State::UnorderedAccess)) ret |= D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
	if (EnumMaskBitSet(State, Resource::State::DepthWrite)) ret |= D3D12_RESOURCE_STATE_DEPTH_WRITE;
	if (EnumMaskBitSet(State, Resource::State::DepthRead)) ret |= D3D12_RESOURCE_STATE_DEPTH_READ;
	if (EnumMaskBitSet(State, Resource::State::NonPixelShaderResource)) ret |= D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
	if (EnumMaskBitSet(State, Resource::State::PixelShaderResource)) ret |= D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	if (EnumMaskBitSet(State, Resource::State::StreamOut)) ret |= D3D12_RESOURCE_STATE_STREAM_OUT;
	if (EnumMaskBitSet(State, Resource::State::IndirectArgument)) ret |= D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT;
	if (EnumMaskBitSet(State, Resource::State::CopyDest)) ret |= D3D12_RESOURCE_STATE_COPY_DEST;
	if (EnumMaskBitSet(State, Resource::State::CopySource)) ret |= D3D12_RESOURCE_STATE_COPY_SOURCE;
	if (EnumMaskBitSet(State, Resource::State::ResolveDest)) ret |= D3D12_RESOURCE_STATE_RESOLVE_DEST;
	if (EnumMaskBitSet(State, Resource::State::ResolveSource)) ret |= D3D12_RESOURCE_STATE_RESOLVE_SOURCE;
	if (EnumMaskBitSet(State, Resource::State::AccelerationStructure)) ret |= D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE;
	if (EnumMaskBitSet(State, Resource::State::ShadingRateSource)) ret |= D3D12_RESOURCE_STATE_SHADING_RATE_SOURCE;
	if (EnumMaskBitSet(State, Resource::State::GenericRead)) ret |= D3D12_RESOURCE_STATE_GENERIC_READ;
	if (EnumMaskBitSet(State, Resource::State::Present)) ret |= D3D12_RESOURCE_STATE_PRESENT;
	if (EnumMaskBitSet(State, Resource::State::Predication)) ret |= D3D12_RESOURCE_STATE_PREDICATION;

	return ret;
}