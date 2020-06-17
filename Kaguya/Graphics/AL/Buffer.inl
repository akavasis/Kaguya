Buffer::Buffer(const Device* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties)
	: Resource(pDevice, Resource::Properties::Buffer(Properties.NumElements* Properties.Stride, Properties.Flags), Properties.InitialState)
{
}

template<typename T, bool UseConstantBufferAlignment>
Buffer::Buffer(const Device* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties, CPUAccessibleHeapType CPUAccessibleHeapType)
	: Resource(pDevice, Resource::Properties::Buffer(Properties.NumElements* Properties.Stride, Properties.Flags), CPUAccessibleHeapType)
{
}

template<typename T, bool UseConstantBufferAlignment>
Buffer::Buffer(const Device* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties, const Heap* pHeap, UINT64 HeapOffset)
	: Resource(pDevice, Resource::Properties::Buffer(Properties.NumElements* Properties.Stride, Properties.Flags), pHeap, HeapOffset, Properties.InitialState)
{
}