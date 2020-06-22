#include "pch.h"
#include "Buffer.h"

Buffer::~Buffer()
{
	Unmap();
}

BYTE* Buffer::Map()
{
	if (!m_pMappedData)
	{
		ThrowCOMIfFailed(m_pResource->Map(0, nullptr, (void**)&m_pMappedData));
	}

	return m_pMappedData;
}

void Buffer::Unmap()
{
	if (!m_pMappedData) return;

	m_pResource->Unmap(0, nullptr);
	m_pMappedData = nullptr;
}