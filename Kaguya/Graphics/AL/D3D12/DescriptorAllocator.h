#pragma once
#include <d3d12.h>

#include "DescriptorHeap.h"

class Device;

class DescriptorAllocator
{
public:
	DescriptorAllocator(Device* pDevice);
	~DescriptorAllocator();

	inline auto* GetUniversalGpuDescriptorHeap() const { return &m_CBSRUADescriptorHeap; }

	DescriptorAllocation AllocateCBDescriptors(UINT NumDescriptors);
	DescriptorAllocation AllocateSRDescriptors(UINT NumDescriptors);
	DescriptorAllocation AllocateUADescriptors(UINT NumDescriptors);

	DescriptorAllocation AllocateSamplerDescriptors(UINT NumDescriptors);

	DescriptorAllocation AllocateRenderTargetDescriptors(UINT NumDescriptors);

	DescriptorAllocation AllocateDepthStencilDescriptors(UINT NumDescriptors);

	void FreeCBDescriptors(DescriptorAllocation& DescriptorAllocation);
	void FreeSRDescriptors(DescriptorAllocation& DescriptorAllocation);
	void FreeUADescriptors(DescriptorAllocation& DescriptorAllocation);

	void FreeSamplerDescriptors(DescriptorAllocation& DescriptorAllocation);

	void FreeRenderTargetDescriptors(DescriptorAllocation& DescriptorAllocation);

	void FreeDepthStencilDescriptors(DescriptorAllocation& DescriptorAllocation);
private:
	enum : UINT { NumDescriptorsPerRange = 2048 };

	Device* m_pDevice;

	CBSRUADescriptorHeap m_CBSRUADescriptorHeap;
	SamplerDescriptorHeap m_SamplerDescriptorHeap;
	RenderTargetDescriptorHeap m_RenderTargetDescriptorHeap;
	DepthStencilDescriptorHeap m_DepthStencilDescriptorHeap;
};