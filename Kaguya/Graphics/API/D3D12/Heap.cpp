#include "pch.h"
#include "Heap.h"

Heap::Heap(ID3D12Device* pDevice, const D3D12_HEAP_DESC& Desc)
{
	ThrowIfFailed(pDevice->CreateHeap(&Desc, IID_PPV_ARGS(m_ApiHandle.ReleaseAndGetAddressOf())));
}