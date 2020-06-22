#include "pch.h"
#include "DescriptorAllocator.h"
#include "Device.h"

DescriptorAllocator::DescriptorAllocator(Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE Type)
	: m_pDevice(pDevice),
	m_Type(Type)
{
}

DescriptorAllocator::~DescriptorAllocator()
{
}

Descriptor DescriptorAllocator::Allocate(UINT NumDescriptors)
{
	Descriptor descriptor;

	for (auto iter = m_DescriptorHeaps.begin(); iter != m_DescriptorHeaps.end(); ++iter)
	{
		auto optDescriptor = iter->get()->Allocate(NumDescriptors, m_pDevice->GetDescriptorIncrementSize(m_Type));
		if (optDescriptor.has_value())
		{
			descriptor = optDescriptor.value();
			break;
		}
	}

	if (!descriptor)
	{
		auto pDescriptorHeap = std::make_unique<DescriptorHeap>(m_pDevice, m_Type, NumDescriptorsPerHeap);
		descriptor = pDescriptorHeap->Allocate(NumDescriptors, m_pDevice->GetDescriptorIncrementSize(m_Type)).value();
		m_DescriptorHeaps.push_back(std::move(pDescriptorHeap));
	}

	return descriptor;
}

void DescriptorAllocator::Free(const Descriptor& Descriptor)
{
	if (!Descriptor) return;
	Descriptor.m_pDescriptorHeap->Free(Descriptor, m_pDevice->GetDescriptorIncrementSize(m_Type));
}