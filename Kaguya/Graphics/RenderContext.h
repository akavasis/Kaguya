#pragma once
#include "RenderDevice.h"

/*
	High level command context
*/
class RenderContext
{
public:
	RenderContext() = default;
	RenderContext(size_t RenderPassIndex,
		Buffer* pSystemConstants,
		Buffer* pGpuData,
		RenderDevice* pRenderDevice,
		CommandContext* pCommandContext)
		: SV_RenderPassIndex(RenderPassIndex),
		SV_pSystemConstants(pSystemConstants),
		SV_pGpuData(pGpuData),
		SV_pRenderDevice(pRenderDevice),
		SV_pCommandContext(pCommandContext)
	{
	}

	inline auto GetCommandContext() const { return SV_pCommandContext; }

	inline auto GetCurrentBackBufferHandle() const
	{
		return SV_pRenderDevice->GetCurrentBackBufferHandle();
	}

	[[nodiscard]] inline auto GetBuffer(RenderResourceHandle Handle) const { return SV_pRenderDevice->GetBuffer(Handle); }
	[[nodiscard]] inline auto GetTexture(RenderResourceHandle Handle) const { return SV_pRenderDevice->GetTexture(Handle); }

	Descriptor GetShaderResourceView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> MostDetailedMip = {}, std::optional<UINT> MipLevels = {}) const
	{
		return SV_pRenderDevice->GetShaderResourceView(RenderResourceHandle, MostDetailedMip, MipLevels);
	}
	Descriptor GetUnorderedAccessView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}) const
	{
		return SV_pRenderDevice->GetUnorderedAccessView(RenderResourceHandle, ArraySlice, MipSlice);
	}
	Descriptor GetRenderTargetView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const
	{
		return SV_pRenderDevice->GetRenderTargetView(RenderResourceHandle, ArraySlice, MipSlice, ArraySize);
	}
	Descriptor GetDepthStencilView(RenderResourceHandle RenderResourceHandle, std::optional<UINT> ArraySlice = {}, std::optional<UINT> MipSlice = {}, std::optional<UINT> ArraySize = {}) const
	{
		return SV_pRenderDevice->GetDepthStencilView(RenderResourceHandle, ArraySlice, MipSlice, ArraySize);
	}

	template<typename T>
	void UpdateRenderPassData(const T& Data)
	{
		auto pCpuPtr = SV_pGpuData->Map();
		SV_pGpuData->Update<T>(SV_RenderPassIndex, Data);
	}

	void CopyResource(RenderResourceHandle Dst, RenderResourceHandle Src);

	void TransitionBarrier(RenderResourceHandle ResourceHandle, Resource::State TransitionState, UINT Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES);
	void AliasingBarrier(RenderResourceHandle BeforeResourceHandle, RenderResourceHandle AfterResourceHandle);
	void UAVBarrier(RenderResourceHandle ResourceHandle);

	/*
		Bind PSO and Root Signature given a PSO handle, also binds shader layout resources used for bindless
	*/
	void SetPipelineState(RenderResourceHandle PipelineStateHandle);

	void SetRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues);
	void SetRootShaderResourceView(UINT RootParameterIndex, RenderResourceHandle BufferHandle);

	void DispatchRays
	(
		RenderResourceHandle RayGenerationShaderTable,
		RenderResourceHandle MissShaderTable,
		RenderResourceHandle HitGroupShaderTable,
		UINT Width, UINT Height, UINT Depth = 1
	);

	auto operator->() const { return SV_pCommandContext; }
private:
	void SetPipelineState(PipelineState* pPipelineState);
	void SetRaytracingPipelineState(RaytracingPipelineState* pRaytracingPipelineState);

	void BindGraphicsShaderLayoutResource(const RootSignature* pRootSignature);
	void BindComputeShaderLayoutResource(const RootSignature* pRootSignature);

	// SV have the same notion as Shader's SV, it is provided by the RenderGraph
	size_t					SV_RenderPassIndex		= 0;
	Buffer*					SV_pSystemConstants		= nullptr;
	Buffer*					SV_pGpuData				= nullptr;
	RenderDevice*			SV_pRenderDevice		= nullptr;
	CommandContext*			SV_pCommandContext		= nullptr;

	RenderResourceHandle	m_PipelineStateHandle	= {};
	PipelineState::Type		m_PipelineStateType		= PipelineState::Type::Unknown;
};