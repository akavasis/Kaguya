#include "pch.h"
#include "RenderCommandContext.h"
#include "RenderDevice.h"

#include "AL/D3D12/Resource.h"
#include "AL/D3D12/Buffer.h"
#include "AL/D3D12/Texture.h"
#include "AL/D3D12/RootSignature.h"
#include "AL/D3D12/PipelineState.h"

#pragma region RenderCommandContext
RenderCommandContext::RenderCommandContext(RenderDevice* pRenderDevice, D3D12_COMMAND_LIST_TYPE Type)
	: m_Type(Type),
	m_DynamicViewDescriptorHeap(&pRenderDevice->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
	m_DynamicSamplerDescriptorHeap(&pRenderDevice->GetDevice(), D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
{
	switch (Type)
	{
	case D3D12_COMMAND_LIST_TYPE_DIRECT:
		pOwningCommandQueue = pRenderDevice->GetGraphicsQueue();
		m_pCurrentAllocator = pRenderDevice->GetGraphicsQueue()->RequestAllocator();
		break;
	case D3D12_COMMAND_LIST_TYPE_COMPUTE:
		pOwningCommandQueue = pRenderDevice->GetComputeQueue();
		m_pCurrentAllocator = pRenderDevice->GetComputeQueue()->RequestAllocator();
		break;
	case D3D12_COMMAND_LIST_TYPE_COPY:
		pOwningCommandQueue = pRenderDevice->GetCopyQueue();
		m_pCurrentAllocator = pRenderDevice->GetCopyQueue()->RequestAllocator();
		break;
	}

	ThrowCOMIfFailed(pRenderDevice->GetDevice().GetD3DDevice()->CreateCommandList(1, Type, m_pCurrentAllocator, nullptr, IID_PPV_ARGS(m_pCommandList.ReleaseAndGetAddressOf())));

	for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		m_pCurrentDescriptorHeaps[i] = nullptr;
	}
}

bool RenderCommandContext::Close(ResourceStateTracker* pGlobalResourceStateTracker)
{
	FlushResourceBarriers();
	ThrowCOMIfFailed(m_pCommandList->Close());

	// Resolve the pending resource barriers by checking the global state of the  
	// (sub)resources. Add barriers if the pending state and the global state do 
	//  not match. 
	std::vector<D3D12_RESOURCE_BARRIER> resourceBarriers;
	// Reserve enough space (worst-case, all pending barriers). 
	resourceBarriers.reserve(m_PendingResourceBarriers.size());

	for (auto pendingBarrier : m_PendingResourceBarriers)
	{
		auto pendingTransition = pendingBarrier.Transition;

		auto resourceState = pGlobalResourceStateTracker->Find(pendingTransition.pResource);
		if (!resourceState.has_value())
			continue;

		// If all subresources are being transitioned, and there are multiple 
		// subresources of the resource that are in a different state... 
		if (pendingTransition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
			!resourceState->SubresourceState.empty())
		{
			// Transition all subresources 
			for (const auto& subresourceState : resourceState->SubresourceState)
			{
				if (pendingTransition.StateAfter != subresourceState.second)
				{
					D3D12_RESOURCE_BARRIER newBarrier = pendingBarrier;
					newBarrier.Transition.Subresource = subresourceState.first;
					newBarrier.Transition.StateBefore = subresourceState.second;
					resourceBarriers.push_back(newBarrier);
				}
			}
		}
		else
		{
			// No (sub)resources need to be transitioned. Just add a single transition barrier (if needed). 
			auto globalState = resourceState->GetSubresourceState(pendingTransition.Subresource);
			if (pendingTransition.StateAfter != globalState)
			{
				// Fix-up the before state based on current global state of the resource. 
				pendingBarrier.Transition.StateBefore = globalState;
				resourceBarriers.push_back(pendingBarrier);
			}
		}
	}

	UINT numBarriers = static_cast<UINT>(resourceBarriers.size());
	if (numBarriers > 0)
	{
		//auto pD3DCommandList = pPendingCommandList->GetD3DCommandList();
		//pD3DCommandList->ResourceBarrier(numBarriers, resourceBarriers.data());
	}

	m_PendingResourceBarriers.clear();

	pGlobalResourceStateTracker->UpdateResourceStates(m_ResourceStateTracker);
	m_ResourceStateTracker.Reset();
	return numBarriers > 0;
}

void RenderCommandContext::Reset()
{
	m_pCommandList->Reset(m_pCurrentAllocator, nullptr);
	for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
		m_pCurrentDescriptorHeaps[i] = nullptr;

	// TODO: Add method to save bounded gpu descriptors before command list is resetted
	m_DynamicViewDescriptorHeap.Reset();
	m_DynamicSamplerDescriptorHeap.Reset();

	// Reset resource state tracking and resource barriers 
	m_ResourceStateTracker.Reset();
	m_ResourceBarriers.clear();
	m_PendingResourceBarriers.clear();
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
	// Transition barriers indicate that a set of subresources transition between different usages.  
	// The caller must specify the before and after usages of the subresources.  
	// The D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES flag is used to transition all subresources in a resource at the same time. 
	D3D12_RESOURCE_BARRIER resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(pResource->GetD3DResource(), D3D12_RESOURCE_STATE_COMMON, TransitionState, Subresource);

	// First check if there is already a known state for the given resource. 
	// If there is, the resource has been used on the command list before and 
	// already has a known state within the command list execution. 
	auto resourceState = m_ResourceStateTracker.Find(resourceBarrier.Transition.pResource);
	if (resourceState.has_value())
	{
		// If the known state of the resource is different... 
		if (resourceBarrier.Transition.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
			!resourceState->SubresourceState.empty())
		{
			// First transition all of the subresources if they are different than the StateAfter. 
			for (auto subresourceState : resourceState->SubresourceState)
			{
				if (resourceBarrier.Transition.StateAfter != subresourceState.second)
				{
					D3D12_RESOURCE_BARRIER newBarrier = resourceBarrier;
					newBarrier.Transition.Subresource = subresourceState.first;
					newBarrier.Transition.StateBefore = subresourceState.second;
					m_ResourceBarriers.push_back(newBarrier);
				}
			}
		}
		else
		{
			D3D12_RESOURCE_STATES finalState = resourceState->GetSubresourceState(resourceBarrier.Transition.Subresource);
			if (resourceBarrier.Transition.StateAfter != finalState)
			{
				// Push a new transition barrier with the correct before state. 
				D3D12_RESOURCE_BARRIER newBarrier = resourceBarrier;
				newBarrier.Transition.StateBefore = finalState;
				m_ResourceBarriers.push_back(newBarrier);
			}
		}
	}
	else
	{
		m_PendingResourceBarriers.push_back(resourceBarrier);
	}

	// Update the state of the resource/subresource for this command list 
	m_ResourceStateTracker.SetResourceState(resourceBarrier.Transition.pResource, resourceBarrier.Transition.Subresource, resourceBarrier.Transition.StateAfter);
}

void RenderCommandContext::AliasingBarrier(Resource* pBeforeResource, Resource* pAfterResource)
{
	// Aliasing barriers indicate a transition between usages of two different resources which have mappings into the same heap.  
	// The application can specify both the before and the after resource.  
	// Note that one or both resources can be NULL (indicating that any tiled resource could cause aliasing). 
	D3D12_RESOURCE_BARRIER barrier;
	if (!pBeforeResource && !pAfterResource)
		barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(nullptr, nullptr);
	else if (pBeforeResource && !pAfterResource)
		barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(pBeforeResource->GetD3DResource(), nullptr);
	else if (!pBeforeResource && pAfterResource)
		barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(nullptr, pAfterResource->GetD3DResource());
	else
		barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(pBeforeResource->GetD3DResource(), pAfterResource->GetD3DResource());
	m_ResourceBarriers.push_back(barrier);
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
	m_ResourceBarriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(pResource->GetD3DResource()));
}

