template<typename T, bool UseConstantBufferAlignment>
Buffer::Buffer(const Device* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties)
	: Resource(pDevice, Resource::Properties::Buffer(Properties.NumElements* Properties.Stride, Properties.Flags), Properties.InitialState),
	m_NumElements(Properties.NumElements),
	m_Stride(Properties.Stride)
{
}

template<typename T, bool UseConstantBufferAlignment>
Buffer::Buffer(const Device* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties, CPUAccessibleHeapType CPUAccessibleHeapType)
	: Resource(pDevice, Resource::Properties::Buffer(Properties.NumElements* Properties.Stride, Properties.Flags), CPUAccessibleHeapType),
	m_NumElements(Properties.NumElements),
	m_Stride(Properties.Stride),
	m_CPUAccessibleHeapType(CPUAccessibleHeapType)
{
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
	assert(m_pMappedData && ElementIndex >= 0 && ElementIndex < m_NumElements && "ElementIndex out of bound");
	memcpy(&m_pMappedData[ElementIndex * m_Stride], &Data, m_Stride);
}
