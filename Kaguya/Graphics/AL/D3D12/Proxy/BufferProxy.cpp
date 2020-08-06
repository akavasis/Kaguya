#include "pch.h"
#include "BufferProxy.h"

BufferProxy::BufferProxy()
	: ResourceProxy(Resource::Type::Buffer)
{
	m_SizeInBytes = 0;
	m_Stride = 0;
	m_CpuAccess = Buffer::CpuAccess::None;
	m_pOptionalDataForUpload = nullptr;
}

void BufferProxy::SetSizeInBytes(UINT64 SizeInBytes)
{
	m_SizeInBytes = SizeInBytes;
}

void BufferProxy::SetStride(UINT Stride)
{
	m_Stride = Stride;
}

void BufferProxy::SetCpuAccess(Buffer::CpuAccess CpuAccess)
{
	m_CpuAccess = CpuAccess;
}

void BufferProxy::SetOptionalDataForUpload(const void* pOptionDataForUpload)
{
	m_pOptionalDataForUpload = pOptionDataForUpload;
}

void BufferProxy::Link()
{
	m_NumSubresources = 1;

	switch (m_CpuAccess)
	{
	case Buffer::CpuAccess::Write: InitialState = Resource::State::GenericRead; break;
	case Buffer::CpuAccess::Read: InitialState = Resource::State::CopyDest; break;
	}

	ResourceProxy::Link();
}

D3D12_HEAP_PROPERTIES BufferProxy::BuildD3DHeapProperties() const
{
	switch (m_CpuAccess)
	{
	case Buffer::CpuAccess::Write: return kUploadHeapProps;
	case Buffer::CpuAccess::Read: return kReadbackHeapProps;
	default: return kDefaultHeapProps;
	}
}

D3D12_RESOURCE_DESC BufferProxy::BuildD3DResourceDesc() const
{
	assert(m_SizeInBytes != 0);
	return CD3DX12_RESOURCE_DESC::Buffer(m_SizeInBytes, GetD3DResourceFlags(BindFlags));
}