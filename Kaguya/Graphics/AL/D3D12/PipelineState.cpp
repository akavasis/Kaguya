#include "pch.h"
#include "PipelineState.h"
#include "Device.h"
#include "../Proxy/PipelineStateProxy.h"

PipelineState::PipelineState(PipelineStateProxy& Proxy)
	: m_Type(Proxy.m_Type)
{
	Proxy.Link();
}

GraphicsPipelineState::GraphicsPipelineState(const Device* pDevice, GraphicsPipelineStateProxy& Proxy)
	: PipelineState(Proxy)
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = Proxy.BuildD3DDesc();
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(m_PipelineState.ReleaseAndGetAddressOf())));
}

ComputePipelineState::ComputePipelineState(const Device* pDevice, ComputePipelineStateProxy& Proxy)
	: PipelineState(Proxy)
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC desc = Proxy.BuildD3DDesc();
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateComputePipelineState(&desc, IID_PPV_ARGS(m_PipelineState.ReleaseAndGetAddressOf())));
}