void RenderCommandContext::FlushResourceBarriers()
{
	UINT numBarriers = static_cast<UINT>(m_ResourceBarriers.size());
	if (numBarriers > 0)
	{
		m_pCommandList->ResourceBarrier(numBarriers, m_ResourceBarriers.data());
		m_ResourceBarriers.clear();
	}
}

void RenderCommandContext::CopyResource(Resource* pDstResource, Resource* pSrcResource)
{
	TransitionBarrier(pDstResource, D3D12_RESOURCE_STATE_COPY_DEST);
	TransitionBarrier(pSrcResource, D3D12_RESOURCE_STATE_COPY_SOURCE);
	m_pCommandList->CopyResource(pDstResource->GetD3DResource(), pSrcResource->GetD3DResource());
}

void RenderCommandContext::SetSRV(UINT RootParameterIndex, UINT DescriptorOffset, Resource* pResource, D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle)
{
	m_DynamicViewDescriptorHeap.StageDescriptors(RootParameterIndex, DescriptorOffset, 1, CPUHandle);
}

void RenderCommandContext::SetUAV(UINT RootParameterIndex, UINT DescriptorOffset, Resource* pResource, D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle)
{
	m_DynamicViewDescriptorHeap.StageDescriptors(RootParameterIndex, DescriptorOffset, 1, CPUHandle);
}
#pragma endregion RenderCommandContext

