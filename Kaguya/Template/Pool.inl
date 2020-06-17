template<typename T, size_t Size>
inline Pool<T, Size>::Pool()
	: m_FreeStart(0),
	m_NumActiveElements(0)
{
	for (size_t i = 0; i < Size; ++i)
	{
		m_Elements[i].Next = i + 1;
	}
}

template<typename T, size_t Size>
inline Pool<T, Size>::~Pool()
{
}

template<typename T, size_t Size>
inline typename Pool<T, Size>::Element& Pool<T, Size>::operator[](size_t Index)
{
	return m_Elements[Index];
}

template<typename T, size_t Size>
inline typename const Pool<T, Size>::Element& Pool<T, Size>::operator[](size_t Index) const
{
	return m_Elements[Index];
}

template<typename T, size_t Size>
inline size_t Pool<T, Size>::Allocate()
{
	if (m_NumActiveElements == Size)
		throw std::exception("Consider increasing the size of the pool");
	m_NumActiveElements++;
	size_t index = m_FreeStart;
	m_FreeStart = m_Elements[index].Next;
	return index;
}

template<typename T, size_t Size>
inline void Pool<T, Size>::Free(size_t Index)
{
	m_NumActiveElements--;
	m_Elements[Index].Next = m_FreeStart;
	m_FreeStart = Index;
}