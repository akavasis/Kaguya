#include "pch.h"
#include "RenderContext.h"
#include "RendererRegistry.h"

void RenderContext::TransitionBarrier(RenderResourceHandle ResourceHandle, Resource::State TransitionState, UINT Subresource /*= D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES*/)
{
	switch (ResourceHandle.Type)
	{
	case RenderResourceType::Buffer:
		m_pCommandContext->TransitionBarrier(m_pRenderDevice->GetBuffer(ResourceHandle), TransitionState, Subresource);
		break;

	case RenderResourceType::Texture:
		m_pCommandContext->TransitionBarrier(m_pRenderDevice->GetTexture(ResourceHandle), TransitionState, Subresource);
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
		m_pCommandContext->UAVBarrier(m_pRenderDevice->GetBuffer(ResourceHandle));
		break;

	case RenderResourceType::Texture:
		m_pCommandContext->UAVBarrier(m_pRenderDevice->GetTexture(ResourceHandle));
		break;
	}
}

void RenderContext::SetPipelineState(RenderResourceHandle PipelineStateHandle)
{
	m_PipelineStateHandle = PipelineStateHandle;

	switch (m_PipelineStateHandle.Type)
	{
	case RenderResourceType::GraphicsPSO:
		SetGraphicsPSO(m_pRenderDevice->GetGraphicsPSO(m_PipelineStateHandle));
		break;

	case RenderResourceType::ComputePSO:
		SetComputePSO(m_pRenderDevice->GetComputePSO(m_PipelineStateHandle));
		break;

	case RenderResourceType::RaytracingPSO:
		SetRaytracingPSO(m_pRenderDevice->GetRaytracingPSO(m_PipelineStateHandle));
		break;
	}
}

