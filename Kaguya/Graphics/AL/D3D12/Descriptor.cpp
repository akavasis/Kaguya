#include "pch.h"
#include "Descriptor.h"

Descriptor::Descriptor()
	: m_CPUDescriptorHandle{ 0 },
	m_NumDescriptors(0u),
	m_DescriptorIncrementSize(0u),
	m_pDescriptorHeap(nullptr)
{
}

Descriptor::Descriptor(D3D12_CPU_DESCRIPTOR_HANDLE CPUDescriptorHandle, UINT NumDescriptors, UINT DescriptorIncrementSize, DescriptorHeap* pDescriptorHeap)
	: m_CPUDescriptorHandle(CPUDescriptorHandle),
	m_NumDescriptors(NumDescriptors),
	m_DescriptorIncrementSize(DescriptorIncrementSize),
	m_pDescriptorHeap(pDescriptorHeap)
{
}

Descriptor::~Descriptor()
{
}

D3D12_CPU_DESCRIPTOR_HANDLE Descriptor::operator[](INT Index) const
{
	assert(Index >= 0 && Index < m_NumDescriptors);
	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_CPUDescriptorHandle);
	return handle.Offset(Index, m_DescriptorIncrementSize);
}

Descriptor::operator bool() const
{
	return m_CPUDescriptorHandle.ptr != 0;
}

UINT Descriptor::NumDescriptors() const
{
	return m_NumDescriptors;
}