template<typename T>
inline void Buffer::Update(INT ElementIndex, const T& Data)
{
	assert(m_pMappedData != nullptr && ElementIndex >= 0 && ElementIndex < (m_SizeInBytes / m_Stride) && "ElementIndex out of bound");
	memcpy(&m_pMappedData[ElementIndex * m_Stride], &Data, m_Stride);
}