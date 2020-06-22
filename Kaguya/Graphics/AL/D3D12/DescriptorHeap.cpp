#include "pch.h"
#include "DescriptorHeap.h"
#include "Device.h"

DescriptorHeap::DescriptorHeap(Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT NumDescriptors)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc;
	desc.Type = Type;
	desc.NumDescriptors = NumDescriptors;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pDescriptorHeap)));
	m_BaseDescriptorHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	m_NumFreeHandles = NumDescriptors;
	AllocateFreeBlock(0, m_NumFreeHandles);
}

std::optional<Descriptor> DescriptorHeap::Allocate(UINT NumDescriptors, UINT DescriptorIncrementSize)
{
	if (m_NumFreeHandles < NumDescriptors) return std::nullopt;

	// Get a block in the pool that is large enough to satisfy the required descriptor,
	// The function returns an iterator pointing to the key in the map container which is equivalent to k passed in the parameter. 
	// In case k is not present in the map container, the function returns an iterator pointing to the immediate next element which is just greater than k.
	auto iter = m_SizePool.lower_bound(NumDescriptors);
	if (iter == m_SizePool.end()) return std::nullopt;

	// Query all information about this available block
	Size size = iter->first;
	FreeBlocksByOffsetPoolIter offsetIter{ iter->second };
	Offset offset = offsetIter.Iterator->first;

	// Remove the block from the pool
	m_SizePool.erase(size);
	m_FreeBlocksByOffsetPool.erase(offsetIter.Iterator);

	// Allocate a available block
	Offset newOffset = offset + NumDescriptors;
	Size newSize = size - NumDescriptors;

	// Return left over block to the pool
	if (newSize > 0)
	{
		AllocateFreeBlock(newOffset, newSize);
	}

	m_NumFreeHandles -= NumDescriptors;

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_BaseDescriptorHandle, offset, DescriptorIncrementSize);
	return Descriptor(handle, NumDescriptors, DescriptorIncrementSize, this);
}

void DescriptorHeap::Free(const Descriptor& Descriptor, UINT DescriptorIncrementSize)
{
	Offset offset = (Descriptor[0].ptr - m_BaseDescriptorHandle.ptr) / DescriptorIncrementSize;
	Size size = Descriptor.NumDescriptors();

	FreeBlock(offset, size);
}

void DescriptorHeap::AllocateFreeBlock(Offset offset, Size size)
{
	auto offsetElement = m_FreeBlocksByOffsetPool.emplace(offset, m_SizePool.begin());
	auto sizeElement = m_SizePool.emplace(size, offsetElement.first);
	offsetElement.first->second.Iterator = sizeElement;
}

void DescriptorHeap::FreeBlock(Offset offset, Size size)
{
	// upper_bound returns end if it there is no next block
	auto nextIter = m_FreeBlocksByOffsetPool.upper_bound(offset);
	auto prevIter = nextIter;
	if (prevIter != m_FreeBlocksByOffsetPool.begin()) // there is no free block in front of this one
		--prevIter;
	else
		prevIter = m_FreeBlocksByOffsetPool.end();

	m_NumFreeHandles += size;

	if (prevIter != m_FreeBlocksByOffsetPool.end())
	{
		Offset prevOffset = prevIter->first;
		Size prevSize = prevIter->second.Iterator->first;
		if (offset == prevOffset + prevSize)
		{
			// Merge previous and current block
			// PrevBlock.Offset					CurrentBlock.Offset
			// |								|
			// |<--------PrevBlock.Size-------->|<--------CurrentBlock.Size-------->|

			offset = prevOffset;
			size += prevSize;

			// Remove this block now that it has been merged
			// Erase order needs to be size the offset, because offset iter still references element in the offset pool
			m_SizePool.erase(prevIter->second.Iterator);
			m_FreeBlocksByOffsetPool.erase(prevOffset);
		}
	}

	if (nextIter != m_FreeBlocksByOffsetPool.end())
	{
		Offset nextOffset = nextIter->first;
		Size nextSize = nextIter->second.Iterator->first;
		if (offset + size == nextOffset)
		{
			// Merge current and next block
			// CurrentBlock.Offset				   NextBlock.Offset           
			// |								   |                          
			// |<--------CurrentBlock.Size-------->|<--------NextBlock.Size-------->

			// only increase the size of the block
			size += nextSize;

			// Remove this block now that it has been merged
			// Erase order needs to be size the offset, because offset iter still references element in the offset pool
			m_SizePool.erase(nextIter->second.Iterator);
			m_FreeBlocksByOffsetPool.erase(nextOffset);
		}
	}

	AllocateFreeBlock(offset, size);
}