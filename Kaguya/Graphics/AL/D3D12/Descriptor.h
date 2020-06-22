#pragma once
#include <d3d12.h>

class DescriptorHeap;

class Descriptor
{
public:
	Descriptor();
	Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle, UINT NumDescriptors, UINT DescriptorIncrementSize, DescriptorHeap* pDescriptorHeap);
	~Descriptor();

	D3D12_CPU_DESCRIPTOR_HANDLE operator[](INT Index) const;
	operator bool() const;
	UINT NumDescriptors() const;
private:
	friend class DescriptorAllocator;

	D3D12_CPU_DESCRIPTOR_HANDLE m_CPUDescriptorHandle;
	UINT m_NumDescriptors;
	UINT m_DescriptorIncrementSize;
	DescriptorHeap* m_pDescriptorHeap;
};