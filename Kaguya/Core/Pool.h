#pragma once
#include <vector>
#include <cassert>
#include <atomic>
#include "Synchronization/CriticalSection.h"

template<typename T, size_t Size>
class Pool
{
public:
	template<typename T>
	struct SElement
	{
		T Value;
		size_t Next;
	};

	template<>
	struct SElement<void>
	{
		size_t Next;
	};

	using Element = SElement<T>;

	Pool()
	{
		Elements.resize(Size);
		Reset();
	}

	auto& operator[](size_t Index)
	{
		return Elements[Index];
	}

	const auto& operator[](size_t Index) const
	{
		return Elements[Index];
	}

	void Reset()
	{
		FreeStart = 0;
		NumActiveElements = 0;
		for (size_t i = 0; i < Size; ++i)
		{
			Elements[i].Next = i + 1;
		}
	}

	// Removes the first element from the free list and returns its index
	// throws if no free elements remain
	size_t Allocate()
	{
		assert(NumActiveElements < Size && "Consider increasing the size of the pool");
		NumActiveElements++;
		size_t index = FreeStart;
		FreeStart = Elements[index].Next;
		return index;
	}

	void Free(size_t Index)
	{
		NumActiveElements--;
		Elements[Index].Next = FreeStart;
		FreeStart = Index;
	}
private:
	std::vector<Element> Elements;
	size_t FreeStart;
	size_t NumActiveElements;
};

template<typename T, size_t Size>
class ThreadSafePool
{
public:
	void Reset()
	{
		ScopedCriticalSection SCS(CriticalSection);
		Pool.Reset();
	}

	auto& operator[](size_t Index)
	{
		ScopedCriticalSection SCS(CriticalSection);
		return Pool[Index];
	}

	const auto& operator[](size_t Index) const
	{
		ScopedCriticalSection SCS(CriticalSection);
		return Pool[Index];
	}

	// Removes the first element from the free list and returns its index
	// throws if no free elements remain
	size_t Allocate()
	{
		ScopedCriticalSection SCS(CriticalSection);
		return Pool.Allocate();
	}

	void Free(size_t Index)
	{
		ScopedCriticalSection SCS(CriticalSection);
		return Pool.Free(Index);
	}
private:
	Pool<T, Size> Pool;
	CriticalSection CriticalSection;
};