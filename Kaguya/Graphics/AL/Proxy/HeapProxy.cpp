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
	D3D12_HEAP_DESC desc;

	desc.SizeInBytes = SizeInBytes;
	switch (Type)
	{
	case Heap::Type::Default: { desc.Properties = kDefaultHeapProps; } break;
	case Heap::Type::Upload: { desc.Properties = kUploadHeapProps; } break;
	case Heap::Type::Readback: { desc.Properties = kReadbackHeapProps; } break;
	}
	desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	desc.Flags = GetD3DHeapFlags(Flags);

	return desc;
}