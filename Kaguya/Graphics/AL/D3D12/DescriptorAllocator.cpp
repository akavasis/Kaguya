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
	auto ret = m_CBSRUADescriptorHeap.AllocateCBDescriptors(NumDescriptors);
	if (!ret)
	{
		CORE_ERROR("{} Failed, consider increasing NumDescriptorsPerRange", __FUNCTION__);
	}
	return ret;
}

DescriptorAllocation DescriptorAllocator::AllocateSRDescriptors(UINT NumDescriptors)
{
	auto ret = m_CBSRUADescriptorHeap.AllocateSRDescriptors(NumDescriptors);
	if (!ret)
	{
		CORE_ERROR("{} Failed, consider increasing NumDescriptorsPerRange", __FUNCTION__);
	}
	return ret;
}

DescriptorAllocation DescriptorAllocator::AllocateUADescriptors(UINT NumDescriptors)
{
	auto ret = m_CBSRUADescriptorHeap.AllocateUADescriptors(NumDescriptors);
	if (!ret)
	{
		CORE_ERROR("{} Failed, consider increasing NumDescriptorsPerRange", __FUNCTION__);
	}
	return ret;
}

DescriptorAllocation DescriptorAllocator::AllocateSamplerDescriptors(UINT NumDescriptors)
{
	auto ret = m_SamplerDescriptorHeap.Allocate(0, NumDescriptors);
	if (!ret)
	{
		CORE_ERROR("{} Failed, consider increasing NumDescriptorsPerRange", __FUNCTION__);
	}
	return ret;
}

DescriptorAllocation DescriptorAllocator::AllocateRenderTargetDescriptors(UINT NumDescriptors)
{
	auto ret = m_RenderTargetDescriptorHeap.Allocate(0, NumDescriptors);
	if (!ret)
	{
		CORE_ERROR("{} Failed, consider increasing NumDescriptorsPerRange", __FUNCTION__);
	}
	return ret;
}

DescriptorAllocation DescriptorAllocator::AllocateDepthStencilDescriptors(UINT NumDescriptors)
{
	auto ret = m_DepthStencilDescriptorHeap.Allocate(0, NumDescriptors);
	if (!ret)
	{
		CORE_ERROR("{} Failed, consider increasing NumDescriptorsPerRange", __FUNCTION__);
	}
	return ret;
}

void DescriptorAllocator::FreeCBDescriptors(DescriptorAllocation&& DescriptorAllocation)
{
	if (!DescriptorAllocation)
		return;
	DescriptorAllocation.pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ConstantBuffer, std::move(DescriptorAllocation));
}

void DescriptorAllocator::FreeSRDescriptors(DescriptorAllocation&& DescriptorAllocation)
{
	if (!DescriptorAllocation)
		return;
	DescriptorAllocation.pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ShaderResource, std::move(DescriptorAllocation));
}

void DescriptorAllocator::FreeUADescriptors(DescriptorAllocation&& DescriptorAllocation)
{
	if (!DescriptorAllocation)
		return;
	DescriptorAllocation.pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::UnorderedAccess, std::move(DescriptorAllocation));
}

void DescriptorAllocator::FreeSamplerDescriptors(DescriptorAllocation&& DescriptorAllocation)
{
	if (!DescriptorAllocation)
		return;
	DescriptorAllocation.pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ConstantBuffer, std::move(DescriptorAllocation));
}

void DescriptorAllocator::FreeRenderTargetDescriptors(DescriptorAllocation&& DescriptorAllocation)
{
	if (!DescriptorAllocation)
		return;
	DescriptorAllocation.pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ConstantBuffer, std::move(DescriptorAllocation));
}

void DescriptorAllocator::FreeDepthStencilDescriptors(DescriptorAllocation&& DescriptorAllocation)
{
	if (!DescriptorAllocation)
		return;
	DescriptorAllocation.pOwningHeap->Free(CBSRUADescriptorHeap::RangeType::ConstantBuffer, std::move(DescriptorAllocation));
}