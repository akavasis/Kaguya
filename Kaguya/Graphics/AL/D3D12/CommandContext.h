#pragma once
#include <d3d12.h>
#include "ResourceStateTracker.h"
#include "DescriptorHeap.h"
#include "CommandQueue.h"
#include "DeviceBuffer.h"
#include "DeviceTexture.h"
#include "Heap.h"
#include "RootSignature.h"
#include "PipelineState.h"
#include "RaytracingPipelineState.h"

class CommandContext
{
public:
	enum Type
	{
		Direct,
		Compute,
		Copy,
		NumTypes
	};

	CommandContext(const Device* pDevice, CommandQueue* pCommandQueue, Type Type);

	inline auto GetD3DCommandList() const { return m_pCommandList.Get(); }
	inline auto GetType() const { return m_Type; }

	void SetPipelineState(const PipelineState* pPipelineState);
	void SetDescriptorHeaps(CBSRUADescriptorHeap* pCBSRUADescriptorHeap, SamplerDescriptorHeap* pSamplerDescriptorHeap);

	void TransitionBarrier(DeviceResource* pResource, DeviceResource::State TransitionState, UINT Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void AliasingBarrier(DeviceResource* pBeforeResource, DeviceResource* pAfterResource);
	void UAVBarrier(DeviceResource* pResource);
	void FlushResourceBarriers();

	void CopyBufferRegion(DeviceBuffer* pDstBuffer, UINT64 DstOffset, DeviceBuffer* pSrcBuffer, UINT64 SrcOffset, UINT64 NumBytes);
	void CopyTextureRegion(const D3D12_TEXTURE_COPY_LOCATION* pDst, UINT DstX, UINT DstY, UINT DstZ, const D3D12_TEXTURE_COPY_LOCATION* pSrc, const D3D12_BOX* pSrcBox);
	void CopyResource(DeviceResource* pDstResource, DeviceResource* pSrcResource);

	// Graphics operations
	inline void SetGraphicsRootSignature(const RootSignature* pRootSignature);
	inline void SetGraphicsRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor);
	inline void SetGraphicsRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);
	inline void SetGraphicsRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues);
	inline void SetGraphicsRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	inline void SetGraphicsRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	inline void SetGraphicsRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

	inline void SetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY PrimitiveTopology);
	inline void SetVertexBuffers(UINT StartSlot, UINT NumViews, const D3D12_VERTEX_BUFFER_VIEW* pViews);
	inline void SetIndexBuffer(const D3D12_INDEX_BUFFER_VIEW* pView);

	inline void SetViewports(UINT NumViewports, const D3D12_VIEWPORT* pViewports);
	inline void SetScissorRects(UINT NumRects, const D3D12_RECT* pRects);
	inline void SetRenderTargets(UINT NumRenderTargetDescriptors, Descriptor* pRenderTargetDescriptors, BOOL RTsSingleHandleToDescriptorRange, Descriptor DepthStencilDescriptor);
	inline void ClearRenderTarget(Descriptor RenderTargetDescriptor, const FLOAT Color[4], UINT NumRects, const D3D12_RECT* pRects);
	inline void ClearDepthStencil(Descriptor DepthStencilDescriptor, D3D12_CLEAR_FLAGS ClearFlags, FLOAT Depth, UINT8 Stencil, UINT NumRects, const D3D12_RECT* pRects);

	void DrawInstanced(UINT VertexCount, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation);
	void DrawIndexedInstanced(UINT IndexCount, UINT InstanceCount, UINT StartIndexLocation, UINT BaseVertexLocation, UINT StartInstanceLocation);

	// Compute operations
	inline void SetComputeRootSignature(const RootSignature* pRootSignature);
	inline void SetComputeRootDescriptorTable(UINT RootParameterIndex, D3D12_GPU_DESCRIPTOR_HANDLE BaseDescriptor);
	inline void SetComputeRoot32BitConstant(UINT RootParameterIndex, UINT SrcData, UINT DestOffsetIn32BitValues);
	inline void SetComputeRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues);
	inline void SetComputeRootConstantBufferView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	inline void SetComputeRootShaderResourceView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);
	inline void SetComputeRootUnorderedAccessView(UINT RootParameterIndex, D3D12_GPU_VIRTUAL_ADDRESS BufferLocation);

	void Dispatch(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ);
	void Dispatch1D(UINT ThreadGroupCountX, UINT ThreadSizeX);
	void Dispatch2D(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadSizeX, UINT ThreadSizeY);
	void Dispatch3D(UINT ThreadGroupCountX, UINT ThreadGroupCountY, UINT ThreadGroupCountZ, UINT ThreadSizeX, UINT ThreadSizeY, UINT ThreadSizeZ);

	void BuildRaytracingAccelerationStructure(const D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC* pDesc,
		UINT NumPostbuildInfoDescs,
		const D3D12_RAYTRACING_ACCELERATION_STRUCTURE_POSTBUILD_INFO_DESC* pPostbuildInfoDescs);

	void SetRaytracingPipelineState(RaytracingPipelineState* pPipelineState)
	{
		m_pCommandList->SetPipelineState1(pPipelineState->GetD3DPipelineState());
	}

	void DispatchRays(const D3D12_DISPATCH_RAYS_DESC* pDesc)
	{
		FlushResourceBarriers();
		m_pCommandList->DispatchRays(pDesc);
	}
private:
	friend class RenderDevice;

	bool Close(ResourceStateTracker& GlobalResourceStateTracker);
	void Reset();
	// Marks current allocator as active with the FenceValue and request a new one
	void RequestNewAllocator(UINT64 FenceValue);

	CommandQueue*										pOwningCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>	m_pCommandList;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4>	m_pPendingCommandList;
	ID3D12CommandAllocator*								m_pCurrentAllocator; // Given by the CommandQueue
	ID3D12CommandAllocator*								m_pCurrentPendingAllocator; // Given by the CommandQueue
	Type												m_Type;

	// Resource state tracker for this command list
	ResourceStateTracker								m_ResourceStateTracker;
};

#include "CommandContext.inl"

D3D12_COMMAND_LIST_TYPE GetD3DCommandListType(CommandContext::Type Type);