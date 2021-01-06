#include "pch.h"
#include "CommandContext.h"

CommandContext::CommandContext(ID3D12Device4* pDevice, D3D12_COMMAND_LIST_TYPE CommandListType)
{
	auto CommandListFlags	= D3D12_COMMAND_LIST_FLAG_NONE;
	ThrowIfFailed(pDevice->CreateCommandList1(1, CommandListType, CommandListFlags, IID_PPV_ARGS(m_pCommandList.ReleaseAndGetAddressOf())));
	ThrowIfFailed(pDevice->CreateCommandList1(1, CommandListType, CommandListFlags, IID_PPV_ARGS(m_pPendingCommandList.ReleaseAndGetAddressOf())));
}

bool CommandContext::Close(ResourceStateTracker* pGlobalResourceStateTracker)
{
	FlushResourceBarriers();
	ThrowIfFailed(m_pCommandList->Close());

	UINT numPendingBarriers = m_ResourceStateTracker.FlushPendingResourceBarriers(*pGlobalResourceStateTracker, m_pPendingCommandList.Get());
	pGlobalResourceStateTracker->UpdateResourceStates(m_ResourceStateTracker);
	ThrowIfFailed(m_pPendingCommandList->Close());
	return numPendingBarriers > 0;
}

void CommandContext::Reset(UINT64 FenceValue, UINT64 CompletedValue, CommandQueue* pCommandQueue)
{
	pCommandQueue->DiscardCommandAllocator(FenceValue, pAllocator);
	pCommandQueue->DiscardCommandAllocator(FenceValue, pPendingAllocator);

	pAllocator			= pCommandQueue->RequestCommandAllocator(CompletedValue);
	pPendingAllocator	= pCommandQueue->RequestCommandAllocator(CompletedValue);

	ThrowIfFailed(m_pCommandList->Reset(pAllocator, nullptr));
	ThrowIfFailed(m_pPendingCommandList->Reset(pPendingAllocator, nullptr));

	// Reset resource state tracking and resource barriers 
	m_ResourceStateTracker.Reset();
}

void CommandContext::SetPipelineState(const PipelineState* pPipelineState)
{
	m_pCommandList->SetPipelineState(pPipelineState->GetApiHandle());
}

void CommandContext::SetDescriptorHeaps(CBSRUADescriptorHeap* pCBSRUADescriptorHeap, SamplerDescriptorHeap* pSamplerDescriptorHeap)
{
	UINT					NumDescriptorHeaps												= 0;
	ID3D12DescriptorHeap*	pDescriptorHeapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES]	= {};
	if (pCBSRUADescriptorHeap)	
	{
		pDescriptorHeapsToBind[NumDescriptorHeaps++] = pCBSRUADescriptorHeap->GetApiHandle();
	}
	if (pSamplerDescriptorHeap) 
	{
		pDescriptorHeapsToBind[NumDescriptorHeaps++] = pSamplerDescriptorHeap->GetApiHandle();
	}

	if (NumDescriptorHeaps > 0)
	{
		m_pCommandList->SetDescriptorHeaps(NumDescriptorHeaps, pDescriptorHeapsToBind);
	}
}

void CommandContext::TransitionBarrier(Resource* pResource, Resource::State TransitionState, UINT Subresource /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
{
	// Transition barriers indicate that a set of subresources transition between different usages.  
	// The caller must specify the before and after usages of the subresources.  
	// The D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES flag is used to transition all subresources in a resource at the same time. 
	ID3D12Resource* resource = pResource ? pResource->GetApiHandle() : nullptr;
	if (resource)
	{
		D3D12_RESOURCE_STATES transitionState = GetD3D12ResourceStates(TransitionState);
		m_ResourceStateTracker.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Transition(resource, D3D12_RESOURCE_STATE_UNKNOWN, transitionState, Subresource));
	}
}

void CommandContext::AliasingBarrier(Resource* pBeforeResource, Resource* pAfterResource)
{
	// Aliasing barriers indicate a transition between usages of two different resources which have mappings into the same heap.  
	// The application can specify both the before and the after resource.  
	// Note that one or both resources can be NULL (indicating that any tiled resource could cause aliasing). 
	ID3D12Resource* before = pBeforeResource ? pBeforeResource->GetApiHandle() : nullptr;
	ID3D12Resource* after = pAfterResource ? pAfterResource->GetApiHandle() : nullptr;
	m_ResourceStateTracker.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::Aliasing(before, after));
}

