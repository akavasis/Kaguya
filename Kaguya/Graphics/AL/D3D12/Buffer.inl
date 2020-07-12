template<typename T>
inline void Buffer::Update(INT ElementIndex, const T& Data)
{
	memcpy(&m_pMappedData[ElementIndex * m_Stride], &Data, m_Stride);
}