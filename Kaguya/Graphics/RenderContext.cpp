#include "pch.h"
#include "RenderContext.h"

void RenderContext::CopyBufferRegion(RenderResourceHandle DstBuffer, UINT64 DstOffset, RenderResourceHandle SrcBuffer, UINT64 SrcOffset, UINT64 NumBytes)
{
	auto pDstBuffer = SV_pRenderDevice->GetBuffer(DstBuffer)->GetApiHandle();
	auto pSrcBuffer = SV_pRenderDevice->GetBuffer(SrcBuffer)->GetApiHandle();

	SV_CommandList->CopyBufferRegion(pDstBuffer, DstOffset, pSrcBuffer, SrcOffset, NumBytes);
}

void RenderContext::CopyResource(RenderResourceHandle Dst, RenderResourceHandle Src)
{
	switch (Dst.Type)
	{
	case RenderResourceType::Buffer:
	{
		auto pDstBuffer = SV_pRenderDevice->GetBuffer(Dst)->GetApiHandle();
		auto pSrcBuffer = SV_pRenderDevice->GetBuffer(Src)->GetApiHandle();
		SV_CommandList->CopyResource(pDstBuffer, pSrcBuffer);
	}
	break;

	case RenderResourceType::Texture:
	{
		auto pDstTexture = SV_pRenderDevice->GetTexture(Dst)->GetApiHandle();
		auto pSrcTexture = SV_pRenderDevice->GetTexture(Src)->GetApiHandle();
		SV_CommandList->CopyResource(pDstTexture, pSrcTexture);
	}
	break;
	}
}

