#include "pch.h"
#include "RasterizerState.h"

RasterizerState::RasterizerState()
{
	SetFillMode(FillMode::Solid);
	SetCullMode(CullMode::Back);
	SetFrontCounterClockwise(false);
	SetDepthBias(0);
	SetDepthBiasClamp(0.0f);
	SetSlopeScaledDepthBias(0.0f);
	SetDepthClipEnable(true);
	SetMultisampleEnable(false);
	SetAntialiasedLineEnable(false);
	SetForcedSampleCount(0);
	SetConservativeRaster(false);
}

void RasterizerState::SetFillMode(FillMode FillMode)
{
	m_FillMode = FillMode;
}

void RasterizerState::SetCullMode(CullMode CullMode)
{
	m_CullMode = CullMode;
}

void RasterizerState::SetFrontCounterClockwise(bool FrontCounterClockwise)
{
	m_FrontCounterClockwise = FrontCounterClockwise;
}

void RasterizerState::SetDepthBias(int DepthBias)
{
	m_DepthBias = DepthBias;
}

void RasterizerState::SetDepthBiasClamp(float DepthBiasClamp)
{
	m_DepthBiasClamp = DepthBiasClamp;
}

void RasterizerState::SetSlopeScaledDepthBias(float SlopeScaledDepthBias)
{
	m_SlopeScaledDepthBias = SlopeScaledDepthBias;
}

void RasterizerState::SetDepthClipEnable(bool DepthClipEnable)
{
	m_DepthClipEnable = DepthClipEnable;
}

void RasterizerState::SetMultisampleEnable(bool MultisampleEnable)
{
	m_MultisampleEnable = MultisampleEnable;
}

void RasterizerState::SetAntialiasedLineEnable(bool AntialiasedLineEnable)
{
	m_AntialiasedLineEnable = AntialiasedLineEnable;
}

void RasterizerState::SetForcedSampleCount(unsigned int ForcedSampleCount)
{
	m_ForcedSampleCount = ForcedSampleCount;
}

void RasterizerState::SetConservativeRaster(bool ConservativeRaster)
{
	m_ConservativeRaster = ConservativeRaster;
}

RasterizerState::operator D3D12_RASTERIZER_DESC() const noexcept
{
	D3D12_RASTERIZER_DESC desc = {};
	desc.FillMode = GetD3DFillMode(m_FillMode);
	desc.CullMode = GetD3DCullMode(m_CullMode);
	desc.FrontCounterClockwise = m_FrontCounterClockwise ? TRUE : FALSE;
	desc.DepthBias = m_DepthBias;
	desc.DepthBiasClamp = m_DepthBiasClamp;
	desc.SlopeScaledDepthBias = m_SlopeScaledDepthBias;
	desc.DepthClipEnable = m_DepthClipEnable ? TRUE : FALSE;
	desc.MultisampleEnable = m_MultisampleEnable ? TRUE : FALSE;
	desc.AntialiasedLineEnable = m_AntialiasedLineEnable ? TRUE : FALSE;
	desc.ForcedSampleCount = m_ForcedSampleCount;
	desc.ConservativeRaster = m_ConservativeRaster ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF : D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON;

	return desc;
}

D3D12_FILL_MODE GetD3DFillMode(RasterizerState::FillMode FillMode)
{
	switch (FillMode)
	{
	case RasterizerState::FillMode::Wireframe: return D3D12_FILL_MODE_WIREFRAME;
	case RasterizerState::FillMode::Solid: return D3D12_FILL_MODE_SOLID;
	}
	return D3D12_FILL_MODE();
}

D3D12_CULL_MODE GetD3DCullMode(RasterizerState::CullMode CullMode)
{
	switch (CullMode)
	{
	case RasterizerState::CullMode::None: return D3D12_CULL_MODE_NONE;
	case RasterizerState::CullMode::Front: return D3D12_CULL_MODE_FRONT;
	case RasterizerState::CullMode::Back: return D3D12_CULL_MODE_BACK;
	}
	return D3D12_CULL_MODE();
}