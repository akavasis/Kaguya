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

D3D12_GPU_VIRTUAL_ADDRESS Buffer::GetBufferLocationAt(INT Index)
{
	assert(Index >= 0 && Index < m_NumElements && "Index is out of range in Buffer");
	return UINT64(INT64(m_pResource->GetGPUVirtualAddress()) + INT64(Index) * INT64(m_Stride));
}