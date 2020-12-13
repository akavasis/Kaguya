#include "pch.h"
#include "RenderContext.h"
#include "RendererRegistry.h"

void RenderContext::CopyResource(RenderResourceHandle Dst, RenderResourceHandle Src)
{
	assert(Dst.Type == Src.Type);
	switch (Dst.Type)
	{
	case RenderResourceType::Buffer:
		SV_pCommandContext->CopyResource(SV_pRenderDevice->GetBuffer(Dst), SV_pRenderDevice->GetBuffer(Src));
		break;

	case RenderResourceType::Texture:
		SV_pCommandContext->CopyResource(SV_pRenderDevice->GetTexture(Dst), SV_pRenderDevice->GetTexture(Src));
		break;
	}
}

void RenderContext::TransitionBarrier(RenderResourceHandle ResourceHandle, Resource::State TransitionState, UINT Subresource /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
{
	switch (ResourceHandle.Type)
	{
	case RenderResourceType::Buffer:
		SV_pCommandContext->TransitionBarrier(SV_pRenderDevice->GetBuffer(ResourceHandle), TransitionState, Subresource);
		break;

	case RenderResourceType::Texture:
		SV_pCommandContext->TransitionBarrier(SV_pRenderDevice->GetTexture(ResourceHandle), TransitionState, Subresource);
		break;
	}
}

void RenderContext::AliasingBarrier(RenderResourceHandle BeforeResourceHandle, RenderResourceHandle AfterResourceHandle)
{
	// TODO: Implement
	__debugbreak();
}

void RenderContext::UAVBarrier(RenderResourceHandle ResourceHandle)
{
	switch (ResourceHandle.Type)
	{
	case RenderResourceType::Buffer:
		SV_pCommandContext->UAVBarrier(SV_pRenderDevice->GetBuffer(ResourceHandle));
		break;

	case RenderResourceType::Texture:
		SV_pCommandContext->UAVBarrier(SV_pRenderDevice->GetTexture(ResourceHandle));
		break;
	}
}

void RenderContext::SetPipelineState(RenderResourceHandle PipelineStateHandle)
{
	if (m_PipelineStateHandle == PipelineStateHandle)
		return;

	m_PipelineStateHandle = PipelineStateHandle;

	switch (m_PipelineStateHandle.Type)
	{
	case RenderResourceType::PipelineState:
		SetPipelineState(SV_pRenderDevice->GetPipelineState(m_PipelineStateHandle));
		break;

	case RenderResourceType::RaytracingPipelineState:
		SetRaytracingPipelineState(SV_pRenderDevice->GetRaytracingPipelineState(m_PipelineStateHandle));
		break;
	}
}

void RenderContext::SetRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
	switch (m_PipelineStateType)
	{
	case PipelineState::Type::Graphics:
		SV_pCommandContext->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
		break;

		// RaytracingPipelineState's use SetComputeXXX method
	case PipelineState::Type::Compute:
		SV_pCommandContext->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
		break;
	}
}

void RenderContext::SetRootShaderResourceView(UINT RootParameterIndex, RenderResourceHandle BufferHandle)
{
	assert(BufferHandle.Type == RenderResourceType::Buffer);

	auto pBuffer = SV_pRenderDevice->GetBuffer(BufferHandle);
	auto GpuVirtualAddress = pBuffer->GetGpuVirtualAddress();

	switch (m_PipelineStateType)
	{
	case PipelineState::Type::Graphics:
		SV_pCommandContext->SetGraphicsRootShaderResourceView(RootParameterIndex, GpuVirtualAddress);
		break;

		// RaytracingPipelineState's use SetComputeXXX method
	case PipelineState::Type::Compute:
		SV_pCommandContext->SetComputeRootShaderResourceView(RootParameterIndex, GpuVirtualAddress);
		break;
	}
}

void RenderContext::DispatchRays
(
	RenderResourceHandle RayGenerationShaderTable,
	RenderResourceHandle MissShaderTable,
	RenderResourceHandle HitGroupShaderTable,
	UINT Width, UINT Height, UINT Depth
)
{
	D3D12_DISPATCH_RAYS_DESC Desc = {};
	{
		auto pRayGenerationShaderTable					= SV_pRenderDevice->GetBuffer(RayGenerationShaderTable);
		Desc.RayGenerationShaderRecord.StartAddress		= pRayGenerationShaderTable->GetGpuVirtualAddress();
		Desc.RayGenerationShaderRecord.SizeInBytes		= pRayGenerationShaderTable->GetMemoryRequested();

		if (MissShaderTable)
		{
			auto pMissShaderTable						= SV_pRenderDevice->GetBuffer(MissShaderTable);
			Desc.MissShaderTable.StartAddress			= pMissShaderTable->GetGpuVirtualAddress();
			Desc.MissShaderTable.SizeInBytes			= pMissShaderTable->GetMemoryRequested();
			Desc.MissShaderTable.StrideInBytes			= pMissShaderTable->GetStride();
		}

		if (HitGroupShaderTable)
		{
			auto pHitGroupShaderTable					= SV_pRenderDevice->GetBuffer(HitGroupShaderTable);
			Desc.HitGroupTable.StartAddress				= pHitGroupShaderTable->GetGpuVirtualAddress();
			Desc.HitGroupTable.SizeInBytes				= pHitGroupShaderTable->GetMemoryRequested();
			Desc.HitGroupTable.StrideInBytes			= pHitGroupShaderTable->GetStride();
		}

		Desc.Width = Width;
		Desc.Height = Height;
		Desc.Depth = Depth;
	}
	SV_pCommandContext->DispatchRays(&Desc);
}

