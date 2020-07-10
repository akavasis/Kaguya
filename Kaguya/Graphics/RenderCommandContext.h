#pragma once
#include <d3d12.h>

#include "AL/D3D12/ResourceStateTracker.h"
#include "AL/D3D12/DynamicDescriptorHeap.h"
#include "AL/D3D12/DescriptorHeap.h"

class RenderDevice;

class Resource;
class Buffer;
class Texture;
class RootSignature;
class PipelineState;
class GraphicsPipelineState;
class ComputePipelineState;

class RenderCommandContext
{
public:
	RenderCommandContext(RenderDevice* pRenderDevice, D3D12_COMMAND_LIST_TYPE Type);

	inline auto GetD3DCommandList() const { return m_pCommandList.Get(); }

	void SetPipelineState(const PipelineState* pPipelineState);
	void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* pDescriptorHeap);

	void TransitionBarrier(Resource* pResource, D3D12_RESOURCE_STATES TransitionState, UINT Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void AliasingBarrier(Resource* pBeforeResource, Resource* pAfterResource);
	void UAVBarrier(Resource* pResource);
	void FlushResourceBarriers();

	void CopyResource(Resource* pDstResource, Resource* pSrcResource);
	void CopyBufferRegion(Buffer* pDstBuffer, UINT64 DstOffset, Buffer* pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes);

	void SetSRV(UINT RootParameterIndex, UINT DescriptorOffset, UINT NumHandlesWithinDescriptor, Descriptor Descriptor);
	void SetUAV(UINT RootParameterIndex, UINT DescriptorOffset, UINT NumHandlesWithinDescriptor, Descriptor Descriptor);

	// Graphics operations
	void SetGraphicsRootSignature(const RootSignature* pRootSignature);
	inline void SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);
	inline void SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues);
	inline void SetGraphicsRootCBV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	inline void SetGraphicsRootSRV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	inline void SetGraphicsRootUAV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

	inline void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology);
	inline void SetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW* pViews);
	inline void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView);

	inline void SetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports);
	inline void SetScissorRects(UINT NumRects, const D3D12_RECT* pRects);

	void DrawInstanced(UINT VertexCount, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation);
	void DrawIndexedInstanced(UINT IndexCount, UINT InstanceCount, UINT StartIndexLocation, UINT BaseVertexLocation, UINT StartInstanceLocation);

	// Compute operations
	void SetComputeRootSignature(const RootSignature* pRootSignature);
	inline void SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);
	inline void SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues);
	inline void SetComputeRootCBV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	inline void SetComputeRootSRV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	inline void SetComputeRootUAV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

	void Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ);
protected:
	friend class RenderDevice;

	bool Close(ResourceStateTracker& GlobalResourceStateTracker);
	void Reset();
	// Marks current allocator as active with the FenceValue and request a new one
	void RequestNewAllocator(UINT64 FenceValue);

	class CommandQueue* pOwningCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCommandList;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pPendingCommandList;
	ID3D12CommandAllocator* m_pCurrentAllocator; // Given by the CommandQueue
	ID3D12CommandAllocator* m_pCurrentPendingAllocator; // Given by the CommandQueue
	D3D12_COMMAND_LIST_TYPE m_Type;

	// Keep track of the currently bound descriptor heaps. Only change descriptor 
	// heaps if they are different than the currently bound descriptor heaps.
	ID3D12DescriptorHeap* m_pCurrentDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	// Resource state tracker for this command list
	ResourceStateTracker m_ResourceStateTracker;

	DynamicDescriptorHeap m_DynamicViewDescriptorHeap;
	DynamicDescriptorHeap m_DynamicSamplerDescriptorHeap;
};

#include "RenderCommandContext.inl"