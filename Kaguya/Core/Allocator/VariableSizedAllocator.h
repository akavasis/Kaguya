#pragma once
#include <map>
#include <optional>

class VariableSizedAllocator
{
public:
	using OffsetType = std::size_t;
	using SizeType = std::size_t;

	VariableSizedAllocator() = default;
	VariableSizedAllocator(SizeType Capacity);
	~VariableSizedAllocator() = default;

	inline auto GetSize() const { return m_Capacity; }
	inline auto GetCurrentSize() const { return m_Capacity - m_AvailableSize; }

	// Return val: OffsetType: Where the new offset is, SizeType the Size that was passed in the function parameter
	std::optional<std::pair<OffsetType, SizeType>> Allocate(SizeType Size);
	void Free(OffsetType Offset, SizeType Size);
	// Reset will remove all allocation data
	void Reset(SizeType Capacity); // Reset with new capacity
	void Reset(); // Reset with current capacity
private:
	void AllocateFreeBlock(OffsetType offset, SizeType size);

	struct FreeBlocksByOffsetPoolIter;
	struct FreeBlocksBySizePoolIter;
	using FreeBlocksByOffsetPool = std::map<OffsetType, FreeBlocksBySizePoolIter>;
	using FreeBlocksBySizePool = std::multimap<SizeType, FreeBlocksByOffsetPoolIter>;

	struct FreeBlocksByOffsetPoolIter
	{
		FreeBlocksByOffsetPool::iterator Iterator;
		FreeBlocksByOffsetPoolIter(FreeBlocksByOffsetPool::iterator iter) : Iterator(iter) {}
	};

	struct FreeBlocksBySizePoolIter
	{
		FreeBlocksBySizePool::iterator Iterator;
		FreeBlocksBySizePoolIter(FreeBlocksBySizePool::iterator iter) : Iterator(iter) {}
	};

	FreeBlocksByOffsetPool m_FreeBlocksByOffsetPool;
	FreeBlocksBySizePool m_FreeBlocksBySizePool;
	SizeType m_Capacity;
	SizeType m_AvailableSize;
};