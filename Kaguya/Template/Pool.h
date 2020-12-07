#pragma once
#include <array>
#include <cassert>

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

	Pool();
	~Pool();

	Element& operator[](size_t Index);
	const Element& operator[](size_t Index) const;

	// Removes the first element from the free list and returns its index
	// throws if no free elements remain
	size_t Allocate();
	void Free(size_t Index);

	void Reset();
private:
	std::array<Element, Size> Elements;
	size_t FreeStart;
	size_t NumActiveElements;
};

#include "Pool.inl"