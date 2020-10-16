#pragma once
#include "AL/D3D12/DeviceBuffer.h"
#include "AL/D3D12/DescriptorHeap.h"

class RenderBuffer
{
public:
	DeviceBuffer* pBuffer;
	Descriptor ShaderResourceView;
	Descriptor UnorderedAccessView;
};