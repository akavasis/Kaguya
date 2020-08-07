#pragma once
#include <d3d12.h>
#include "DescriptorHeap.h"

class Device;

class DescriptorAllocator
{
public:
	DescriptorAllocator(Device* pDevice);

	inline auto* GetUniversalGpuDescriptorHeap() { return &m_CBSRUADescriptorHeap; }
	inline auto* GetUniversalGpuDescriptorHeap() const { return &m_CBSRUADescriptorHeap; }

	DescriptorAllocation AllocateCBDescriptors(UINT NumDescriptors);
	DescriptorAllocation AllocateSRDescriptors(UINT NumDescriptors);
	DescriptorAllocation AllocateUADescriptors(UINT NumDescriptors);

	DescriptorAllocation AllocateSamplerDescriptors(UINT NumDescriptors);

	DescriptorAllocation AllocateRenderTargetDescriptors(UINT NumDescriptors);

	DescriptorAllocation AllocateDepthStencilDescriptors(UINT NumDescriptors);

	void FreeCBDescriptors(DescriptorAllocation* pDescriptorAllocation);
	void FreeSRDescriptors(DescriptorAllocation* pDescriptorAllocation);
	void FreeUADescriptors(DescriptorAllocation* pDescriptorAllocation);

	void FreeSamplerDescriptors(DescriptorAllocation* pDescriptorAllocation);

	void FreeRenderTargetDescriptors(DescriptorAllocation* pDescriptorAllocation);

	void FreeDepthStencilDescriptors(DescriptorAllocation* pDescriptorAllocation);
private:
	enum : UINT { NumDescriptorsPerRange = 2048 };

	Device* m_pDevice;

	CBSRUADescriptorHeap m_CBSRUADescriptorHeap;
	SamplerDescriptorHeap m_SamplerDescriptorHeap;
	RenderTargetDescriptorHeap m_RenderTargetDescriptorHeap;
	DepthStencilDescriptorHeap m_DepthStencilDescriptorHeap;
};