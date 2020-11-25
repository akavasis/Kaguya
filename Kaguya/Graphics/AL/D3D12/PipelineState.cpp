#include "pch.h"
#include "PipelineState.h"
#include "Device.h"
#include "../Proxy/PipelineStateProxy.h"

PipelineState::PipelineState(PipelineStateProxy& Proxy)
	: m_Type(Proxy.m_Type)
{
	Proxy.Link();

	pRootSignature = Proxy.pRootSignature;
}

GraphicsPipelineState::GraphicsPipelineState(const Device* pDevice, GraphicsPipelineStateProxy& Proxy)
	: PipelineState(Proxy)
{
	if (!Proxy.pMS)
	{
		D3D12_GRAPHICS_PIPELINE_STATE_DESC Desc = Proxy.BuildD3DDesc();
		ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateGraphicsPipelineState(&Desc, IID_PPV_ARGS(m_PipelineState.ReleaseAndGetAddressOf())));
	}
	else
	{
		auto PSOStream = CD3DX12_PIPELINE_MESH_STATE_STREAM(Proxy.BuildMSPSODesc());

		D3D12_PIPELINE_STATE_STREAM_DESC Desc;
		Desc.pPipelineStateSubobjectStream = &PSOStream;
		Desc.SizeInBytes = sizeof(PSOStream);

		ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreatePipelineState(&Desc, IID_PPV_ARGS(m_PipelineState.ReleaseAndGetAddressOf())));
	}
}

ComputePipelineState::ComputePipelineState(const Device* pDevice, ComputePipelineStateProxy& Proxy)
	: PipelineState(Proxy)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC Desc = Proxy.BuildD3DDesc();
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateComputePipelineState(&Desc, IID_PPV_ARGS(m_PipelineState.ReleaseAndGetAddressOf())));
}