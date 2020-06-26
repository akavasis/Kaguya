#include "pch.h"
#include "RenderGraphRegistry.h"
#include "../RenderDevice.h"

RenderGraphRegistry::RenderGraphRegistry(RenderDevice& RenderDevice)
	: m_RenderDevice(RenderDevice)
{
}

Buffer* RenderGraphRegistry::GetBuffer(const RenderResourceHandle& RenderResourceHandle) const
{
	return m_RenderDevice.GetBuffer(RenderResourceHandle);
}

Texture* RenderGraphRegistry::GetTexture(const RenderResourceHandle& RenderResourceHandle) const
{
	return m_RenderDevice.GetTexture(RenderResourceHandle);
}

Heap* RenderGraphRegistry::GetHeap(const RenderResourceHandle& RenderResourceHandle) const
{
	return m_RenderDevice.GetHeap(RenderResourceHandle);
}

RootSignature* RenderGraphRegistry::GetRootSignature(const RenderResourceHandle& RenderResourceHandle) const
{
	return m_RenderDevice.GetRootSignature(RenderResourceHandle);
}

GraphicsPipelineState* RenderGraphRegistry::GetGraphicsPSO(const RenderResourceHandle& RenderResourceHandle) const
{
	return m_RenderDevice.GetGraphicsPSO(RenderResourceHandle);
}

ComputePipelineState* RenderGraphRegistry::GetComputePSO(const RenderResourceHandle& RenderResourceHandle) const
{
	return m_RenderDevice.GetComputePSO(RenderResourceHandle);
}