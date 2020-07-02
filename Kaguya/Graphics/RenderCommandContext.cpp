#include "pch.h"
#include "RenderCommandContext.h"
#include "RenderDevice.h"

#include "AL/D3D12/Resource.h"
#include "AL/D3D12/Buffer.h"
#include "AL/D3D12/Texture.h"
#include "AL/D3D12/RootSignature.h"
#include "AL/D3D12/PipelineState.h"

RenderCommandContext::RenderCommandContext(RenderDevice* pRenderDevice, D3D12_COMMAND_LIST_TYPE Type)
	: m_Type(Type),
	m_DynamicViewDescriptorHeap(&pRenderDevice->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
	m_DynamicSamplerDescriptorHeap(&pRenderDevice->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
{
	switch (Type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT: pOwningCommandQueue = pRenderDevice->GetGraphicsQueue(); break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE: pOwningCommandQueue = pRenderDevice->GetComputeQueue(); break;
	case D3D12_COMMAND_LIST_TYPE_COPY: pOwningCommandQueue = pRenderDevice->GetCopyQueue(); break;
	}

	m_pCurrentAllocator = pOwningCommandQueue->RequestAllocator();
	m_pCurrentPendingAllocator = pOwningCommandQueue->RequestAllocator();

	ThrowCOMIfFailed(pRenderDevice->GetDevice().GetD3DDevice()->CreateCommandList(1, Type, m_pCurrentAllocator, nullptr, IID_PPV_ARGS(m_pCommandList.ReleaseAndGetAddressOf())));
	ThrowCOMIfFailed(pRenderDevice->GetDevice().GetD3DDevice()->CreateCommandList(1, Type, m_pCurrentPendingAllocator, nullptr, IID_PPV_ARGS(m_pPendingCommandList.ReleaseAndGetAddressOf())));

	for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		m_pCurrentDescriptorHeaps[i] = nullptr;
	}
}

bool RenderCommandContext::Close(ResourceStateTracker& GlobalResourceStateTracker)
{
	FlushResourceBarriers();
	ThrowCOMIfFailed(m_pCommandList->Close());

	// TODO: Add a pending command list
	UINT numPendingBarriers = m_ResourceStateTracker.FlushPendingResourceBarriers(GlobalResourceStateTracker, m_pPendingCommandList.Get());
	GlobalResourceStateTracker.UpdateResourceStates(m_ResourceStateTracker);
	ThrowCOMIfFailed(m_pPendingCommandList->Close());
	return numPendingBarriers > 0;
}

void RenderCommandContext::Reset()
{
	m_pCommandList->Reset(m_pCurrentAllocator, nullptr);
	m_pPendingCommandList->Reset(m_pCurrentPendingAllocator, nullptr);
	for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		m_pCurrentDescriptorHeaps[i] = nullptr;

	// TODO: Add method to save bounded gpu descriptors before command list is resetted
	m_DynamicViewDescriptorHeap.Reset();
	m_DynamicSamplerDescriptorHeap.Reset();

	// Reset resource state tracking and resource barriers 
	m_ResourceStateTracker.Reset();
}

void RenderCommandContext::RequestNewAllocator(UINT64 FenceValue)
{
	pOwningCommandQueue->MarkAllocatorAsActive(FenceValue, m_pCurrentAllocator);
	m_pCurrentAllocator = pOwningCommandQueue->RequestAllocator();
}

void RenderCommandContext::SetPipelineState(const PipelineState* pPipelineState)
{
	m_pCommandList->SetPipelineState(pPipelineState->GetD3DPipelineState());
}

void RenderCommandContext::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* pDescriptorHeap)
{
	if (!pDescriptorHeap) return;
	if (m_pCurrentDescriptorHeaps[Type] == pDescriptorHeap) return;

	m_pCurrentDescriptorHeaps[Type] = pDescriptorHeap;

	UINT NumDescriptorHeaps = 0;
	ID3D12DescriptorHeap* DescriptorHeapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

	for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		ID3D12DescriptorHeap* DescriptorHeapIter = m_pCurrentDescriptorHeaps[i];
		if (DescriptorHeapIter)
			DescriptorHeapsToBind[NumDescriptorHeaps++] = DescriptorHeapIter;
	}

	if (NumDescriptorHeaps > 0)
		m_pCommandList->SetDescriptorHeaps(NumDescriptorHeaps, DescriptorHeapsToBind);
}

