inline void CommandContext::SetGraphicsRootSignature(const RootSignature* pRootSignature)
{
	m_pCommandList->SetGraphicsRootSignature(pRootSignature->GetD3DRootSignature());
}

inline void CommandContext::SetGraphicsRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
	m_pCommandList->SetGraphicsRootDescriptorTable(RootParameterIndex, BaseDescriptor);
}

inline void CommandContext::SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

inline void CommandContext::SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}

inline void CommandContext::SetGraphicsRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);
}

inline void CommandContext::SetGraphicsRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);
}

inline void CommandContext::SetGraphicsRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}

inline void CommandContext::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
	m_pCommandList->IASetPrimitiveTopology(PrimitiveTopology);
}

inline void CommandContext::SetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW* pViews)
{
	m_pCommandList->IASetVertexBuffers(StartSlot, NumViews, pViews);
}

inline void CommandContext::SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView)
{
	m_pCommandList->IASetIndexBuffer(pView);
}

inline void CommandContext::SetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports)
{
	m_pCommandList->RSSetViewports(NumViewports, pViewports);
}

inline void CommandContext::SetScissorRects(UINT NumRects, const D3D12_RECT* pRects)
{
	m_pCommandList->RSSetScissorRects(NumRects, pRects);
}

inline void CommandContext::SetRenderTargets(UINT NumRenderTargetDescriptors, Descriptor RenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, Descriptor DepthStencilDescriptor)
{
	const D3D12_CPU_DESCRIPTOR_HANDLE* pRenderTargetDescriptors = NumRenderTargetDescriptors > 0 ? &RenderTargetDescriptors.CPUHandle : nullptr;
	const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor = DepthStencilDescriptor.IsValid() ? &DepthStencilDescriptor.CPUHandle : nullptr;
	m_pCommandList->OMSetRenderTargets(NumRenderTargetDescriptors, pRenderTargetDescriptors, RTsSingleHandleToDescriptorRange, pDepthStencilDescriptor);
}

inline void CommandContext::ClearRenderTarget(Descriptor RenderTargetDescriptor, const FLOAT Color[4], UINT NumRects, const D3D12_RECT* pRects)
{
	m_pCommandList->ClearRenderTargetView(RenderTargetDescriptor.CPUHandle, Color, NumRects, pRects);
}

inline void CommandContext::ClearDepthStencil(Descriptor DepthStencilDescriptor, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects)
{
	m_pCommandList->ClearDepthStencilView(DepthStencilDescriptor.CPUHandle, ClearFlags, Depth, Stencil, NumRects, pRects);
}

inline void CommandContext::SetComputeRootSignature(const RootSignature* pRootSignature)
{
	m_pCommandList->SetComputeRootSignature(pRootSignature->GetD3DRootSignature());
}

inline void CommandContext::SetComputeRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor)
{
	m_pCommandList->SetComputeRootDescriptorTable(RootParameterIndex, BaseDescriptor);
}

inline void CommandContext::SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

inline void CommandContext::SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}

inline void CommandContext::SetComputeRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);
}

inline void CommandContext::SetComputeRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);
}

inline void CommandContext::SetComputeRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}