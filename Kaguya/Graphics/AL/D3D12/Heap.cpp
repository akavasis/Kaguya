#include "pch.h"
#include "Heap.h"
#include "Device.h"
#include "../Proxy/HeapProxy.h"

const D3D12_HEAP_PROPERTIES kDefaultHeapProps =
{
	.Type = D3D12_HEAP_TYPE_DEFAULT,
	.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask = 0,
	.VisibleNodeMask = 0
};

const D3D12_HEAP_PROPERTIES kUploadHeapProps =
{
	.Type = D3D12_HEAP_TYPE_UPLOAD,
	.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask = 0,
	.VisibleNodeMask = 0,
};

const D3D12_HEAP_PROPERTIES kReadbackHeapProps =
{
	.Type = D3D12_HEAP_TYPE_READBACK,
	.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN,
	.CreationNodeMask = 0,
	.VisibleNodeMask = 0
};

Heap::Heap(const Device* pDevice, HeapProxy& Proxy)
	: m_SizeInBytes(Math::AlignUp<UINT64>(Proxy.SizeInBytes, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT)),
	m_Type(Proxy.Type),
	m_Flags(Proxy.Flags)
{
	Proxy.Link();

	D3D12_HEAP_DESC desc = Proxy.BuildD3DDesc();
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateHeap(&desc, IID_PPV_ARGS(&m_pHeap)));
}

D3D12_HEAP_FLAGS GetD3DHeapFlags(Heap::Flags Flags)
{
	D3D12_HEAP_FLAGS ret = D3D12_HEAP_FLAG_NONE;

	if (EnumMaskBitSet(Flags, Heap::Flags::Buffers)) ret |= D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS;
	if (EnumMaskBitSet(Flags, Heap::Flags::NonRTDSTextures)) ret |= D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES;
	if (EnumMaskBitSet(Flags, Heap::Flags::RTDSTextures)) ret |= D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES;
	if (EnumMaskBitSet(Flags, Heap::Flags::AllResources)) ret |= D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES;

	return ret;
}