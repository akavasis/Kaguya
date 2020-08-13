#include "pch.h"
#include "Buffer.h"
#include "Device.h"
#include "../Proxy/BufferProxy.h"

Buffer::Buffer(const Device* pDevice, BufferProxy& Proxy)
	: Resource(pDevice, Proxy),
	m_Stride(Proxy.m_Stride),
	m_CpuAccess(Proxy.m_CpuAccess),
	m_pMappedData(nullptr)
{
	if (m_CpuAccess == CpuAccess::Write && Proxy.m_pOptionalDataForUpload)
	{
		Map();
		memcpy(m_pMappedData, Proxy.m_pOptionalDataForUpload, GetMemoryRequested());
		Unmap();
	}
}

Buffer::Buffer(const Device* pDevice, const Heap* pHeap, UINT64 HeapOffset, BufferProxy& Proxy)
	: Resource(pDevice, pHeap, HeapOffset, Proxy),
	m_Stride(Proxy.m_Stride),
	m_CpuAccess(Proxy.m_CpuAccess),
	m_pMappedData(nullptr)
{
	if (m_CpuAccess == CpuAccess::Write && Proxy.m_pOptionalDataForUpload)
	{
		Map();
		memcpy(m_pMappedData, Proxy.m_pOptionalDataForUpload, GetMemoryRequested());
		Unmap();
	}
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
	if (!m_pMappedData) 
		return;

	m_pResource->Unmap(0, nullptr);
	m_pMappedData = nullptr;
}

D3D12_GPU_VIRTUAL_ADDRESS Buffer::GetGpuVirtualAddress() const
{
	return m_pResource->GetGPUVirtualAddress();
}

D3D12_GPU_VIRTUAL_ADDRESS Buffer::GetGpuVirtualAddressAt(INT Index) const
{
	assert(m_Stride != 0 && "Stride should not be 0");
	assert(Index >= 0 && Index < (GetSizeInBytes() / m_Stride) && "Index is out of range in Buffer");
	return UINT64(INT64(m_pResource->GetGPUVirtualAddress()) + INT64(Index) * INT64(m_Stride));
}