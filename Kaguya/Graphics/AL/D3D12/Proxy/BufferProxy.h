#pragma once
#include "ResourceProxy.h"
#include "../Buffer.h"

class BufferProxy : public ResourceProxy
{
	friend class Buffer;
public:
	BufferProxy();

	void SetSizeInBytes(UINT64 SizeInBytes);
	void SetStride(UINT Stride);
	void SetCpuAccess(Buffer::CpuAccess CpuAccess);
	void SetOptionalDataForUpload(const void* pOptionDataForUpload);
protected:
	void Link() override;
	D3D12_HEAP_PROPERTIES BuildD3DHeapProperties() const override;
	D3D12_RESOURCE_DESC BuildD3DResourceDesc() const override;
private:
	UINT64 m_SizeInBytes;					//< Default value: 0, must be set
	UINT m_Stride;							//< Default value: 0, must be set
	Buffer::CpuAccess m_CpuAccess;			//< Default value: None, optional set
	const void* m_pOptionalDataForUpload;	//< Default value: nullptr, optional set when CpuAccess is Upload
};