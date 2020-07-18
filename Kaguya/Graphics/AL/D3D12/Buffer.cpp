#include "pch.h"
#include "Buffer.h"

Buffer::Buffer(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource)
	: Resource(ExistingID3D12Resource)
{
	D3D12_RESOURCE_DESC desc = m_pResource->GetDesc();
	m_SizeInBytes = desc.Width;
}

Buffer::Buffer(const Device* pDevice, const Properties& Properties)
	: Resource(pDevice, Resource::Properties::Buffer(Properties.SizeInBytes, Properties.Flags), Properties.InitialState),
	m_SizeInBytes(Properties.SizeInBytes),
	m_Stride(Properties.Stride)
{
}

Buffer::Buffer(const Device* pDevice, const Properties& Properties, CPUAccessibleHeapType CPUAccessibleHeapType, const void* pData)
	: Resource(pDevice, Resource::Properties::Buffer(Properties.SizeInBytes, Properties.Flags), CPUAccessibleHeapType),
	m_SizeInBytes(Properties.SizeInBytes),
	m_Stride(Properties.Stride),
	m_CPUAccessibleHeapType(CPUAccessibleHeapType)
{
	if (m_CPUAccessibleHeapType == CPUAccessibleHeapType::Upload && pData)
	{
		Map();
		memcpy(m_pMappedData, pData, m_SizeInBytes);
		Unmap();
	}
}

Buffer::Buffer(const Device* pDevice, const Properties& Properties, const Heap* pHeap, UINT64 HeapOffset)
	: Resource(pDevice, Resource::Properties::Buffer(Properties.SizeInBytes, Properties.Flags), Properties.InitialState, pHeap, HeapOffset),
	m_SizeInBytes(Properties.SizeInBytes),
	m_Stride(Properties.Stride),
	m_CPUAccessibleHeapType(pHeap->GetCPUAccessibleHeapType())
{
}

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

D3D12_GPU_VIRTUAL_ADDRESS Buffer::GetGpuVirtualAddress() const
{
	return m_pResource->GetGPUVirtualAddress();
}

D3D12_GPU_VIRTUAL_ADDRESS Buffer::GetGpuVirtualAddressAt(INT Index) const
{
	assert(Index >= 0 && Index < (m_SizeInBytes / m_Stride) && "Index is out of range in Buffer");
	return UINT64(INT64(m_pResource->GetGPUVirtualAddress()) + INT64(Index) * INT64(m_Stride));
}