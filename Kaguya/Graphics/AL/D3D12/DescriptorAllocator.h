#pragma once
#include <d3d12.h>

#include <memory>
#include <vector>

#include "DescriptorHeap.h"

class Device;

class DescriptorAllocator
{
public:
	DescriptorAllocator(Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE Type);
	~DescriptorAllocator();

	Descriptor Allocate(UINT NumDescriptors);
	void Free(const Descriptor& Descriptor);
private:
	enum : UINT { NumDescriptorsPerHeap = 2048 };
	Device* m_pDevice;

	D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
	std::vector<std::unique_ptr<DescriptorHeap>> m_DescriptorHeaps;
};