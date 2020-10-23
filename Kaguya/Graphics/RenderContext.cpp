#include "pch.h"
#include "RenderContext.h"
#include "RendererRegistry.h"

void RenderContext::TransitionBarrier(RenderResourceHandle ResourceHandle, DeviceResource::State TransitionState, UINT Subresource /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
{
	switch (ResourceHandle.Type)
	{
	case RenderResourceType::DeviceBuffer:
		SV_pCommandContext->TransitionBarrier(SV_pRenderDevice->GetBuffer(ResourceHandle), TransitionState, Subresource);
		break;

	case RenderResourceType::DeviceTexture:
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
	case RenderResourceType::DeviceBuffer:
		SV_pCommandContext->UAVBarrier(SV_pRenderDevice->GetBuffer(ResourceHandle));
		break;

	case RenderResourceType::DeviceTexture:
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
	case RenderResourceType::GraphicsPSO:
		SetGraphicsPSO(SV_pRenderDevice->GetGraphicsPSO(m_PipelineStateHandle));
		break;

	case RenderResourceType::ComputePSO:
		SetComputePSO(SV_pRenderDevice->GetComputePSO(m_PipelineStateHandle));
		break;

	case RenderResourceType::RaytracingPSO:
		SetRaytracingPSO(SV_pRenderDevice->GetRaytracingPSO(m_PipelineStateHandle));
		break;
	}
}

void RenderContext::SetRoot32BitConstants(UINT RootParameterIndex, UINT Num32BitValuesToSet, const void* pSrcData, UINT DestOffsetIn32BitValues)
{
	switch (m_PipelineStateHandle.Type)
	{
	case RenderResourceType::GraphicsPSO:
		SV_pCommandContext->SetGraphicsRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
		break;

		// RaytracingPSO's use SetComputeXXX method
	case RenderResourceType::ComputePSO:
	case RenderResourceType::RaytracingPSO:
		SV_pCommandContext->SetComputeRoot32BitConstants(RootParameterIndex, Num32BitValuesToSet, pSrcData, DestOffsetIn32BitValues);
		break;
	}
}

void RenderContext::SetRootShaderResourceView(UINT RootParameterIndex, RenderResourceHandle BufferHandle)
{
	assert(BufferHandle.Type == RenderResourceType::DeviceBuffer);

	auto pBuffer = SV_pRenderDevice->GetBuffer(BufferHandle);

	switch (m_PipelineStateHandle.Type)
	{
	case RenderResourceType::GraphicsPSO:
		SV_pCommandContext->SetGraphicsRootShaderResourceView(RootParameterIndex, pBuffer->GetGpuVirtualAddress());
		break;

		// RaytracingPSO's use SetComputeXXX method
	case RenderResourceType::ComputePSO:
	case RenderResourceType::RaytracingPSO:
		SV_pCommandContext->SetComputeRootShaderResourceView(RootParameterIndex, pBuffer->GetGpuVirtualAddress());
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
	assert(RayGenerationShaderTable);

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

void RenderContext::SetGraphicsPSO(GraphicsPipelineState* pGraphicsPipelineState)
{
	SV_pCommandContext->SetPipelineState(pGraphicsPipelineState);
	SV_pCommandContext->SetGraphicsRootSignature(pGraphicsPipelineState->pRootSignature);

	BindGraphicsShaderLayoutResource(pGraphicsPipelineState->pRootSignature);
}

void RenderContext::SetComputePSO(ComputePipelineState* pComputePipelineState)
{
	SV_pCommandContext->SetPipelineState(pComputePipelineState);
	SV_pCommandContext->SetComputeRootSignature(pComputePipelineState->pRootSignature);

	BindComputeShaderLayoutResource(pComputePipelineState->pRootSignature);
}

void RenderContext::SetRaytracingPSO(RaytracingPipelineState* pRaytracingPipelineState)
{
	SV_pCommandContext->SetRaytracingPipelineState(pRaytracingPipelineState);
	SV_pCommandContext->SetComputeRootSignature(pRaytracingPipelineState->pGlobalRootSignature);

	BindComputeShaderLayoutResource(pRaytracingPipelineState->pGlobalRootSignature);
}

void RenderContext::BindGraphicsShaderLayoutResource(const RootSignature* pRootSignature)
{
	const size_t RootParameterOffset = pRootSignature->NumParameters - RootParameters::ShaderLayout::NumRootParameters;

	Descriptor ShaderResourceDescriptorFromStart	= SV_pRenderDevice->GetGpuDescriptorHeapSRDescriptorFromStart();
	Descriptor UnorderedAccessDescriptorFromStart	= SV_pRenderDevice->GetGpuDescriptorHeapUADescriptorFromStart();
	Descriptor SamplerDescriptorFromStart			= SV_pRenderDevice->GetSamplerDescriptorHeapDescriptorFromStart();

	if (SV_pSystemConstants)
		SV_pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::ShaderLayout::SystemConstants + RootParameterOffset, SV_pSystemConstants->GetGpuVirtualAddress());
	if (SV_pGpuData)
		SV_pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::ShaderLayout::RenderPassDataCB + RootParameterOffset, SV_pGpuData->GetGpuVirtualAddressAt(SV_RenderPassIndex));
	SV_pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::ShaderLayout::ShaderResourceDescriptorTable + RootParameterOffset, ShaderResourceDescriptorFromStart.GPUHandle);
	SV_pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::ShaderLayout::UnorderedAccessDescriptorTable + RootParameterOffset, UnorderedAccessDescriptorFromStart.GPUHandle);
	SV_pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::ShaderLayout::SamplerDescriptorTable + RootParameterOffset, SamplerDescriptorFromStart.GPUHandle);
}

void RenderContext::BindComputeShaderLayoutResource(const RootSignature* pRootSignature)
{
	const size_t RootParameterOffset = pRootSignature->NumParameters - RootParameters::ShaderLayout::NumRootParameters;

	Descriptor ShaderResourceDescriptorFromStart	= SV_pRenderDevice->GetGpuDescriptorHeapSRDescriptorFromStart();
	Descriptor UnorderedAccessDescriptorFromStart	= SV_pRenderDevice->GetGpuDescriptorHeapUADescriptorFromStart();
	Descriptor SamplerDescriptorFromStart			= SV_pRenderDevice->GetSamplerDescriptorHeapDescriptorFromStart();

	if (SV_pSystemConstants)
		SV_pCommandContext->SetComputeRootConstantBufferView(RootParameters::ShaderLayout::SystemConstants + RootParameterOffset, SV_pSystemConstants->GetGpuVirtualAddress());
	if (SV_pGpuData)
		SV_pCommandContext->SetComputeRootConstantBufferView(RootParameters::ShaderLayout::RenderPassDataCB + RootParameterOffset, SV_pGpuData->GetGpuVirtualAddressAt(SV_RenderPassIndex));
	SV_pCommandContext->SetComputeRootDescriptorTable(RootParameters::ShaderLayout::ShaderResourceDescriptorTable + RootParameterOffset, ShaderResourceDescriptorFromStart.GPUHandle);
	SV_pCommandContext->SetComputeRootDescriptorTable(RootParameters::ShaderLayout::UnorderedAccessDescriptorTable + RootParameterOffset, UnorderedAccessDescriptorFromStart.GPUHandle);
	SV_pCommandContext->SetComputeRootDescriptorTable(RootParameters::ShaderLayout::SamplerDescriptorTable + RootParameterOffset, SamplerDescriptorFromStart.GPUHandle);
}