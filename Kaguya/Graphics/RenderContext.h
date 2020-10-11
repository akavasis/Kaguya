#pragma once
#include "RenderDevice.h"

/*
	High level command context
*/
class RenderContext
{
public:
	RenderContext(size_t RenderPassIndex,
		Buffer* pGpuData,
		RenderDevice* pRenderDevice,
		CommandContext* pCommandContext)
		: m_RenderPassIndex(RenderPassIndex),
		m_pGpuData(pGpuData),
		m_pRenderDevice(pRenderDevice),
		m_pCommandContext(pCommandContext)
	{
	}

	inline auto GetCommandContext() const { return m_pCommandContext; }

	inline auto GetCurrentSwapChainResourceHandle() const
	{
		return m_pRenderDevice->SwapChainTextures[m_pRenderDevice->FrameIndex];
	}

	[[nodiscard]] inline auto GetBuffer(RenderResourceHandle Handle) const { return m_pRenderDevice->GetBuffer(Handle); }
	[[nodiscard]] inline auto GetTexture(RenderResourceHandle Handle) const { return m_pRenderDevice->GetTexture(Handle); }

	Descriptor GetShaderResourceView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> MostDetailedMip = {}, std::optional<UINT> MipLevels = {}) const
	{
		return m_pRenderDevice->GetShaderResourceView(RenderResourceHandle, MostDetailedMip, MipLevels);
	}
	Descriptor GetUnorderedAccessView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}) const
	{
		return m_pRenderDevice->GetUnorderedAccessView(RenderResourceHandle, ArraySlice, MipSlice);
	}
	Descriptor GetRenderTargetView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const
	{
		return m_pRenderDevice->GetRenderTargetView(RenderResourceHandle, ArraySlice, MipSlice, ArraySize);
	}
	Descriptor GetDepthStencilView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const
	{
		return m_pRenderDevice->GetDepthStencilView(RenderResourceHandle, ArraySlice, MipSlice, ArraySize);
	}

	template<typename T>
	void UpdateRenderPassData(const T& Data)
	{
		static_assert(sizeof(T) < RenderPass::GpuDataByteSize);

		auto pCpuPtr = m_pGpuData->Map();
		m_pGpuData->Update<T>(m_RenderPassIndex, Data);
	}

	void TransitionBarrier(RenderResourceHandle ResourceHandle, Resource::State TransitionState, UINT Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void AliasingBarrier(RenderResourceHandle BeforeResourceHandle, RenderResourceHandle AfterResourceHandle);
	void UAVBarrier(RenderResourceHandle ResourceHandle);

	/*
		Bind PSO and Root Signature given a PSO handle, also binds shader layout resources used for bindless
	*/
	void SetPipelineState(RenderResourceHandle PipelineStateHandle);

	void SetRootShaderResourceView(UINT RootParameterIndex, RenderResourceHandle BufferHandle);

	void DispatchRays
	(
		RenderResourceHandle RayGenerationShaderTable,
		RenderResourceHandle MissShaderTable,
		RenderResourceHandle HitGroupShaderTable,
		UINT Width, UINT Height, UINT Depth = 1
	);

	auto operator->() const { return m_pCommandContext; }
private:
	void SetGraphicsPSO(GraphicsPipelineState* pGraphicsPipelineState);
	void SetComputePSO(ComputePipelineState* pComputePipelineState);
	void SetRaytracingPSO(RaytracingPipelineState* pRaytracingPipelineState);

	void BindGraphicsShaderLayoutResource(const RootSignature* pRootSignature);
	void BindComputeShaderLayoutResource(const RootSignature* pRootSignature);

	size_t m_RenderPassIndex;
	Buffer* m_pGpuData;
	RenderDevice* m_pRenderDevice;
	CommandContext* m_pCommandContext;

	RenderResourceHandle m_PipelineStateHandle;
};