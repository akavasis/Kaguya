#pragma once
#include <map>
#include <optional>

class VariableSizedAllocator
{
public:
	using OffsetType = std::size_t;
	using SizeType = std::size_t;

	VariableSizedAllocator(SizeType Size);
	~VariableSizedAllocator();

	inline auto GetCurrentSize() const { return m_AvailableSize; }

	// Return val: OffsetType: Where the new offset is, SizeType the Size that was passed in the function parameter
	std::optional<std::pair<OffsetType, SizeType>> Allocate(SizeType Size);
	void Free(OffsetType Offset, SizeType Size);
	void Reset();
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
	const SizeType m_Size;
	SizeType m_AvailableSize;
};