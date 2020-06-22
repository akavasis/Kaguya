#include "pch.h"
#include "PipelineState.h"
#include "Device.h"

PipelineState::PipelineState(Type Type)
	: m_Type(Type)
{
}

PipelineState::~PipelineState()
{
}

GraphicsPipelineState::GraphicsPipelineState(Device* pDevice, const Properties& Properties)
	: PipelineState(PipelineState::Type::Graphics)
{
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateGraphicsPipelineState(&Properties.Desc, IID_PPV_ARGS(m_PipelineState.ReleaseAndGetAddressOf())));
}

GraphicsPipelineState::~GraphicsPipelineState()
{
}

ComputePipelineState::ComputePipelineState(Device* pDevice, const Properties& Properties)
	: PipelineState(PipelineState::Type::Compute)
{
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateComputePipelineState(&Properties.Desc, IID_PPV_ARGS(m_PipelineState.ReleaseAndGetAddressOf())));
}

ComputePipelineState::~ComputePipelineState()
{
}