#include "pch.h"
#include "HeapProxy.h"

HeapProxy::HeapProxy()
{
	SizeInBytes = 0;
	Type = Heap::Type::Default;
	Flags = Heap::Flags::None;
}

void HeapProxy::Link()
{
	assert(SizeInBytes != 0);
}

D3D12_HEAP_DESC HeapProxy::BuildD3DDesc()
{
	D3D12_HEAP_DESC Desc;

	Desc.SizeInBytes = SizeInBytes;
	switch (Type)
	{
	case Heap::Type::Default: { Desc.Properties = kDefaultHeapProps; } break;
	case Heap::Type::Upload: { Desc.Properties = kUploadHeapProps; } break;
	case Heap::Type::Readback: { Desc.Properties = kReadbackHeapProps; } break;
	}
	Desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	Desc.Flags = GetD3DHeapFlags(Flags);

	return Desc;
}