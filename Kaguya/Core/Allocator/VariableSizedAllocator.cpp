#include "pch.h"
#include "VariableSizedAllocator.h"

VariableSizedAllocator::VariableSizedAllocator(SizeType Size)
	: m_Size(Size),
	m_AvailableSize(Size)
{
	AllocateFreeBlock(0, Size);
}

VariableSizedAllocator::~VariableSizedAllocator()
{
}

std::optional<std::pair<VariableSizedAllocator::OffsetType, VariableSizedAllocator::SizeType>> VariableSizedAllocator::Allocate(SizeType Size)
{
	if (m_AvailableSize < Size)
		return std::nullopt;

	// Get a block in the pool that is large enough to satisfy the required Size,
	// The function returns an iterator pointing to the key in the map container which is equivalent to k passed in the parameter. 
	// In case k is not present in the map container, the function returns an iterator pointing to the immediate next element which is just greater than k.
	auto iter = m_FreeBlocksBySizePool.lower_bound(Size);
	if (iter == m_FreeBlocksBySizePool.end())
		return std::nullopt;

	// Query all information about this available block
	SizeType size = iter->first;
	FreeBlocksByOffsetPoolIter offsetIter(iter->second);
	OffsetType offset = offsetIter.Iterator->first;

	// Remove the block from the pool
	m_FreeBlocksBySizePool.erase(size);
	m_FreeBlocksByOffsetPool.erase(offsetIter.Iterator);

	// Allocate a available block
	OffsetType newOffset = offset + Size;
	SizeType newSize = size - Size;

	// Return left over block to the pool
	if (newSize > 0)
	{
		AllocateFreeBlock(newOffset, newSize);
	}

	m_AvailableSize -= Size;
	return std::make_pair(offset, Size);
}

void VariableSizedAllocator::Free(OffsetType Offset, SizeType Size)
{
	// upper_bound returns end if it there is no next block
	auto nextIter = m_FreeBlocksByOffsetPool.upper_bound(Offset);
	auto prevIter = nextIter;
	if (prevIter != m_FreeBlocksByOffsetPool.begin()) // there is no free block in front of this one
		--prevIter;
	else
		prevIter = m_FreeBlocksByOffsetPool.end();

	m_AvailableSize += Size;

	if (prevIter != m_FreeBlocksByOffsetPool.end())
	{
		OffsetType prevOffset = prevIter->first;
		SizeType prevSize = prevIter->second.Iterator->first;
		if (Offset == prevOffset + prevSize)
		{
			// Merge previous and current block
			// PrevBlock.Offset					CurrentBlock.Offset
			// |								|
			// |<--------PrevBlock.Size-------->|<--------CurrentBlock.Size-------->|

			Offset = prevOffset;
			Size += prevSize;

			// Remove this block now that it has been merged
			// Erase order needs to be size the offset, because offset iter still references element in the offset pool
			m_FreeBlocksBySizePool.erase(prevIter->second.Iterator);
			m_FreeBlocksByOffsetPool.erase(prevOffset);
		}
	}

	if (nextIter != m_FreeBlocksByOffsetPool.end())
	{
		OffsetType nextOffset = nextIter->first;
		SizeType nextSize = nextIter->second.Iterator->first;
		if (Offset + Size == nextOffset)
		{
			// Merge current and next block
			// CurrentBlock.Offset				   NextBlock.Offset           
			// |								   |                          
			// |<--------CurrentBlock.Size-------->|<--------NextBlock.Size-------->

			// only increase the size of the block
			Size += nextSize;

			// Remove this block now that it has been merged
			// Erase order needs to be size the offset, because offset iter still references element in the offset pool
			m_FreeBlocksBySizePool.erase(nextIter->second.Iterator);
			m_FreeBlocksByOffsetPool.erase(nextOffset);
		}
	}

	AllocateFreeBlock(Offset, Size);
}

void VariableSizedAllocator::Reset()
{
	m_FreeBlocksByOffsetPool.clear();
	m_FreeBlocksBySizePool.clear();
	m_AvailableSize = m_Size;
	AllocateFreeBlock(0, m_AvailableSize);
}

void VariableSizedAllocator::AllocateFreeBlock(OffsetType offset, SizeType size)
{
	auto offsetElement = m_FreeBlocksByOffsetPool.emplace(offset, m_FreeBlocksBySizePool.begin());
	auto sizeElement = m_FreeBlocksBySizePool.emplace(size, offsetElement.first);
	offsetElement.first->second.Iterator = sizeElement;
}