void RenderContext::SetRootShaderResourceView(UINT RootParameterIndex, RenderResourceHandle BufferHandle)
{
	assert(BufferHandle.Type == RenderResourceType::Buffer);

	auto pBuffer = m_pRenderDevice->GetBuffer(BufferHandle);

	switch (m_PipelineStateHandle.Type)
	{
	case RenderResourceType::GraphicsPSO:
		m_pCommandContext->SetGraphicsRootShaderResourceView(RootParameterIndex, pBuffer->GetGpuVirtualAddress());
		break;

		// RaytracingPSO's use SetComputeXXX method
	case RenderResourceType::ComputePSO:
	case RenderResourceType::RaytracingPSO:
		m_pCommandContext->SetComputeRootShaderResourceView(RootParameterIndex, pBuffer->GetGpuVirtualAddress());
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
	auto pRayGenerationShaderTable = m_pRenderDevice->GetBuffer(RayGenerationShaderTable);
	auto pMissShaderTable = m_pRenderDevice->GetBuffer(MissShaderTable);
	auto pHitGroupShaderTable = m_pRenderDevice->GetBuffer(HitGroupShaderTable);

	D3D12_DISPATCH_RAYS_DESC desc = {};

	desc.RayGenerationShaderRecord.StartAddress = pRayGenerationShaderTable->GetGpuVirtualAddress();
	desc.RayGenerationShaderRecord.SizeInBytes = pRayGenerationShaderTable->GetMemoryRequested();

	desc.MissShaderTable.StartAddress = pMissShaderTable->GetGpuVirtualAddress();
	desc.MissShaderTable.SizeInBytes = pMissShaderTable->GetMemoryRequested();
	desc.MissShaderTable.StrideInBytes = pMissShaderTable->GetStride();

	desc.HitGroupTable.StartAddress = pHitGroupShaderTable->GetGpuVirtualAddress();
	desc.HitGroupTable.SizeInBytes = pHitGroupShaderTable->GetMemoryRequested();
	desc.HitGroupTable.StrideInBytes = pHitGroupShaderTable->GetStride();

	desc.Width = Width;
	desc.Height = Height;
	desc.Depth = Depth;

	m_pCommandContext->DispatchRays(&desc);
}

void RenderContext::SetGraphicsPSO(GraphicsPipelineState* pGraphicsPipelineState)
{
	m_pCommandContext->SetPipelineState(pGraphicsPipelineState);
	m_pCommandContext->SetGraphicsRootSignature(pGraphicsPipelineState->pRootSignature);

	BindGraphicsShaderLayoutResource(pGraphicsPipelineState->pRootSignature);
}

void RenderContext::SetComputePSO(ComputePipelineState* pComputePipelineState)
{
	m_pCommandContext->SetPipelineState(pComputePipelineState);
	m_pCommandContext->SetComputeRootSignature(pComputePipelineState->pRootSignature);

	BindComputeShaderLayoutResource(pComputePipelineState->pRootSignature);
}

void RenderContext::SetRaytracingPSO(RaytracingPipelineState* pRaytracingPipelineState)
{
	m_pCommandContext->SetRaytracingPipelineState(pRaytracingPipelineState);
	m_pCommandContext->SetComputeRootSignature(pRaytracingPipelineState->pGlobalRootSignature);

	BindComputeShaderLayoutResource(pRaytracingPipelineState->pGlobalRootSignature);
}

void RenderContext::BindGraphicsShaderLayoutResource(const RootSignature* pRootSignature)
{
	const size_t RootParameterOffset = pRootSignature->NumParameters - RootParameters::ShaderLayout::NumRootParameters;

	Descriptor ShaderResourceDescriptorFromStart = m_pRenderDevice->GetGpuDescriptorHeapSRDescriptorFromStart();
	Descriptor UnorderedAccessDescriptorFromStart = m_pRenderDevice->GetGpuDescriptorHeapUADescriptorFromStart();
	Descriptor SamplerDescriptorFromStart = m_pRenderDevice->GetSamplerDescriptorHeapDescriptorFromStart();

	if (m_pGpuData)
		m_pCommandContext->SetGraphicsRootConstantBufferView(RootParameters::ShaderLayout::RenderPassDataCB + RootParameterOffset, m_pGpuData->GetGpuVirtualAddressAt(m_RenderPassIndex));
	m_pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::ShaderLayout::ShaderResourceDescriptorTable + RootParameterOffset, ShaderResourceDescriptorFromStart.GPUHandle);
	m_pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::ShaderLayout::UnorderedAccessDescriptorTable + RootParameterOffset, UnorderedAccessDescriptorFromStart.GPUHandle);
	m_pCommandContext->SetGraphicsRootDescriptorTable(RootParameters::ShaderLayout::SamplerDescriptorTable + RootParameterOffset, SamplerDescriptorFromStart.GPUHandle);
}

void RenderContext::BindComputeShaderLayoutResource(const RootSignature* pRootSignature)
{
	const size_t RootParameterOffset = pRootSignature->NumParameters - RootParameters::ShaderLayout::NumRootParameters;

	Descriptor ShaderResourceDescriptorFromStart = m_pRenderDevice->GetGpuDescriptorHeapSRDescriptorFromStart();
	Descriptor UnorderedAccessDescriptorFromStart = m_pRenderDevice->GetGpuDescriptorHeapUADescriptorFromStart();
	Descriptor SamplerDescriptorFromStart = m_pRenderDevice->GetSamplerDescriptorHeapDescriptorFromStart();

	if (m_pGpuData)
		m_pCommandContext->SetComputeRootConstantBufferView(RootParameters::ShaderLayout::RenderPassDataCB + RootParameterOffset, m_pGpuData->GetGpuVirtualAddressAt(m_RenderPassIndex));
	m_pCommandContext->SetComputeRootDescriptorTable(RootParameters::ShaderLayout::ShaderResourceDescriptorTable + RootParameterOffset, ShaderResourceDescriptorFromStart.GPUHandle);
	m_pCommandContext->SetComputeRootDescriptorTable(RootParameters::ShaderLayout::UnorderedAccessDescriptorTable + RootParameterOffset, UnorderedAccessDescriptorFromStart.GPUHandle);
	m_pCommandContext->SetComputeRootDescriptorTable(RootParameters::ShaderLayout::SamplerDescriptorTable + RootParameterOffset, SamplerDescriptorFromStart.GPUHandle);
}