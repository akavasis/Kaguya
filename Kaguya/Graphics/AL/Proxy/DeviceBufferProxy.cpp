#include "pch.h"
#include "DeviceBufferProxy.h"
#include "../D3D12/d3dx12.h"

DeviceBufferProxy::DeviceBufferProxy()
	: DeviceResourceProxy(Resource::Type::Buffer)
{
	m_SizeInBytes				= 0;
	m_Stride					= 0;
	m_CpuAccess					= Buffer::CpuAccess::None;
	m_pOptionalDataForUpload	= nullptr;
}

D3D12_HEAP_PROPERTIES DeviceBufferProxy::BuildD3DHeapProperties() const
{
	switch (m_CpuAccess)
	{
	case Buffer::CpuAccess::Write:	return kUploadHeapProps;
	case Buffer::CpuAccess::Read:		return kReadbackHeapProps;
	default:								return kDefaultHeapProps;
	}
}

D3D12_RESOURCE_DESC DeviceBufferProxy::BuildD3DDesc() const
{
	return CD3DX12_RESOURCE_DESC::Buffer(m_SizeInBytes, GetD3DResourceFlags(BindFlags));
}

void DeviceBufferProxy::SetSizeInBytes(UINT64 SizeInBytes)
{
	m_SizeInBytes = SizeInBytes;
}

void DeviceBufferProxy::SetStride(UINT Stride)
{
	m_Stride = Stride;
}

void DeviceBufferProxy::SetCpuAccess(Buffer::CpuAccess CpuAccess)
{
	m_CpuAccess = CpuAccess;
}

void DeviceBufferProxy::SetOptionalDataForUpload(const void* pOptionDataForUpload)
{
	m_pOptionalDataForUpload = pOptionDataForUpload;
}

void DeviceBufferProxy::Link()
{
	assert(m_SizeInBytes != 0);

	m_NumSubresources = 1;

	switch (m_CpuAccess)
	{
	case Buffer::CpuAccess::Write:	InitialState = Resource::State::GenericRead; break;
	case Buffer::CpuAccess::Read:		InitialState = Resource::State::CopyDest; break;
	}

	DeviceResourceProxy::Link();
}