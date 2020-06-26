#pragma once
#include <d3d12.h>

#include "AL/D3D12/ResourceStateTracker.h"
#include "AL/D3D12/DynamicDescriptorHeap.h"

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
	inline auto GetD3DCommandList() const { return m_pCommandList.Get(); }

	void SetPipelineState(const PipelineState* pPipelineState);
	void SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE Type, ID3D12DescriptorHeap* pDescriptorHeap);

	void TransitionBarrier(Resource* pResource, D3D12_RESOURCE_STATES TransitionState, UINT Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void AliasingBarrier(Resource* pBeforeResource, Resource* pAfterResource);
	void UAVBarrier(Resource* pResource);
	void FlushResourceBarriers();

	void CopyResource(Resource* pDstResource, Resource* pSrcResource);

	void SetSRV(UINT RootParameterIndex, UINT DescriptorOffset, Resource* pResource, D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle);
	void SetUAV(UINT RootParameterIndex, UINT DescriptorOffset, Resource* pResource, D3D12_CPU_DESCRIPTOR_HANDLE CPUHandle);
protected:
	RenderCommandContext(RenderDevice* pRenderDevice, D3D12_COMMAND_LIST_TYPE Type);

	bool Close(UINT64 FenceValue, ResourceStateTracker* pGlobalResourceStateTracker);
	void Reset();

	CommandQueue* pOwningCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_pCommandList;
	ID3D12CommandAllocator* m_pCurrentAllocator; // Given by the CommandQueue
	D3D12_COMMAND_LIST_TYPE m_Type;

	// Keep track of the currently bound descriptor heaps. Only change descriptor 
	// heaps if they are different than the currently bound descriptor heaps.
	ID3D12DescriptorHeap* m_pCurrentDescriptorHeaps[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	// Pending resource transitions are committed to a separate commandlist before this commandlist
	// is executed on the command queue. This guarantees that resources will
	// be in the expected state at the beginning of a command list.
	std::vector<D3D12_RESOURCE_BARRIER> m_PendingResourceBarriers;
	// Resource barriers that need to be committed to the command list.
	std::vector<D3D12_RESOURCE_BARRIER> m_ResourceBarriers;
	// Resource state tracker for this command list
	ResourceStateTracker m_ResourceStateTracker;

	DynamicDescriptorHeap m_DynamicViewDescriptorHeap;
	DynamicDescriptorHeap m_DynamicSamplerDescriptorHeap;
};

class GraphicsCommandContext : public RenderCommandContext
{
public:
	void SetRootSignature(const RootSignature* pRootSignature);
	void SetRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);
	void SetRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues);
	void SetRootCBV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void SetRootSRV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void SetRootUAV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

	void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology);
	void SetVertexBuffers(UINT StartSlot, UINT NumViews, Buffer* const* ppVertexBuffers);
	void SetIndexBuffer(Buffer* pIndexBuffer);

	void SetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports);
	void SetScissorRects(UINT NumRects, const D3D12_RECT* pRects);

	void DrawInstanced(UINT VertexCount, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation);
	void DrawIndexedInstanced(UINT IndexCount, UINT InstanceCount, UINT StartIndexLocation, UINT BaseVertexLocation, UINT StartInstanceLocation);
private:
	GraphicsCommandContext(RenderDevice* pRenderDevice);
};

class ComputeCommandContext : public RenderCommandContext
{
public:
	void SetRootSignature(const RootSignature* pRootSignature);
	void SetRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);
	void SetRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues);
	void SetRootCBV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void SetRootSRV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	void SetRootUAV(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

	void Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ);
private:
	ComputeCommandContext(RenderDevice* pRenderDevice);
};

class CopyCommandContext : public RenderCommandContext
{
public:

private:
	CopyCommandContext(RenderDevice* pRenderDevice);
};