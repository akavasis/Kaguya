#include "pch.h"
#include "CommandList.h"
#include "Device.h"
using Microsoft::WRL::ComPtr;

CommandList::CommandList(const Device* pDevice, D3D12_COMMAND_LIST_TYPE Type)
	: m_pCurrentDescriptorHeaps{ nullptr },
	m_pDynamicViewDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV),
	m_pDynamicSamplerDescriptorHeap(pDevice, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
{
	auto pD3DDevice = pDevice->GetD3DDevice();
	ThrowCOMIfFailed(pD3DDevice->CreateCommandAllocator(Type, IID_PPV_ARGS(&m_pD3D12CommandAllocator)));
	ThrowCOMIfFailed(pD3DDevice->CreateCommandList(0, Type, m_pD3D12CommandAllocator.Get(), nullptr, IID_PPV_ARGS(&m_pD3D12CommandList)));
	ThrowCOMIfFailed(m_pD3D12CommandList->Close());
}

CommandList::~CommandList()
{
}

void CommandList::Reset(PipelineState* pPipelineState)
{
	Reset(m_pD3D12CommandAllocator.Get(), pPipelineState);
}

void CommandList::Reset(ID3D12CommandAllocator* pCommandAllocator, PipelineState* pPipelineState)
{
	if (pCommandAllocator)
	{
		// Reset bounded api objects 
		for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
			m_pCurrentDescriptorHeaps[i] = nullptr;

		// Reset resource state tracking and resource barriers 
		m_ResourceStateTracker.Reset();
		m_ResourceBarriers.clear();
		m_PendingResourceBarriers.clear();

		// Reset dynamic descriptor heap 
		m_pDynamicViewDescriptorHeap.Reset();
		m_pDynamicSamplerDescriptorHeap.Reset();

		// Reset command allocator and commandlist 
		ThrowCOMIfFailed(pCommandAllocator->Reset());
		ThrowCOMIfFailed(m_pD3D12CommandList->Reset(pCommandAllocator, pPipelineState->GetD3DPipelineState()));
	}
}

void CommandList::Close()
{
	FlushResourceBarriers();
	ThrowCOMIfFailed(m_pD3D12CommandList->Close());
}

bool CommandList::Close(CommandList* pPendingCommandList, ResourceStateTracker* pGlobalResourceStateTracker)
{
	Close();

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
					D3D12_RESOURCE_BARRIER newBarrier = GetD3DResourceBarrier(pendingBarrier);
					newBarrier.Transition.Subresource = subresourceState.first;
					newBarrier.Transition.StateBefore = GetD3DResourceStates(subresourceState.second);
					resourceBarriers.push_back(newBarrier);
				}
			}
		}
		else
		{
			// No (sub)resources need to be transitioned. Just add a single transition barrier (if needed). 
			ResourceStates globalState = resourceState->GetSubresourceState(pendingTransition.Subresource);
			if (pendingTransition.StateAfter != globalState)
			{
				// Fix-up the before state based on current global state of the resource. 
				pendingBarrier.Transition.StateBefore = globalState;
				resourceBarriers.push_back(GetD3DResourceBarrier(pendingBarrier));
			}
		}
	}

	UINT numBarriers = static_cast<UINT>(resourceBarriers.size());
	if (numBarriers > 0)
	{
		auto pD3DCommandList = pPendingCommandList->GetD3DCommandList();
		pD3DCommandList->ResourceBarrier(numBarriers, resourceBarriers.data());
	}

	m_PendingResourceBarriers.clear();

	pGlobalResourceStateTracker->UpdateResourceStates(m_ResourceStateTracker);
	m_ResourceStateTracker.Reset();
	return numBarriers > 0;
}

void CommandList::CopyResource(Resource* pDstResource, Resource* pSrcResource)
{
	TransitionBarrier(pDstResource, D3D12_RESOURCE_STATE_COPY_DEST);
	TransitionBarrier(pSrcResource, D3D12_RESOURCE_STATE_COPY_SOURCE);
	m_pD3D12CommandList->CopyResource(pDstResource->GetD3DResource(), pSrcResource->GetD3DResource());
}

void CommandList::DrawInstanced(UINT VertexCount, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation)
{
	FlushResourceBarriers();
	m_pDynamicViewDescriptorHeap.CommitStagedDescriptorsForDraw(this);
	m_pDynamicSamplerDescriptorHeap.CommitStagedDescriptorsForDraw(this);
	m_pD3D12CommandList->DrawInstanced(VertexCount, InstanceCount, StartVertexLocation, StartInstanceLocation);
}

void CommandList::DrawIndexedInstanced(UINT IndexCount, UINT InstanceCount, UINT StartIndexLocation, UINT BaseVertexLocation, UINT StartInstanceLocation)
{
	FlushResourceBarriers();
	m_pDynamicViewDescriptorHeap.CommitStagedDescriptorsForDraw(this);
	m_pDynamicSamplerDescriptorHeap.CommitStagedDescriptorsForDraw(this);
	m_pD3D12CommandList->DrawIndexedInstanced(IndexCount, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation);
}

