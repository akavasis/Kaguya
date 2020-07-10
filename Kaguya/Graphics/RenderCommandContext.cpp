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

void RenderCommandContext::TransitionBarrier(Resource* pResource, D3D12_RESOURCE_STATES TransitionState, UINT Subresource /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
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

void RenderCommandContext::AliasingBarrier(Resource* pBeforeResource, Resource* pAfterResource)
{
	// Aliasing barriers indicate a transition between usages of two different resources which have mappings into the same heap.  
	// The application can specify both the before and the after resource.  
	// Note that one or both resources can be NULL (indicating that any tiled resource could cause aliasing). 
	ID3D12Resource* before = pBeforeResource ? pBeforeResource->GetD3DResource() : nullptr;
	ID3D12Resource* after = pAfterResource ? pAfterResource->GetD3DResource() : nullptr;
	m_ResourceStateTracker.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(before, after));
}

void RenderCommandContext::UAVBarrier(Resource* pResource)
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

void RenderCommandContext::CopyBufferRegion(Buffer* pDstBuffer, UINT64 DstOffset, Buffer* pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes)
{
	TransitionBarrier(pDstBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
	TransitionBarrier(pSrcBuffer, D3D12_RESOURCE_STATE_COPY_SOURCE);
	FlushResourceBarriers();
	m_pCommandList->CopyBufferRegion(pDstBuffer->GetD3DResource(), DstOffset, pSrcBuffer->GetD3DResource(), SrcOffset, NumBytes);
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

void RenderCommandContext::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
	FlushResourceBarriers();
	m_DynamicViewDescriptorHeap.CommitStagedDescriptorsForDispatch(m_pCommandList.Get());
	m_DynamicSamplerDescriptorHeap.CommitStagedDescriptorsForDispatch(m_pCommandList.Get());
	m_pCommandList->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}