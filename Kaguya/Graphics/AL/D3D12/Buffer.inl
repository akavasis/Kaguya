template<typename T>
inline void Buffer::Update(INT ElementIndex, const T& Data)
{
	assert(m_pMappedData && "Map() has not been called, invalid ptr");
	memcpy(&m_pMappedData[ElementIndex * m_Stride], &Data, m_Stride);
}