void CommandList::Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ)
{
	FlushResourceBarriers();
	m_pDynamicViewDescriptorHeap.CommitStagedDescriptorsForDispatch(this);
	m_pDynamicSamplerDescriptorHeap.CommitStagedDescriptorsForDispatch(this);
	m_pD3D12CommandList->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}

void CommandList::FlushResourceBarriers()
{
	UINT numBarriers = static_cast<UINT>(m_ResourceBarriers.size());
	if (numBarriers > 0)
	{
		m_pD3D12CommandList->ResourceBarrier(numBarriers, m_ResourceBarriers.data());
		m_ResourceBarriers.clear();
	}
}

void CommandList::TransitionBarrier(Resource* pResource, ResourceStates TransitionState, UINT Subresource)
{
	// Transition barriers indicate that a set of subresources transition between different usages.  
	// The caller must specify the before and after usages of the subresources.  
	// The D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES flag is used to transition all subresources in a resource at the same time. 
	D3D12_RESOURCE_BARRIER resourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(pResource->GetD3DResource(), D3D12_RESOURCE_STATE_COMMON, TransitionState, Subresource);
	const D3D12_RESOURCE_TRANSITION_BARRIER& transitionBarrier = resourceBarrier.Transition;

	// First check if there is already a known state for the given resource. 
	// If there is, the resource has been used on the command list before and 
	// already has a known state within the command list execution. 
	auto resourceState = m_ResourceStateTracker.Find(transitionBarrier.pResource);
	if (resourceState.has_value())
	{
		// If the known state of the resource is different... 
		if (transitionBarrier.Subresource == D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES &&
			!resourceState->SubresourceState.empty())
		{
			// First transition all of the subresources if they are different than the StateAfter. 
			for (auto subresourceState : resourceState->SubresourceState)
			{
				if (transitionBarrier.StateAfter != subresourceState.second)
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
			D3D12_RESOURCE_STATES finalState = resourceState->GetSubresourceState(transitionBarrier.Subresource);
			if (transitionBarrier.StateAfter != finalState)
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
	m_ResourceStateTracker.SetResourceState(transitionBarrier.pResource, transitionBarrier.Subresource, transitionBarrier.StateAfter);
}

void CommandList::AliasingBarrier(Resource* pBeforeResource, Resource* pAfterResource)
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
		barrier = CD3DX12_RESOURCE_BARRIER::Aliasing(pBeforeResource->Resource().Get(), pAfterResource->Resource().Get());
	m_ResourceBarriers.push_back(barrier);
}

void CommandList::UAVBarrier(Resource* pResource)
{
	// A UAV barrier indicates that all UAV accesses, both read or write,  
	// to a particular resource must complete between any future UAV accesses, both read or write.  
	// It's not necessary for an app to put a UAV barrier between two draw or dispatch calls that only read from a UAV.  
	// Also, it's not necessary to put a UAV barrier between two draw or dispatch calls that  
	// write to the same UAV if the application knows that it is safe to execute the UAV access in any order.  
	// A D3D12_RESOURCE_UAV_BARRIER structure is used to specify the UAV resource to which the barrier applies.  
	// The application can specify NULL for the barrier's UAV, which indicates that any UAV access could require the barrier. 
	if (!pResource || !pResource->Resource()) return;
	m_ResourceBarriers.push_back(CD3DX12_RESOURCE_BARRIER::UAV(pResource->GetD3DResource()));
}

void CommandList::IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology)
{
	m_pD3D12CommandList->IASetPrimitiveTopology(PrimitiveTopology);
}

void CommandList::IASetVertexBuffers(UINT StartSlot, UINT NumViews, Buffer* const* ppVertexBuffers)
{
	if (ppVertexBuffers)
	{
		std::vector<D3D12_VERTEX_BUFFER_VIEW> D3D12VertexBufferViews(NumViews);
		for (UINT i = 0; i < NumViews; ++i)
		{
			D3D12VertexBufferViews[i].BufferLocation = ppVertexBuffers[i]->GetD3DResource()->GetGPUVirtualAddress();
			D3D12VertexBufferViews[i].SizeInBytes = UINT64(ppVertexBuffers[i]->GetNumElements()) * UINT64(ppVertexBuffers[i]->GetStride());
			D3D12VertexBufferViews[i].StrideInBytes = ppVertexBuffers[i]->GetStride();
		}
		m_pD3D12CommandList->IASetVertexBuffers(StartSlot, NumViews, D3D12VertexBufferViews.data());
	}
	else
	{
		m_pD3D12CommandList->IASetVertexBuffers(0u, 0u, nullptr);
	}
}

void CommandList::IASetIndexBuffer(Buffer* pIndexBuffer)
{
	if (pIndexBuffer && pIndexBuffer->GetD3DResource())
	{
		D3D12_INDEX_BUFFER_VIEW indexBufferView;
		indexBufferView.BufferLocation = pIndexBuffer->GetD3DResource()->GetGPUVirtualAddress();
		indexBufferView.SizeInBytes = UINT64(pIndexBuffer->GetNumElements()) * UINT64(pIndexBuffer->GetStride());
		indexBufferView.Format = pIndexBuffer->GetD3DResource()->GetDesc().Format;
		m_pD3D12CommandList->IASetIndexBuffer(&indexBufferView);
	}
	else
		m_pD3D12CommandList->IASetIndexBuffer(nullptr);
}

void CommandList::RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports)
{
	m_pD3D12CommandList->RSSetViewports(NumViewports, pViewports);
}