void RenderContext::TransitionBarrier(RenderResourceHandle ResourceHandle, Resource::State TransitionState, UINT Subresource /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
{
	switch (ResourceHandle.Type)
	{
	case RenderResourceType::Buffer:
		SV_CommandList.TransitionBarrier(SV_pRenderDevice->GetBuffer(ResourceHandle)->GetApiHandle(), TransitionState, Subresource);
		break;

	case RenderResourceType::Texture:
		SV_CommandList.TransitionBarrier(SV_pRenderDevice->GetTexture(ResourceHandle)->GetApiHandle(), TransitionState, Subresource);
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
		SV_CommandList.UAVBarrier(SV_pRenderDevice->GetBuffer(ResourceHandle)->GetApiHandle());
		break;

	case RenderResourceType::Texture:
		SV_CommandList.UAVBarrier(SV_pRenderDevice->GetTexture(ResourceHandle)->GetApiHandle());
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
		SV_CommandList->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
		break;

		// RaytracingPipelineState's use SetComputeXXX method
	case PipelineState::Type::Compute:
		SV_CommandList->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
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
		SV_CommandList->SetGraphicsRootShaderResourceView(RootParameterIndex, GpuVirtualAddress);
		break;

		// RaytracingPipelineState's use SetComputeXXX method
	case PipelineState::Type::Compute:
		SV_CommandList->SetComputeRootShaderResourceView(RootParameterIndex, GpuVirtualAddress);
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

	Desc.Width	= Width;
	Desc.Height = Height;
	Desc.Depth	= Depth;
	SV_CommandList->DispatchRays(&Desc);
}

void RenderContext::SetPipelineState(PipelineState* pPipelineState)
{
	m_PipelineStateType = pPipelineState->GetType();

	SV_CommandList->SetPipelineState(pPipelineState->GetApiHandle());

	switch (m_PipelineStateType)
	{
	case PipelineState::Type::Graphics:
	{
		SV_CommandList->SetGraphicsRootSignature(pPipelineState->pRootSignature->GetApiHandle());
		BindGraphicsShaderLayoutResource(pPipelineState->pRootSignature);
	}
	break;

	case PipelineState::Type::Compute:
	{
		SV_CommandList->SetComputeRootSignature(pPipelineState->pRootSignature->GetApiHandle());
		BindComputeShaderLayoutResource(pPipelineState->pRootSignature);
	}
	break;
	}
}

void RenderContext::SetRaytracingPipelineState(RaytracingPipelineState* pRaytracingPipelineState)
{
	m_PipelineStateType = PipelineState::Type::Compute;

	SV_CommandList->SetPipelineState1(pRaytracingPipelineState->GetApiHandle());
	SV_CommandList->SetComputeRootSignature(pRaytracingPipelineState->pGlobalRootSignature->GetApiHandle());

	BindComputeShaderLayoutResource(pRaytracingPipelineState->pGlobalRootSignature);
}

void RenderContext::BindGraphicsShaderLayoutResource(const RootSignature* pRootSignature)
{
	const auto RootParameterOffset = pRootSignature->NumParameters - RootParameters::ShaderLayout::NumRootParameters;

	auto ShaderResourceDescriptorFromStart	= SV_pRenderDevice->GetGpuDescriptorHeapSRDescriptorFromStart();
	auto UnorderedAccessDescriptorFromStart	= SV_pRenderDevice->GetGpuDescriptorHeapUADescriptorFromStart();
	auto SamplerDescriptorFromStart			= SV_pRenderDevice->GetSamplerDescriptorHeapDescriptorFromStart();

	if (SV_pSystemConstants)
		SV_CommandList->SetGraphicsRootConstantBufferView(RootParameters::ShaderLayout::SystemConstants + RootParameterOffset, SV_pSystemConstants->GetGpuVirtualAddress());
	if (SV_pGpuData)
		SV_CommandList->SetGraphicsRootConstantBufferView(RootParameters::ShaderLayout::RenderPassDataCB + RootParameterOffset, SV_pGpuData->GetGpuVirtualAddressAt(SV_RenderPassIndex));
	SV_CommandList->SetGraphicsRootDescriptorTable(RootParameters::ShaderLayout::ShaderResourceDescriptorTable + RootParameterOffset, ShaderResourceDescriptorFromStart.GpuHandle);
	SV_CommandList->SetGraphicsRootDescriptorTable(RootParameters::ShaderLayout::UnorderedAccessDescriptorTable + RootParameterOffset, UnorderedAccessDescriptorFromStart.GpuHandle);
	SV_CommandList->SetGraphicsRootDescriptorTable(RootParameters::ShaderLayout::SamplerDescriptorTable + RootParameterOffset, SamplerDescriptorFromStart.GpuHandle);
}

void RenderContext::BindComputeShaderLayoutResource(const RootSignature* pRootSignature)
{
	const auto RootParameterOffset = pRootSignature->NumParameters - RootParameters::ShaderLayout::NumRootParameters;

	auto ShaderResourceDescriptorFromStart	= SV_pRenderDevice->GetGpuDescriptorHeapSRDescriptorFromStart();
	auto UnorderedAccessDescriptorFromStart	= SV_pRenderDevice->GetGpuDescriptorHeapUADescriptorFromStart();
	auto SamplerDescriptorFromStart			= SV_pRenderDevice->GetSamplerDescriptorHeapDescriptorFromStart();

	if (SV_pSystemConstants)
		SV_CommandList->SetComputeRootConstantBufferView(RootParameters::ShaderLayout::SystemConstants + RootParameterOffset, SV_pSystemConstants->GetGpuVirtualAddress());
	if (SV_pGpuData)
		SV_CommandList->SetComputeRootConstantBufferView(RootParameters::ShaderLayout::RenderPassDataCB + RootParameterOffset, SV_pGpuData->GetGpuVirtualAddressAt(SV_RenderPassIndex));
	SV_CommandList->SetComputeRootDescriptorTable(RootParameters::ShaderLayout::ShaderResourceDescriptorTable + RootParameterOffset, ShaderResourceDescriptorFromStart.GpuHandle);
	SV_CommandList->SetComputeRootDescriptorTable(RootParameters::ShaderLayout::UnorderedAccessDescriptorTable + RootParameterOffset, UnorderedAccessDescriptorFromStart.GpuHandle);
	SV_CommandList->SetComputeRootDescriptorTable(RootParameters::ShaderLayout::SamplerDescriptorTable + RootParameterOffset, SamplerDescriptorFromStart.GpuHandle);
}