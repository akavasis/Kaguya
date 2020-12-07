#pragma once
#include "API/D3D12/Buffer.h"
#include "API/D3D12/DescriptorHeap.h"

class RenderBuffer
{
public:
	Buffer* pBuffer;
	Descriptor ShaderResourceView;
	Descriptor UnorderedAccessView;
};