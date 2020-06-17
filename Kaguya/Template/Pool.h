#pragma once
#include <array>

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
private:
	std::array<Element, Size> m_Elements;
	size_t m_FreeStart;
	size_t m_NumActiveElements;
};

#include "Pool.inl"