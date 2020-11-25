#include "pch.h"
#include "PipelineStateProxy.h"
#include "../D3D12/RootSignature.h"
#include "../D3D12/Shader.h"

PipelineStateProxy::PipelineStateProxy(PipelineState::Type Type)
	: m_Type(Type)
{

}

void PipelineStateProxy::Link()
{
	assert(pRootSignature != nullptr);
}

GraphicsPipelineStateProxy::GraphicsPipelineStateProxy()
	: PipelineStateProxy(PipelineState::Type::Graphics)
{
	m_NumRenderTargets = 0;
	memset(m_RTVFormats, 0, sizeof(m_RTVFormats));
	m_DSVFormat = DXGI_FORMAT_UNKNOWN;
	m_SampleDesc = { .Count = 1, .Quality = 0 };
}

void GraphicsPipelineStateProxy::AddRenderTargetFormat(DXGI_FORMAT Format)
{
	assert(m_NumRenderTargets < 8 && "Max Render Target Reached");
	m_RTVFormats[m_NumRenderTargets++] = Format;
}

void GraphicsPipelineStateProxy::SetDepthStencilFormat(DXGI_FORMAT Format)
{
	m_DSVFormat = Format;
}

void GraphicsPipelineStateProxy::SetSampleDesc(UINT Count, UINT Quality)
{
	m_SampleDesc.Count = Count;
	m_SampleDesc.Quality = Quality;
}

void GraphicsPipelineStateProxy::Link()
{
	if (pVS) assert(pVS->GetType() == Shader::Type::Vertex);
	if (pPS) assert(pPS->GetType() == Shader::Type::Pixel);
	if (pDS) assert(pDS->GetType() == Shader::Type::Domain);
	if (pHS) assert(pHS->GetType() == Shader::Type::Hull);
	if (pGS) assert(pGS->GetType() == Shader::Type::Geometry);
	assert(PrimitiveTopology != PrimitiveTopology::Undefined);
	assert(m_NumRenderTargets <= 8);

	PipelineStateProxy::Link();
}

D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsPipelineStateProxy::BuildD3DDesc()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc = {};
	Desc.pRootSignature						= pRootSignature->GetApiHandle();
	if (pVS) Desc.VS						= pVS->GetD3DShaderBytecode();
	if (pPS) Desc.PS						= pPS->GetD3DShaderBytecode();
	if (pDS) Desc.DS						= pDS->GetD3DShaderBytecode();
	if (pHS) Desc.HS						= pHS->GetD3DShaderBytecode();
	if (pGS) Desc.GS						= pGS->GetD3DShaderBytecode();
	Desc.StreamOutput						= {};
	Desc.BlendState							= BlendState;
	Desc.SampleMask							= UINT_MAX;
	Desc.RasterizerState					= RasterizerState;
	Desc.DepthStencilState					= DepthStencilState;
	Desc.InputLayout						= InputLayout;
	Desc.IBStripCutValue					= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	Desc.PrimitiveTopologyType				= GetD3DPrimitiveTopologyType(PrimitiveTopology);
	Desc.NumRenderTargets					= m_NumRenderTargets;
	memcpy(Desc.RTVFormats, m_RTVFormats, sizeof(Desc.RTVFormats));
	Desc.DSVFormat							= m_DSVFormat;
	Desc.SampleDesc							= m_SampleDesc;
	Desc.NodeMask							= 0;
	Desc.CachedPSO							= {};
	Desc.Flags								= D3D12_PIPELINE_STATE_FLAG_NONE;

	return Desc;
}

D3DX12_MESH_SHADER_PIPELINE_STATE_DESC GraphicsPipelineStateProxy::BuildMSPSODesc()
{
	D3DX12_MESH_SHADER_PIPELINE_STATE_DESC Desc = {};
	Desc.pRootSignature							= pRootSignature->GetApiHandle();
	Desc.MS										= pMS->GetD3DShaderBytecode();
	Desc.PS										= pPS->GetD3DShaderBytecode();
	Desc.BlendState								= BlendState;
	Desc.SampleMask								= UINT_MAX;
	Desc.RasterizerState						= RasterizerState;
	Desc.DepthStencilState						= DepthStencilState;
	Desc.PrimitiveTopologyType					= GetD3DPrimitiveTopologyType(PrimitiveTopology);
	Desc.NumRenderTargets						= m_NumRenderTargets;
	memcpy(Desc.RTVFormats, m_RTVFormats, sizeof(Desc.RTVFormats));
	Desc.DSVFormat								= m_DSVFormat;
	Desc.SampleDesc								= m_SampleDesc;
	Desc.NodeMask								= 0;
	Desc.CachedPSO								= {};
	Desc.Flags									= D3D12_PIPELINE_STATE_FLAG_NONE;

	return Desc;
}

ComputePipelineStateProxy::ComputePipelineStateProxy()
	: PipelineStateProxy(PipelineState::Type::Compute)
{

}

void ComputePipelineStateProxy::Link()
{
	assert(pCS != nullptr && pCS->GetType() == Shader::Type::Compute);

	PipelineStateProxy::Link();
}

D3D12_COMPUTE_PIPELINE_STATE_DESC ComputePipelineStateProxy::BuildD3DDesc()
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC Desc	= {};
	Desc.pRootSignature						= pRootSignature->GetApiHandle();
	Desc.CS									= pCS->GetD3DShaderBytecode();
	Desc.NodeMask							= 0;
	Desc.CachedPSO							= {};
	Desc.Flags								= D3D12_PIPELINE_STATE_FLAG_NONE;

	return Desc;
}