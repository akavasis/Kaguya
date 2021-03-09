#include "pch.h"
#include "PipelineStateBuilder.h"

#include "Shader.h"

//----------------------------------------------------------------------------------------------------
D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsPipelineStateBuilder::Build()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc		= {};
	Desc.pRootSignature							= *pRootSignature;
	if (pVS) Desc.VS							= pVS->GetD3DShaderBytecode();
	if (pPS) Desc.PS							= pPS->GetD3DShaderBytecode();
	if (pDS) Desc.DS							= pDS->GetD3DShaderBytecode();
	if (pHS) Desc.HS							= pHS->GetD3DShaderBytecode();
	if (pGS) Desc.GS							= pGS->GetD3DShaderBytecode();
	Desc.StreamOutput							= {};
	Desc.BlendState								= BlendState;
	Desc.SampleMask								= DefaultSampleMask();
	Desc.RasterizerState						= RasterizerState;
	Desc.DepthStencilState						= DepthStencilState;
	Desc.InputLayout							= InputLayout;
	Desc.IBStripCutValue						= D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	Desc.PrimitiveTopologyType					= GetD3DPrimitiveTopologyType(PrimitiveTopology);
	Desc.NumRenderTargets						= NumRenderTargets;
	memcpy(Desc.RTVFormats, RTVFormats, sizeof(Desc.RTVFormats));
	Desc.DSVFormat								= DSVFormat;
	Desc.SampleDesc								= SampleDesc;
	Desc.NodeMask								= 0;
	Desc.CachedPSO								= {};
	Desc.Flags									= D3D12_PIPELINE_STATE_FLAG_NONE;
	return Desc;
}

D3DX12_MESH_SHADER_PIPELINE_STATE_DESC GraphicsPipelineStateBuilder::BuildMSPSODesc()
{
	D3DX12_MESH_SHADER_PIPELINE_STATE_DESC Desc = {};
	Desc.pRootSignature							= *pRootSignature;
	Desc.MS										= pMS->GetD3DShaderBytecode();
	Desc.PS										= pPS->GetD3DShaderBytecode();
	Desc.BlendState								= BlendState;
	Desc.SampleMask								= DefaultSampleMask();
	Desc.RasterizerState						= RasterizerState;
	Desc.DepthStencilState						= DepthStencilState;
	Desc.PrimitiveTopologyType					= GetD3DPrimitiveTopologyType(PrimitiveTopology);
	Desc.NumRenderTargets						= NumRenderTargets;
	memcpy(Desc.RTVFormats, RTVFormats, sizeof(Desc.RTVFormats));
	Desc.DSVFormat								= DSVFormat;
	Desc.SampleDesc								= SampleDesc;
	Desc.NodeMask								= 0;
	Desc.CachedPSO								= {};
	Desc.Flags									= D3D12_PIPELINE_STATE_FLAG_NONE;
	return Desc;
}

void GraphicsPipelineStateBuilder::AddRenderTargetFormat(DXGI_FORMAT Format)
{
	RTVFormats[NumRenderTargets++] = Format;
}

void GraphicsPipelineStateBuilder::SetDepthStencilFormat(DXGI_FORMAT Format)
{
	DSVFormat = Format;
}

void GraphicsPipelineStateBuilder::SetSampleDesc(UINT Count, UINT Quality)
{
	SampleDesc = 
	{
		.Count = Count,
		.Quality = Quality
	};
}

//----------------------------------------------------------------------------------------------------
D3D12_COMPUTE_PIPELINE_STATE_DESC ComputePipelineStateBuilder::Build()
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC Desc	= {};
	Desc.pRootSignature						= *pRootSignature;
	Desc.CS									= pCS->GetD3DShaderBytecode();
	Desc.NodeMask							= 0;
	Desc.CachedPSO							= {};
	Desc.Flags								= D3D12_PIPELINE_STATE_FLAG_NONE;
	return Desc;
}