void RenderContext::SetPipelineState(PipelineState* pPipelineState)
{
	m_PipelineStateType = pPipelineState->GetType();

	SV_pCommandContext->SetPipelineState(pPipelineState);

	switch (m_PipelineStateType)
	{
	case PipelineState::Type::Graphics:
	{
		SV_pCommandContext->SetGraphicsRootSignature(pPipelineState->pRootSignature);
		BindGraphicsShaderLayoutResource(pPipelineState->pRootSignature);
	}
	break;

	case PipelineState::Type::Compute:
	{
		SV_pCommandContext->SetComputeRootSignature(pPipelineState->pRootSignature);
		BindComputeShaderLayoutResource(pPipelineState->pRootSignature);
	}
	break;
	}
}

void RenderContext::SetRaytracingPipelineState(RaytracingPipelineState* pRaytracingPipelineState)
{
	m_PipelineStateType = PipelineState::Type::Compute;

	SV_pCommandContext->SetRaytracingPipelineState(pRaytracingPipelineState);
	SV_pCommandContext->SetComputeRootSignature(pRaytracingPipelineState->pGlobalRootSignature);

	BindComputeShaderLayoutResource(pRaytracingPipelineState->pGlobalRootSignature);
}

void RenderContext::BindGraphicsShaderLayoutResource(const RootSignature* pRootSignature)
{
	const auto RootParameterOffset = pRootSignature->NumParameters - RootParameters::ShaderLayout::NumRootParameters;

	auto ShaderResourceDescriptorFromStart	= SV_pRenderDevice->GetGpuDescriptorHeapSRDescriptorFromStart();
	auto UnorderedAccessDescriptorFromStart	= SV_pRenderDevice->GetGpuDescriptorHeapUADescriptorFromStart();
	auto SamplerDescriptorFromStart			= SV_pRenderDevice->GetSamplerDescriptorHeapDescriptorFromStart();

	if (SV_pSystemConstants)
		SV_pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::ShaderLayout::SystemConstants + RootParameterOffset, SV_pSystemConstants->GetGpuVirtualAddress());
	if (SV_pGpuData)
		SV_pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::ShaderLayout::RenderPassDataCB + RootParameterOffset, SV_pGpuData->GetGpuVirtualAddressAt(SV_RenderPassIndex));
	SV_pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::ShaderLayout::ShaderResourceDescriptorTable + RootParameterOffset, ShaderResourceDescriptorFromStart.GpuHandle);
	SV_pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::ShaderLayout::UnorderedAccessDescriptorTable + RootParameterOffset, UnorderedAccessDescriptorFromStart.GpuHandle);
	SV_pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::ShaderLayout::SamplerDescriptorTable + RootParameterOffset, SamplerDescriptorFromStart.GpuHandle);
}

void RenderContext::BindComputeShaderLayoutResource(const RootSignature* pRootSignature)
{
	const auto RootParameterOffset = pRootSignature->NumParameters - RootParameters::ShaderLayout::NumRootParameters;

	auto ShaderResourceDescriptorFromStart	= SV_pRenderDevice->GetGpuDescriptorHeapSRDescriptorFromStart();
	auto UnorderedAccessDescriptorFromStart	= SV_pRenderDevice->GetGpuDescriptorHeapUADescriptorFromStart();
	auto SamplerDescriptorFromStart			= SV_pRenderDevice->GetSamplerDescriptorHeapDescriptorFromStart();

	if (SV_pSystemConstants)
		SV_pCommandContext->SetComputeRootConstantBufferView(RootParameters::ShaderLayout::SystemConstants + RootParameterOffset, SV_pSystemConstants->GetGpuVirtualAddress());
	if (SV_pGpuData)
		SV_pCommandContext->SetComputeRootConstantBufferView(RootParameters::ShaderLayout::RenderPassDataCB + RootParameterOffset, SV_pGpuData->GetGpuVirtualAddressAt(SV_RenderPassIndex));
	SV_pCommandContext->SetComputeRootDescriptorTable(RootParameters::ShaderLayout::ShaderResourceDescriptorTable + RootParameterOffset, ShaderResourceDescriptorFromStart.GpuHandle);
	SV_pCommandContext->SetComputeRootDescriptorTable(RootParameters::ShaderLayout::UnorderedAccessDescriptorTable + RootParameterOffset, UnorderedAccessDescriptorFromStart.GpuHandle);
	SV_pCommandContext->SetComputeRootDescriptorTable(RootParameters::ShaderLayout::SamplerDescriptorTable + RootParameterOffset, SamplerDescriptorFromStart.GpuHandle);
}