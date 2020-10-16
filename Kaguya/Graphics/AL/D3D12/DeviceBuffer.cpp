#include "pch.h"
#include "DeviceBuffer.h"
#include "Device.h"
#include "../Proxy/DeviceBufferProxy.h"

DeviceBuffer::DeviceBuffer(const Device* pDevice, DeviceBufferProxy& Proxy)
	: DeviceResource(pDevice, Proxy),
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

DeviceBuffer::DeviceBuffer(const Device* pDevice, const Heap* pHeap, UINT64 HeapOffset, DeviceBufferProxy& Proxy)
	: DeviceResource(pDevice, pHeap, HeapOffset, Proxy),
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

DeviceBuffer::~DeviceBuffer()
{
	Unmap();
}

BYTE* DeviceBuffer::Map()
{
	if (!m_pMappedData)
	{
		ThrowCOMIfFailed(m_pResource->Map(0, nullptr, (void**)&m_pMappedData));
	}

	return m_pMappedData;
}

void DeviceBuffer::Unmap()
{
	if (!m_pMappedData) 
		return;

	m_pResource->Unmap(0, nullptr);
	m_pMappedData = nullptr;
}

D3D12_GPU_VIRTUAL_ADDRESS DeviceBuffer::GetGpuVirtualAddress() const
{
	return m_pResource->GetGPUVirtualAddress();
}

D3D12_GPU_VIRTUAL_ADDRESS DeviceBuffer::GetGpuVirtualAddressAt(INT Index) const
{
	assert(m_Stride != 0 && "Stride should not be 0");
	assert(Index >= 0 && Index < (GetSizeInBytes() / m_Stride) && "Index is out of range in Buffer");
	return UINT64(INT64(m_pResource->GetGPUVirtualAddress()) + INT64(Index) * INT64(m_Stride));
}