void RenderCommandContext::TransitionBarrier(const Resource* pResource, D3D12_RESOURCE_STATES TransitionState, UINT Subresource /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
{
#define D3D12_RESOURCE_STATE_UNKNOWN (static_cast<D3D12_RESOURCE_STATES>(-1))
	// Transition barriers indicate that a set of subresources transition between different usages.  
	// The caller must specify the before and after usages of the subresources.  
	// The D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES flag is used to transition all subresources in a resource at the same time. 
	ID3D12Resource* resource = pResource ? pResource->GetD3DResource() : nullptr;
	if (resource)
		m_ResourceStateTracker.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_UNKNOWN, TransitionState, Subresource));
#undef D3D12_RESOURCE_STATE_UNKNOWN
}

void RenderCommandContext::AliasingBarrier(const Resource* pBeforeResource, const Resource* pAfterResource)
{
	// Aliasing barriers indicate a transition between usages of two different resources which have mappings into the same heap.  
	// The application can specify both the before and the after resource.  
	// Note that one or both resources can be NULL (indicating that any tiled resource could cause aliasing). 
	ID3D12Resource* before = pBeforeResource ? pBeforeResource->GetD3DResource() : nullptr;
	ID3D12Resource* after = pAfterResource ? pAfterResource->GetD3DResource() : nullptr;
	m_ResourceStateTracker.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(before, after));
}

void RenderCommandContext::UAVBarrier(const Resource* pResource)
{
	// A UAV barrier indicates that all UAV accesses, both read or write,  
	// to a particular resource must complete between any future UAV accesses, both read or write.  
	// It's not necessary for an app to put a UAV barrier between two draw or dispatch calls that only read from a UAV.  
	// Also, it's not necessary to put a UAV barrier between two draw or dispatch calls that  
	// write to the same UAV if the application knows that it is safe to execute the UAV access in any order.  
	// A D3D12_RESOURCE_UAV_BARRIER structure is used to specify the UAV resource to which the barrier applies.  
	// The application can specify NULL for the barrier's UAV, which indicates that any UAV access could require the barrier. 
	ID3D12Resource* resource = pResource ? pResource->GetD3DResource() : nullptr;
	m_ResourceStateTracker.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(resource));
}

void RenderCommandContext::FlushResourceBarriers()
{
	m_ResourceStateTracker.FlushResourceBarriers(m_pCommandList.Get());
}

void RenderCommandContext::CopyResource(Resource* pDstResource, Resource* pSrcResource)
{
	TransitionBarrier(pDstResource, D3D12_RESOURCE_STATE_COPY_DEST);
	TransitionBarrier(pSrcResource, D3D12_RESOURCE_STATE_COPY_SOURCE);
	FlushResourceBarriers();
	m_pCommandList->CopyResource(pDstResource->GetD3DResource(), pSrcResource->GetD3DResource());
}

void RenderCommandContext::SetSRV(UINT RootParameterIndex, UINT DescriptorOffset, UINT NumHandlesWithinDescriptor, Descriptor Descriptor)
{
	m_DynamicViewDescriptorHeap.StageDescriptors(RootParameterIndex, DescriptorOffset, NumHandlesWithinDescriptor, Descriptor[0]);
}

void RenderCommandContext::SetUAV(UINT RootParameterIndex, UINT DescriptorOffset, UINT NumHandlesWithinDescriptor, Descriptor Descriptor)
{
	m_DynamicViewDescriptorHeap.StageDescriptors(RootParameterIndex, DescriptorOffset, NumHandlesWithinDescriptor, Descriptor[0]);
}

void RenderCommandContext::SetGraphicsRootSignature(const RootSignature* pRootSignature)
{
	auto pD3DRootSignature = pRootSignature->GetD3DRootSignature();
	m_DynamicViewDescriptorHeap.ParseRootSignature(pRootSignature);
	m_DynamicSamplerDescriptorHeap.ParseRootSignature(pRootSignature);
	m_pCommandList->SetGraphicsRootSignature(pD3DRootSignature);
}

void RenderCommandContext::SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

void RenderCommandContext::SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}

void RenderCommandContext::SetGraphicsRootCBV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);
}

