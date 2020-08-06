#include "pch.h"
#include "HeapProxy.h"

HeapProxy::HeapProxy()
{
	m_SizeInBytes = 0;
	m_Type = Heap::Type::Default;
	m_Flags = Heap::Flags::None;
}

void HeapProxy::SetSizeInBytes(UINT64 SizeInBytes)
{
	m_SizeInBytes = SizeInBytes;
}

void HeapProxy::SetType(Heap::Type Type)
{
	m_Type = Type;
}

void HeapProxy::SetFlags(Heap::Flags Flags)
{
	m_Flags = Flags;
}

void HeapProxy::Link()
{
	assert(m_SizeInBytes != 0);
}

D3D12_HEAP_DESC HeapProxy::GetD3DHeapDesc()
{
	D3D12_HEAP_DESC ret;

	ret.SizeInBytes = m_SizeInBytes;
	switch (m_Type)
	{
	case Heap::Type::Default: { ret.Properties = kDefaultHeapProps; } break;
	case Heap::Type::Upload: { ret.Properties = kUploadHeapProps; } break;
	case Heap::Type::Readback: { ret.Properties = kReadbackHeapProps; } break;
	}
	ret.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	ret.Flags = GetD3DHeapFlags(m_Flags);

	return ret;
}