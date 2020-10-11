template<typename T, size_t Size>
inline Pool<T, Size>::Pool()
{
	Reset();
}

template<typename T, size_t Size>
inline Pool<T, Size>::~Pool()
{
}

template<typename T, size_t Size>
inline typename Pool<T, Size>::Element& Pool<T, Size>::operator[](size_t Index)
{
	return Elements[Index];
}

template<typename T, size_t Size>
inline typename const Pool<T, Size>::Element& Pool<T, Size>::operator[](size_t Index) const
{
	return Elements[Index];
}

template<typename T, size_t Size>
inline size_t Pool<T, Size>::Allocate()
{
	assert(NumActiveElements < Size && "Consider increasing the size of the pool");
	NumActiveElements++;
	size_t index = FreeStart;
	FreeStart = Elements[index].Next;
	return index;
}

template<typename T, size_t Size>
inline void Pool<T, Size>::Free(size_t Index)
{
	NumActiveElements--;
	Elements[Index].Next = FreeStart;
	FreeStart = Index;
}

template<typename T, size_t Size>
inline void Pool<T, Size>::Reset()
{
	FreeStart = 0;
	NumActiveElements = 0;
	for (size_t i = 0; i < Size; ++i)
	{
		Elements[i].Next = i + 1;
	}
}