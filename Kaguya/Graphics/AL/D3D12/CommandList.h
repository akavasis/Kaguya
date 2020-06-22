#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include "ResourceStateTracker.h"
#include "DynamicDescriptorHeap.h"
#include "Descriptor.h"
#include "Buffer.h"
#include "Texture.h"
#include "Heap.h"
#include "RootSignature.h"
#include "PipelineState.h"

class Device;

class CommandList
{
public:
	CommandList(const Device* pDevice, D3D12_COMMAND_LIST_TYPE Type);

	inline auto GetD3DCommandList() { return m_pD3D12CommandList.Get(); }

	// Note: Reset() does not check to see the command queue has finished using the memory thats being allocated by the allocator
	void Reset(PipelineState* pPipelineState);
	void Reset(ID3D12CommandAllocator* pCommandAllocator, PipelineState* pPipelineState);
	void Close();
	bool Close(CommandList* pPendingCommandList, ResourceStateTracker* pGlobalResourceStateTracker);

	// Copy resource from GPU to GPU
	void CopyResource(Resource* pDstResource, Resource* pSrcResource);

	void DrawInstanced(UINT VertexCount, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation);
	void DrawIndexedInstanced(UINT IndexCount, UINT InstanceCount, UINT StartIndexLocation, UINT BaseVertexLocation, UINT StartInstanceLocation);

	void Dispatch(UINT ThreadGroupXCount, UINT ThreadGroupYCount, UINT ThreadGroupZCount);

	void FlushResourceBarriers();
	void TransitionBarrier(Resource* pResource, ResourceStates TransitionState, UINT Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void AliasingBarrier(Resource* pBeforeResource, Resource* pAfterResource);
	void UAVBarrier(Resource* pResource);

	void IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology);
	void IASetVertexBuffers(UINT StartSlot, UINT NumViews, Buffer* const* ppVertexBuffers);
	void IASetIndexBuffer(Buffer* pIndexBuffer);

	void RSSetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports);
	void RSSetScissorRects(UINT NumRects, const D3D12_RECT* pRects);

	void ClearRenderTargetView(D3D12_CPU_DESCRIPTOR_HANDLE RenderTargetView, const FLOAT ColorRGBA[4], UINT NumRects, const D3D12_RECT* pRects);
	void ClearDepthStencilView(D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects);

	void SetPipelineState(ID3D12PipelineState* pPipelineState);

	void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType, ID3D12DescriptorHeap* pDescriptorHeap);

	// Graphics Root Signature Operations
	void SetGraphicsRootSignature(const RootSignature& pRootSignature);
	void SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);
	void SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues);
	void SetGraphicsRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void SetGraphicsRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void SetGraphicsRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

	// Compute Root Signature Operations
	void SetComputeRootSignature(const RootSignature& pRootSignature);
	void SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);
	void SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues);
	void SetComputeRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void SetComputeRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void SetComputeRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

	void SetShaderResourceView(UINT RootParameterIndex, UINT DescriptorOffset, Resource* pGPUResource, const D3D12_SHADER_RESOURCE_VIEW_DESC* SRVDesc = nullptr);
	void SetUnorderedAccessView(UINT RootParameterIndex, UINT DescriptorOffset, Resource* pGPUResource, const D3D12_UNORDERED_ACCESS_VIEW_DESC* UAVDesc = nullptr);
private:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_pD3D12CommandAllocator;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pD3D12CommandList;

	// Keep track of the currently bound descriptor heaps. Only change descriptor 
	// heaps if they are different than the currently bound descriptor heaps.
	ID3D12DescriptorHeap* m_pCurrentDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	// Pending resource transitions are committed to a separate commandlist before this commandlist
	// is executed on the command queue. This guarantees that resources will
	// be in the expected state at the beginning of a command list.
	std::vector<ResourceBarrier> m_PendingResourceBarriers;
	// Resource barriers that need to be committed to the command list.
	std::vector<ResourceBarrier> m_ResourceBarriers;
	// Resource state tracker for this command list
	ResourceStateTracker m_ResourceStateTracker;

	DynamicDescriptorHeap m_pDynamicViewDescriptorHeap;
	DynamicDescriptorHeap m_pDynamicSamplerDescriptorHeap;
};