#pragma region GraphicsCommandContext
GraphicsCommandContext::GraphicsCommandContext(RenderDevice* pRenderDevice)
	: RenderCommandContext(pRenderDevice, D3D12_COMMAND_LIST_TYPE_DIRECT)
{
}

void GraphicsCommandContext::SetRootSignature(const RootSignature* pRootSignature)
{
	auto pD3DRootSignature = pRootSignature->GetD3DRootSignature();
	m_DynamicViewDescriptorHeap.ParseRootSignature(pRootSignature);
	m_DynamicSamplerDescriptorHeap.ParseRootSignature(pRootSignature);
	m_pCommandList->SetGraphicsRootSignature(pD3DRootSignature);
}

void GraphicsCommandContext::SetRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

void GraphicsCommandContext::SetRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}

void GraphicsCommandContext::SetRootCBV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);
}

void GraphicsCommandContext::SetRootSRV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);
}

void GraphicsCommandContext::SetRootUAV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}

void GraphicsCommandContext::SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
	m_pCommandList->IASetPrimitiveTopology(PrimitiveTopology);
}

void GraphicsCommandContext::SetVertexBuffers(UINT StartSlot, UINT NumViews, Buffer* const* ppVertexBuffers)
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

void GraphicsCommandContext::SetIndexBuffer(Buffer* pIndexBuffer)
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

void GraphicsCommandContext::SetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports)
{
	m_pCommandList->RSSetViewports(NumViewports, pViewports);
}

void GraphicsCommandContext::SetScissorRects(UINT NumRects, const D3D12_RECT* pRects)
{
	m_pCommandList->RSSetScissorRects(NumRects, pRects);
}

void GraphicsCommandContext::DrawInstanced(UINT VertexCount, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	FlushResourceBarriers();
	m_DynamicViewDescriptorHeap.CommitStagedDescriptorsForDraw(m_pCommandList.Get());
	m_DynamicSamplerDescriptorHeap.CommitStagedDescriptorsForDraw(m_pCommandList.Get());
	m_pCommandList->DrawInstanced(VertexCount, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void GraphicsCommandContext::DrawIndexedInstanced(UINT IndexCount, UINT InstanceCount, UINT StartIndexLocation, UINT BaseVertexLocation, UINT StartInstanceLocation)
{
	FlushResourceBarriers();
	m_DynamicViewDescriptorHeap.CommitStagedDescriptorsForDraw(m_pCommandList.Get());
	m_DynamicSamplerDescriptorHeap.CommitStagedDescriptorsForDraw(m_pCommandList.Get());
	m_pCommandList->DrawIndexedInstanced(IndexCount, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}
#pragma endregion GraphicsCommandContext

#pragma region ComputeCommandContext
ComputeCommandContext::ComputeCommandContext(RenderDevice* pRenderDevice)
	: RenderCommandContext(pRenderDevice, D3D12_COMMAND_LIST_TYPE_COMPUTE)
{
}

void ComputeCommandContext::SetRootSignature(const RootSignature* pRootSignature)
{
	auto pD3DRootSignature = pRootSignature->GetD3DRootSignature();
	m_DynamicViewDescriptorHeap.ParseRootSignature(pRootSignature);
	m_DynamicSamplerDescriptorHeap.ParseRootSignature(pRootSignature);
	m_pCommandList->SetComputeRootSignature(pD3DRootSignature);
}

void ComputeCommandContext::SetRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

void ComputeCommandContext::SetRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
	m_pCommandList->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}

void ComputeCommandContext::SetRootCBV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);
}

void ComputeCommandContext::SetRootSRV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);
}

void ComputeCommandContext::SetRootUAV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pCommandList->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}

void ComputeCommandContext::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
	FlushResourceBarriers();
	m_DynamicViewDescriptorHeap.CommitStagedDescriptorsForDispatch(m_pCommandList.Get());
	m_DynamicSamplerDescriptorHeap.CommitStagedDescriptorsForDispatch(m_pCommandList.Get());
	m_pCommandList->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}
#pragma endregion ComputeCommandContext

#pragma region CopyCommandContext
CopyCommandContext::CopyCommandContext(RenderDevice* pRenderDevice)
	: RenderCommandContext(pRenderDevice, D3D12_COMMAND_LIST_TYPE_COPY)
{
}
#pragma endregion CopyCommandContext