void CommandContext::UAVBarrier(Resource* pResource)
{
	// A UAV barrier indicates that all UAV accesses, both read or write,  
	// to a particular resource must complete between any future UAV accesses, both read or write.  
	// It's not necessary for an app to put a UAV barrier between two draw or dispatch calls that only read from a UAV.  
	// Also, it's not necessary to put a UAV barrier between two draw or dispatch calls that  
	// write to the same UAV if the application knows that it is safe to execute the UAV access in any order.  
	// A D3D12_RESOURCE_UAV_BARRIER structure is used to specify the UAV resource to which the barrier applies.  
	// The application can specify NULL for the barrier's UAV, which indicates that any UAV access could require the barrier. 
	ID3D12Resource* resource = pResource ? pResource->GetApiHandle() : nullptr;
	m_ResourceStateTracker.ResourceBarrier(CD3DX12_RESOURCE_BARRIER::UAV(resource));
}

void CommandContext::FlushResourceBarriers()
{
	m_ResourceStateTracker.FlushResourceBarriers(m_pCommandList.Get());
}

void CommandContext::CopyBufferRegion(Buffer* pDstBuffer, UINT64 DstOffset, Buffer* pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes)
{
	if (!(pDstBuffer->GetCpuAccess() == Buffer::CpuAccess::Read)) // Readback resources have to be in D3D12_RESOURCE_STATE_COPY_DEST so dont need to transition them
		TransitionBarrier(pDstBuffer, Resource::State::CopyDest);
	if (!(pSrcBuffer->GetCpuAccess() == Buffer::CpuAccess::Write)) // Upload resources have to be in D3D12_RESOURCE_STATE_GENERIC_READ so dont need to transition them
		TransitionBarrier(pSrcBuffer, Resource::State::CopySource);
	FlushResourceBarriers();
	m_pCommandList->CopyBufferRegion(pDstBuffer->GetApiHandle(), DstOffset, pSrcBuffer->GetApiHandle(), SrcOffset, NumBytes);
}

void CommandContext::CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox)
{
	m_pCommandList->CopyTextureRegion(pDst, DstX, DstY, DstZ, pSrc, pSrcBox);
}

void CommandContext::CopyResource(Resource* pDstResource, Resource* pSrcResource)
{
	TransitionBarrier(pDstResource, Resource::State::CopyDest);
	TransitionBarrier(pSrcResource, Resource::State::CopySource);
	FlushResourceBarriers();
	m_pCommandList->CopyResource(pDstResource->GetApiHandle(), pSrcResource->GetApiHandle());
}

void CommandContext::DrawInstanced(UINT VertexCount, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	FlushResourceBarriers();
	m_pCommandList->DrawInstanced(VertexCount, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void CommandContext::DrawIndexedInstanced(UINT IndexCount, UINT InstanceCount, UINT StartIndexLocation, UINT BaseVertexLocation, UINT StartInstanceLocation)
{
	FlushResourceBarriers();
	m_pCommandList->DrawIndexedInstanced(IndexCount, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

void CommandContext::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
	FlushResourceBarriers();
	m_pCommandList->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void CommandContext::Dispatch1D(UINT ThreadGroupCountX, UINT ThreadSizeX)
{
	Dispatch(Math::RoundUpAndDivide(ThreadGroupCountX, ThreadSizeX), 1, 1);
}

void CommandContext::Dispatch2D(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadSizeX, UINT ThreadSizeY)
{
	Dispatch(
		Math::RoundUpAndDivide(ThreadGroupCountX, ThreadSizeX),
		Math::RoundUpAndDivide(ThreadGroupCountY, ThreadSizeY),
		1);
}

void CommandContext::Dispatch3D(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ, UINT ThreadSizeX, UINT ThreadSizeY, UINT ThreadSizeZ)
{
	Dispatch(
		Math::RoundUpAndDivide(ThreadGroupCountX, ThreadSizeX),
		Math::RoundUpAndDivide(ThreadGroupCountY, ThreadSizeY),
		Math::RoundUpAndDivide(ThreadGroupCountZ, ThreadSizeZ));
}

void CommandContext::BuildRaytracingAccelerationStructure(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* pDesc, UINT NumPostbuildInfoDescs, const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* pPostbuildInfoDescs)
{
	m_pCommandList->BuildRaytracingAccelerationStructure(pDesc, NumPostbuildInfoDescs, pPostbuildInfoDescs);
}