void CommandList::RSSetScissorRects(UINT NumRects, const D3D12_RECT* pRects)
{
	m_pD3D12CommandList->RSSetScissorRects(NumRects, pRects);
}

void CommandList::ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT* pRects)
{
	m_pD3D12CommandList->ClearRenderTargetView(RenderTargetView, ColorRGBA, NumRects, pRects);
}

void CommandList::ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects)
{
	m_pD3D12CommandList->ClearDepthStencilView(DepthStencilView, ClearFlags, Depth, Stencil, NumRects, pRects);
}

void CommandList::SetPipelineState(ID3D12PipelineState* pPipelineState)
{
	m_pD3D12CommandList->SetPipelineState(pPipelineState);
}

void CommandList::SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType, ID3D12DescriptorHeap* pDescriptorHeap)
{
	if (!pDescriptorHeap) return;
	if (m_pCurrentDescriptorHeaps[DescriptorHeapType] == pDescriptorHeap) return;

	m_pCurrentDescriptorHeaps[DescriptorHeapType] = pDescriptorHeap;

	UINT NumDescriptorHeaps = 0;
	ID3D12DescriptorHeap* DescriptorHeapsToBind[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES] = {};

	for (UINT i = 0; i < D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES; ++i)
	{
		ID3D12DescriptorHeap* DescriptorHeapIter = m_pCurrentDescriptorHeaps[i];
		if (DescriptorHeapIter)
			DescriptorHeapsToBind[NumDescriptorHeaps++] = DescriptorHeapIter;
	}

	if (NumDescriptorHeaps > 0)
		m_pD3D12CommandList->SetDescriptorHeaps(NumDescriptorHeaps, DescriptorHeapsToBind);
}

void CommandList::SetGraphicsRootSignature(const RootSignature& pRootSignature)
{
	auto pD3DRootSignature = pRootSignature.GetD3DRootSignature();
	m_pDynamicViewDescriptorHeap.ParseRootSignature(pRootSignature);
	m_pDynamicSamplerDescriptorHeap.ParseRootSignature(pRootSignature);
	m_pD3D12CommandList->SetGraphicsRootSignature(pD3DRootSignature);
}

void CommandList::SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	m_pD3D12CommandList->SetGraphicsRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

void CommandList::SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
	m_pD3D12CommandList->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}

void CommandList::SetGraphicsRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pD3D12CommandList->SetGraphicsRootConstantBufferView(RootParameterIndex, BufferLocation);
}

void CommandList::SetGraphicsRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pD3D12CommandList->SetGraphicsRootShaderResourceView(RootParameterIndex, BufferLocation);
}

void CommandList::SetGraphicsRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pD3D12CommandList->SetGraphicsRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}

void CommandList::SetComputeRootSignature(const RootSignature& pRootSignature)
{
	auto pD3DRootSignature = pRootSignature.GetD3DRootSignature();
	m_pDynamicViewDescriptorHeap.ParseRootSignature(pRootSignature);
	m_pDynamicSamplerDescriptorHeap.ParseRootSignature(pRootSignature);
	m_pD3D12CommandList->SetComputeRootSignature(pD3DRootSignature);
}

void CommandList::SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues)
{
	m_pD3D12CommandList->SetComputeRoot32BitConstant(RootParameterIndex, SrcData, DestOffsetIn32BitValues);
}

void CommandList::SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
	m_pD3D12CommandList->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
}

void CommandList::SetComputeRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pD3D12CommandList->SetComputeRootConstantBufferView(RootParameterIndex, BufferLocation);
}

void CommandList::SetComputeRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pD3D12CommandList->SetComputeRootShaderResourceView(RootParameterIndex, BufferLocation);
}

void CommandList::SetComputeRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation)
{
	m_pD3D12CommandList->SetComputeRootUnorderedAccessView(RootParameterIndex, BufferLocation);
}

void CommandList::SetShaderResourceView(UINT RootParameterIndex,
	UINT DescriptorOffset,
	Resource* pGPUResource,
	const D3D12_SHADER_RESOURCE_VIEW_DESC* SRVDesc)
{
	if (!pGPUResource) return;
	m_pDynamicViewDescriptorHeap.StageDescriptors(RootParameterIndex, DescriptorOffset, 1, pGPUResource->SRV(SRVDesc));
}

void CommandList::SetUnorderedAccessView(UINT RootParameterIndex,
	UINT DescriptorOffset,
	Resource* pGPUResource,
	const D3D12_UNORDERED_ACCESS_VIEW_DESC* UAVDesc)
{
	if (!pGPUResource) return;
	m_pDynamicViewDescriptorHeap.StageDescriptors(RootParameterIndex, DescriptorOffset, 1, pGPUResource->UAV(UAVDesc));
}