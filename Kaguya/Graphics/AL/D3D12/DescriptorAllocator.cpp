#include "pch.h"
#include "DescriptorAllocator.h"
#include "Device.h"

DescriptorAllocator::DescriptorAllocator(Device* pDevice)
	: m_pDevice(pDevice),
	m_CBSRUADescriptorHeap(pDevice, { NumDescriptorsPerRange, NumDescriptorsPerRange, NumDescriptorsPerRange }),
	m_SamplerDescriptorHeap(pDevice, { NumDescriptorsPerRange }),
	m_RenderTargetDescriptorHeap(pDevice, { NumDescriptorsPerRange }),
	m_DepthStencilDescriptorHeap(pDevice, { NumDescriptorsPerRange })
{
}

DescriptorAllocator::~DescriptorAllocator()
{
}

DescriptorAllocation DescriptorAllocator::AllocateCBDescriptors(UINT NumDescriptors)
{
	return m_CBSRUADescriptorHeap.AllocateCBDescriptors(NumDescriptors).value_or(DescriptorAllocation());
}

DescriptorAllocation DescriptorAllocator::AllocateSRDescriptors(UINT NumDescriptors)
{
	return m_CBSRUADescriptorHeap.AllocateSRDescriptors(NumDescriptors).value_or(DescriptorAllocation());
}

DescriptorAllocation DescriptorAllocator::AllocateUADescriptors(UINT NumDescriptors)
{
	return m_CBSRUADescriptorHeap.AllocateUADescriptors(NumDescriptors).value_or(DescriptorAllocation());
}

DescriptorAllocation DescriptorAllocator::AllocateSamplerDescriptors(UINT NumDescriptors)
{
	return m_SamplerDescriptorHeap.Allocate(0, NumDescriptors).value_or(DescriptorAllocation());
}

DescriptorAllocation DescriptorAllocator::AllocateRenderTargetDescriptors(UINT NumDescriptors)
{
	return m_RenderTargetDescriptorHeap.Allocate(0, NumDescriptors).value_or(DescriptorAllocation());
}

DescriptorAllocation DescriptorAllocator::AllocateDepthStencilDescriptors(UINT NumDescriptors)
{
	return m_DepthStencilDescriptorHeap.Allocate(0, NumDescriptors).value_or(DescriptorAllocation());
}

void DescriptorAllocator::FreeCBDescriptors(DescriptorAllocation& DescriptorAllocation)
{
	if (DescriptorAllocation.pOwningHeap != &m_CBSRUADescriptorHeap);
	{
		CORE_WARN("This descriptor allocation did not come from this heap");
	}
	DescriptorAllocation.pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ConstantBuffer, DescriptorAllocation);
}

void DescriptorAllocator::FreeSRDescriptors(DescriptorAllocation& DescriptorAllocation)
{
	if (DescriptorAllocation.pOwningHeap != &m_CBSRUADescriptorHeap);
	{
		CORE_WARN("This descriptor allocation did not come from this heap");
	}
	DescriptorAllocation.pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ShaderResource, DescriptorAllocation);
}

void DescriptorAllocator::FreeUADescriptors(DescriptorAllocation& DescriptorAllocation)
{
	if (DescriptorAllocation.pOwningHeap != &m_CBSRUADescriptorHeap);
	{
		CORE_WARN("This descriptor allocation did not come from this heap");
	}
	DescriptorAllocation.pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::UnorderedAccess, DescriptorAllocation);
}

void DescriptorAllocator::FreeSamplerDescriptors(DescriptorAllocation& DescriptorAllocation)
{
	if (DescriptorAllocation.pOwningHeap != &m_SamplerDescriptorHeap);
	{
		CORE_WARN("This descriptor allocation did not come from this heap");
	}
	DescriptorAllocation.pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ConstantBuffer, DescriptorAllocation);
}

void DescriptorAllocator::FreeRenderTargetDescriptors(DescriptorAllocation& DescriptorAllocation)
{
	if (DescriptorAllocation.pOwningHeap != &m_RenderTargetDescriptorHeap);
	{
		CORE_WARN("This descriptor allocation did not come from this heap");
	}
	DescriptorAllocation.pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ConstantBuffer, DescriptorAllocation);
}

void DescriptorAllocator::FreeDepthStencilDescriptors(DescriptorAllocation& DescriptorAllocation)
{
	if (DescriptorAllocation.pOwningHeap != &m_DepthStencilDescriptorHeap);
	{
		CORE_WARN("This descriptor allocation did not come from this heap");
	}
	DescriptorAllocation.pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ConstantBuffer, DescriptorAllocation);
}