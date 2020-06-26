template<typename T, bool UseConstantBufferAlignment>
Buffer::Buffer(const Device* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties)
	: Resource(pDevice, Resource::Properties::Buffer(UINT64(INT64(Properties.NumElements) * INT64(Properties.Stride)), Properties.Flags), Properties.InitialState),
	m_NumElements(Properties.NumElements),
	m_Stride(Properties.Stride)
{
}

template<typename T, bool UseConstantBufferAlignment>
Buffer::Buffer(const Device* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties, CPUAccessibleHeapType CPUAccessibleHeapType, const void* pData)
	: Resource(pDevice, Resource::Properties::Buffer(UINT64(INT64(Properties.NumElements)* INT64(Properties.Stride)), Properties.Flags), CPUAccessibleHeapType),
	m_NumElements(Properties.NumElements),
	m_Stride(Properties.Stride),
	m_CPUAccessibleHeapType(CPUAccessibleHeapType)
{
	if (CPUAccessibleHeapType == CPUAccessibleHeapType::Upload && pData)
	{
		Map();
		memcpy(m_pMappedData, pData, UINT64(INT64(m_NumElements) * INT64(m_Stride)));
		Unmap();
	}
}

template<typename T, bool UseConstantBufferAlignment>
Buffer::Buffer(const Device* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties, const Heap* pHeap, UINT64 HeapOffset)
	: Resource(pDevice, Resource::Properties::Buffer(Properties.NumElements* Properties.Stride, Properties.Flags), pHeap, HeapOffset, Properties.InitialState),
	m_NumElements(Properties.NumElements),
	m_Stride(Properties.Stride),
	m_CPUAccessibleHeapType(pHeap->GetCPUAccessibleHeapType())
{
}

template<typename T>
inline void Buffer::Update(UINT ElementIndex, const T& Data)
{
	assert(m_pMappedData != nullptr && ElementIndex >= 0 && ElementIndex < m_NumElements && "ElementIndex out of bound");
	memcpy(&m_pMappedData[ElementIndex * m_Stride], &Data, m_Stride);
}