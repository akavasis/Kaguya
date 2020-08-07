#include "pch.h"
#include "DescriptorAllocator.h"
#include "Device.h"

DescriptorAllocator::DescriptorAllocator(Device* pDevice)
	: m_pDevice(pDevice),
	m_CBSRUADescriptorHeap(pDevice, NumDescriptorsPerRange, NumDescriptorsPerRange, NumDescriptorsPerRange, true),
	m_SamplerDescriptorHeap(pDevice, { NumDescriptorsPerRange }, true),
	m_RenderTargetDescriptorHeap(pDevice, { NumDescriptorsPerRange }),
	m_DepthStencilDescriptorHeap(pDevice, { NumDescriptorsPerRange })
{
}

DescriptorAllocation DescriptorAllocator::AllocateCBDescriptors(UINT NumDescriptors)
{
	auto ret = m_CBSRUADescriptorHeap.AllocateCBDescriptors(NumDescriptors).value_or(DescriptorAllocation());
	if (!ret)
	{
		CORE_ERROR("{} Failed, consider increasing NumDescriptorsPerRange", __FUNCTION__);
	}
	return ret;
}

DescriptorAllocation DescriptorAllocator::AllocateSRDescriptors(UINT NumDescriptors)
{
	auto ret = m_CBSRUADescriptorHeap.AllocateSRDescriptors(NumDescriptors).value_or(DescriptorAllocation());
	if (!ret)
	{
		CORE_ERROR("{} Failed, consider increasing NumDescriptorsPerRange", __FUNCTION__);
	}
	return ret;
}

DescriptorAllocation DescriptorAllocator::AllocateUADescriptors(UINT NumDescriptors)
{
	auto ret = m_CBSRUADescriptorHeap.AllocateUADescriptors(NumDescriptors).value_or(DescriptorAllocation());
	if (!ret)
	{
		CORE_ERROR("{} Failed, consider increasing NumDescriptorsPerRange", __FUNCTION__);
	}
	return ret;
}

DescriptorAllocation DescriptorAllocator::AllocateSamplerDescriptors(UINT NumDescriptors)
{
	auto ret = m_SamplerDescriptorHeap.Allocate(0, NumDescriptors).value_or(DescriptorAllocation());
	if (!ret)
	{
		CORE_ERROR("{} Failed, consider increasing NumDescriptorsPerRange", __FUNCTION__);
	}
	return ret;
}

DescriptorAllocation DescriptorAllocator::AllocateRenderTargetDescriptors(UINT NumDescriptors)
{
	auto ret = m_RenderTargetDescriptorHeap.Allocate(0, NumDescriptors).value_or(DescriptorAllocation());
	if (!ret)
	{
		CORE_ERROR("{} Failed, consider increasing NumDescriptorsPerRange", __FUNCTION__);
	}
	return ret;
}

DescriptorAllocation DescriptorAllocator::AllocateDepthStencilDescriptors(UINT NumDescriptors)
{
	auto ret = m_DepthStencilDescriptorHeap.Allocate(0, NumDescriptors).value_or(DescriptorAllocation());
	if (!ret)
	{
		CORE_ERROR("{} Failed, consider increasing NumDescriptorsPerRange", __FUNCTION__);
	}
	return ret;
}

void DescriptorAllocator::FreeCBDescriptors(DescriptorAllocation* pDescriptorAllocation)
{
	if (!pDescriptorAllocation)
		return;
	pDescriptorAllocation->pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ConstantBuffer, pDescriptorAllocation);
}

void DescriptorAllocator::FreeSRDescriptors(DescriptorAllocation* pDescriptorAllocation)
{
	if (!pDescriptorAllocation)
		return;
	pDescriptorAllocation->pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ShaderResource, pDescriptorAllocation);
}

void DescriptorAllocator::FreeUADescriptors(DescriptorAllocation* pDescriptorAllocation)
{
	if (!pDescriptorAllocation)
		return;
	pDescriptorAllocation->pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::UnorderedAccess, pDescriptorAllocation);
}

void DescriptorAllocator::FreeSamplerDescriptors(DescriptorAllocation* pDescriptorAllocation)
{
	if (!pDescriptorAllocation)
		return;
	pDescriptorAllocation->pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ConstantBuffer, pDescriptorAllocation);
}

void DescriptorAllocator::FreeRenderTargetDescriptors(DescriptorAllocation* pDescriptorAllocation)
{
	if (!pDescriptorAllocation)
		return;
	pDescriptorAllocation->pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ConstantBuffer, pDescriptorAllocation);
}

void DescriptorAllocator::FreeDepthStencilDescriptors(DescriptorAllocation* pDescriptorAllocation)
{
	if (!pDescriptorAllocation)
		return;
	pDescriptorAllocation->pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ConstantBuffer, pDescriptorAllocation);
}