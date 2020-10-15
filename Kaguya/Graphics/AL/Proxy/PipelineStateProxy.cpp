#include "pch.h"
#include "PipelineStateProxy.h"
#include "../D3D12/RootSignature.h"
#include "../D3D12/Shader.h"

PipelineStateProxy::PipelineStateProxy(PipelineState::Type Type)
	: m_Type(Type)
{
	pRootSignature = nullptr;
}

void PipelineStateProxy::Link()
{
	assert(pRootSignature != nullptr);
}

//
GraphicsPipelineStateProxy::GraphicsPipelineStateProxy()
	: PipelineStateProxy(PipelineState::Type::Graphics)
{
	pVS = nullptr;
	pPS = nullptr;
	pDS = nullptr;
	pHS = nullptr;
	pGS = nullptr;
	PrimitiveTopology = PrimitiveTopology::Undefined;

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
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = pRootSignature->GetD3DRootSignature();
	if (pVS) desc.VS = pVS->GetD3DShaderBytecode();
	if (pPS) desc.PS = pPS->GetD3DShaderBytecode();
	if (pDS) desc.DS = pDS->GetD3DShaderBytecode();
	if (pHS) desc.HS = pHS->GetD3DShaderBytecode();
	if (pGS) desc.GS = pGS->GetD3DShaderBytecode();
	desc.StreamOutput = {};
	desc.BlendState = BlendState;
	desc.SampleMask = UINT_MAX;
	desc.RasterizerState = RasterizerState;
	desc.DepthStencilState = DepthStencilState;
	desc.InputLayout = InputLayout;
	desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	desc.PrimitiveTopologyType = GetD3DPrimitiveTopologyType(PrimitiveTopology);
	desc.NumRenderTargets = m_NumRenderTargets;
	memcpy(desc.RTVFormats, m_RTVFormats, sizeof(desc.RTVFormats));
	desc.DSVFormat = m_DSVFormat;
	desc.SampleDesc = m_SampleDesc;
	desc.NodeMask = 0;
	desc.CachedPSO = {};
	desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	return desc;
}

//
ComputePipelineStateProxy::ComputePipelineStateProxy()
	: PipelineStateProxy(PipelineState::Type::Compute)
{
	pCS = nullptr;
}

void ComputePipelineStateProxy::Link()
{
	assert(pCS != nullptr && pCS->GetType() == Shader::Type::Compute);

	PipelineStateProxy::Link();
}

D3D12_COMPUTE_PIPELINE_STATE_DESC ComputePipelineStateProxy::BuildD3DDesc()
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};
	desc.pRootSignature = pRootSignature->GetD3DRootSignature();
	desc.CS = pCS->GetD3DShaderBytecode();
	desc.NodeMask = 0;
	desc.CachedPSO = {};
	desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;

	return desc;
}