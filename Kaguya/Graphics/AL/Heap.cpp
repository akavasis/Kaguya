#include "pch.h"
#include "Heap.h"
#include "../../Math/MathLibrary.h"

Heap::Heap(const RenderDevice* pDevice, UINT64 SizeInBytes, HeapAliasingResourceCategory HeapAliasingResourceCategories, std::optional<CPUAccessibleHeapType> CPUAccessibleHeapType)
	: m_SizeInBytesAligned(Math::AlignUp<UINT64>(SizeInBytes, D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT)),
	m_HeapAliasingResourceCategories(HeapAliasingResourceCategories),
	m_CPUAccessibleHeapType(CPUAccessibleHeapType)
{
	D3D12_HEAP_DESC desc = {};
	desc.SizeInBytes = m_SizeInBytesAligned;
	if (m_CPUAccessibleHeapType.has_value())
	{
		switch (m_CPUAccessibleHeapType.value())
		{
		case CPUAccessibleHeapType::Upload: { desc.Properties.Type = D3D12_HEAP_TYPE_UPLOAD; } break;
		case CPUAccessibleHeapType::Readback: { desc.Properties.Type = D3D12_HEAP_TYPE_READBACK; } break;
		}
	}
	else
	{
		desc.Properties.Type = D3D12_HEAP_TYPE_DEFAULT;
	}
	desc.Properties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	desc.Properties.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	desc.Properties.CreationNodeMask = desc.Properties.VisibleNodeMask = 0;
	desc.Alignment = D3D12_DEFAULT_RESOURCE_PLACEMENT_ALIGNMENT;
	desc.Flags = D3D12_HEAP_FLAG_NONE;
	switch (HeapAliasingResourceCategories)
	{
	case HeapAliasingResourceCategory::Buffers: { desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_BUFFERS; } break;
	case HeapAliasingResourceCategory::NonRTDSTextures: { desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_NON_RT_DS_TEXTURES; } break;
	case HeapAliasingResourceCategory::RTDSTextures: { desc.Flags = D3D12_HEAP_FLAG_ALLOW_ONLY_RT_DS_TEXTURES; } break;
	case HeapAliasingResourceCategory::AllResources: { desc.Flags = D3D12_HEAP_FLAG_ALLOW_ALL_BUFFERS_AND_TEXTURES; } break;
	}

	pDevice->GetD3DDevice()->CreateHeap(&desc, IID_PPV_ARGS(&m_pHeap));
}

Heap::~Heap()
{
}