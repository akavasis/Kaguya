#include "pch.h"
#include "DescriptorHeap.h"
#include "Device.h"

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

DescriptorHeap::DescriptorHeap(Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT NumDescriptors)
	: m_Allocator(NumDescriptors)
{
	// If you recorded a CPU descriptor handle into the command list (render target or depth stencil) then that descriptor can be reused, 
	// if you recorded a GPU descriptor handle into the command list (everything else) then that descriptor cannot be reused
	D3D12_DESCRIPTOR_HEAP_DESC desc;
	desc.Type = Type;
	desc.NumDescriptors = NumDescriptors;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pDescriptorHeap)));
	m_BaseDescriptorHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
}

std::optional<Descriptor> DescriptorHeap::Allocate(UINT NumDescriptors, UINT DescriptorIncrementSize)
{
	auto pair = m_Allocator.Allocate(NumDescriptors);
	if (!pair)
		return std::nullopt;

	auto [offset, size] = pair.value();

	CD3DX12_CPU_DESCRIPTOR_HANDLE handle(m_BaseDescriptorHandle, offset, DescriptorIncrementSize);
	return Descriptor(handle, NumDescriptors, DescriptorIncrementSize, this);
}

void DescriptorHeap::Free(const Descriptor& Descriptor, UINT DescriptorIncrementSize)
{
	VariableSizedAllocator::OffsetType offset = (Descriptor[0].ptr - m_BaseDescriptorHandle.ptr) / DescriptorIncrementSize;
	VariableSizedAllocator::SizeType size = Descriptor.NumDescriptors();
	m_Allocator.Free(offset, size);
}