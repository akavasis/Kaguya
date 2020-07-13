#include "RenderCommandContext.h"
inline void RenderCommandContext::SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

inline void RenderCommandContext::SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}

inline void RenderCommandContext::SetGraphicsRootCBV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);
}

inline void RenderCommandContext::SetGraphicsRootSRV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);
}

inline void RenderCommandContext::SetGraphicsRootUAV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}

inline void RenderCommandContext::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
	m_pCommandList->IASetPrimitiveTopology(PrimitiveTopology);
}

inline void RenderCommandContext::SetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW* pViews)
{
	m_pCommandList->IASetVertexBuffers(StartSlot, NumViews, pViews);
}

inline void RenderCommandContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView)
{
	m_pCommandList->IASetIndexBuffer(pView);
}

inline void RenderCommandContext::SetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports)
{
	m_pCommandList->RSSetViewports(NumViewports, pViewports);
}

inline void RenderCommandContext::SetScissorRects(UINT NumRects, const D3D12_RECT* pRects)
{
	m_pCommandList->RSSetScissorRects(NumRects, pRects);
}

inline void RenderCommandContext::SetRenderTargets(UINT RenderTargetDescriptorOffset, UINT NumHandlesWithinRenderTargetDescriptor, Descriptor RenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, UINT DepthStencilDescriptorOffset, Descriptor DepthStencilDescriptor)
{
	const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors = RenderTargetDescriptors ? &RenderTargetDescriptors[RenderTargetDescriptorOffset] : nullptr;
	const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor = DepthStencilDescriptor ? &DepthStencilDescriptor[DepthStencilDescriptorOffset] : nullptr;
	m_pCommandList->OMSetRenderTargets(NumHandlesWithinRenderTargetDescriptor, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
}

inline void RenderCommandContext::SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

inline void RenderCommandContext::SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}

inline void RenderCommandContext::SetComputeRootCBV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);
}

inline void RenderCommandContext::SetComputeRootSRV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);
}

inline void RenderCommandContext::SetComputeRootUAV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}