void RenderCommandContext::SetGraphicsRootSRV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);
}

void RenderCommandContext::SetGraphicsRootUAV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}

void RenderCommandContext::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
	m_pCommandList->IASetPrimitiveTopology(PrimitiveTopology);
}

void RenderCommandContext::SetVertexBuffers(UINT StartSlot, UINT NumViews, Buffer* const* ppVertexBuffers)
{
	if (ppVertexBuffers)
	{
		std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferViews(NumViews);
		for (UINT i = 0; i < NumViews; ++i)
		{
			vertexBufferViews[i].BufferLocation = ppVertexBuffers[i]->GetD3DResource()->GetGPUVirtualAddress();
			vertexBufferViews[i].SizeInBytes = UINT64(ppVertexBuffers[i]->GetNumElements()) * UINT64(ppVertexBuffers[i]->GetStride());
			vertexBufferViews[i].StrideInBytes = ppVertexBuffers[i]->GetStride();
		}
		m_pCommandList->IASetVertexBuffers(StartSlot, NumViews, vertexBufferViews.data());
	}
	else
	{
		m_pCommandList->IASetVertexBuffers(0u, 0u, nullptr);
	}
}

void RenderCommandContext::SetIndexBuffer(Buffer* pIndexBuffer)
{
	if (pIndexBuffer && pIndexBuffer->GetD3DResource())
	{
		D3D12_INDEX_BUFFER_VIEW indexBufferView;
		indexBufferView.BufferLocation = pIndexBuffer->GetD3DResource()->GetGPUVirtualAddress();
		indexBufferView.SizeInBytes = UINT64(pIndexBuffer->GetNumElements()) * UINT64(pIndexBuffer->GetStride());
		indexBufferView.Format = pIndexBuffer->GetStride() == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
		m_pCommandList->IASetIndexBuffer(&indexBufferView);
	}
	else
		m_pCommandList->IASetIndexBuffer(nullptr);
}

void RenderCommandContext::SetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports)
{
	m_pCommandList->RSSetViewports(NumViewports, pViewports);
}

void RenderCommandContext::SetScissorRects(UINT NumRects, const D3D12_RECT* pRects)
{
	m_pCommandList->RSSetScissorRects(NumRects, pRects);
}

void RenderCommandContext::DrawInstanced(UINT VertexCount, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	FlushResourceBarriers();
	m_DynamicViewDescriptorHeap.CommitStagedDescriptorsForDraw(m_pCommandList.Get());
	m_DynamicSamplerDescriptorHeap.CommitStagedDescriptorsForDraw(m_pCommandList.Get());
	m_pCommandList->DrawInstanced(VertexCount, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void RenderCommandContext::DrawIndexedInstanced(UINT IndexCount, UINT InstanceCount, UINT StartIndexLocation, UINT BaseVertexLocation, UINT StartInstanceLocation)
{
	FlushResourceBarriers();
	m_DynamicViewDescriptorHeap.CommitStagedDescriptorsForDraw(m_pCommandList.Get());
	m_DynamicSamplerDescriptorHeap.CommitStagedDescriptorsForDraw(m_pCommandList.Get());
	m_pCommandList->DrawIndexedInstanced(IndexCount, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

void RenderCommandContext::SetComputeRootSignature(const RootSignature* pRootSignature)
{
	auto pD3DRootSignature = pRootSignature->GetD3DRootSignature();
	m_DynamicViewDescriptorHeap.ParseRootSignature(pRootSignature);
	m_DynamicSamplerDescriptorHeap.ParseRootSignature(pRootSignature);
	m_pCommandList->SetComputeRootSignature(pD3DRootSignature);
}

void RenderCommandContext::SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

void RenderCommandContext::SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}

void RenderCommandContext::SetComputeRootCBV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);
}

void RenderCommandContext::SetComputeRootSRV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);
}

void RenderCommandContext::SetComputeRootUAV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}

void RenderCommandContext::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
	FlushResourceBarriers();
	m_DynamicViewDescriptorHeap.CommitStagedDescriptorsForDispatch(m_pCommandList.Get());
	m_DynamicSamplerDescriptorHeap.CommitStagedDescriptorsForDispatch(m_pCommandList.Get());
	m_pCommandList->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}