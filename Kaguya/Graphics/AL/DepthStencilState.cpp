#include "pch.h"
#include "DepthStencilState.h"

DepthStencilState::Stencil::operator D3D12_DEPTH_STENCILOP_DESC() const noexcept
{
	D3D12_DEPTH_STENCILOP_DESC desc = {};
	desc.StencilFailOp = GetD3DStencilOp(StencilFailOp);
	desc.StencilDepthFailOp = GetD3DStencilOp(StencilDepthFailOp);
	desc.StencilPassOp = GetD3DStencilOp(StencilPassOp);
	desc.StencilFunc = GetD3DComparisonFunc(StencilFunc);

	return desc;
}

DepthStencilState::DepthStencilState()
{
	SetDepthEnable(true);
	SetDepthWrite(true);
	SetDepthFunc(ComparisonFunc::Less);
	SetStencilEnable(false);
	SetStencilReadMask(0xff);
	SetStencilWriteMask(0xff);
}

void DepthStencilState::SetDepthEnable(bool DepthEnable)
{
	m_DepthEnable = DepthEnable;
}

void DepthStencilState::SetDepthWrite(bool DepthWrite)
{
	m_DepthWrite = DepthWrite;
}

void DepthStencilState::SetDepthFunc(ComparisonFunc DepthFunc)
{
	m_DepthFunc = DepthFunc;
}

void DepthStencilState::SetStencilEnable(bool StencilEnable)
{
	m_StencilEnable = StencilEnable;
}

void DepthStencilState::SetStencilReadMask(UINT8 StencilReadMask)
{
	m_StencilReadMask = StencilReadMask;
}

void DepthStencilState::SetStencilWriteMask(UINT8 StencilWriteMask)
{
	m_StencilWriteMask = StencilWriteMask;
}

void DepthStencilState::SetStencilOp(Face Face, StencilOperation StencilFailOp, StencilOperation StencilDepthFailOp, StencilOperation StencilPassOp)
{
	if (Face == Face::FrontAndBack)
	{
		SetStencilOp(Face::Front, StencilFailOp, StencilDepthFailOp, StencilPassOp);
		SetStencilOp(Face::Back, StencilFailOp, StencilDepthFailOp, StencilPassOp);
	}
	Stencil& stencil = Face == Face::Front ? m_FrontFace : m_BackFace;
	stencil.StencilFailOp = StencilFailOp;
	stencil.StencilDepthFailOp = StencilDepthFailOp;
	stencil.StencilPassOp = StencilPassOp;
}

void DepthStencilState::SetStencilFunc(Face Face, ComparisonFunc StencilFunc)
{
	if (Face == Face::FrontAndBack)
	{
		SetStencilFunc(Face::Front, StencilFunc);
		SetStencilFunc(Face::Back, StencilFunc);
	}
	Stencil& stencil = Face == Face::Front ? m_FrontFace : m_BackFace;
	stencil.StencilFunc = StencilFunc;
}

DepthStencilState::operator D3D12_DEPTH_STENCIL_DESC() const noexcept
{
	D3D12_DEPTH_STENCIL_DESC desc = {};
	desc.DepthEnable = m_DepthEnable ? TRUE : FALSE;
	desc.DepthWriteMask = m_DepthWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
	desc.DepthFunc = GetD3DComparisonFunc(m_DepthFunc);
	desc.StencilEnable = m_StencilEnable ? TRUE : FALSE;
	desc.StencilReadMask = m_StencilReadMask;
	desc.StencilWriteMask = m_StencilWriteMask;
	desc.FrontFace = m_FrontFace;
	desc.BackFace = m_BackFace;

	return desc;
}

D3D12_STENCIL_OP GetD3DStencilOp(DepthStencilState::StencilOperation Op)
{
	switch (Op)
	{
	case DepthStencilState::StencilOperation::Keep: return D3D12_STENCIL_OP_KEEP;
	case DepthStencilState::StencilOperation::Zero: return D3D12_STENCIL_OP_ZERO;
	case DepthStencilState::StencilOperation::Replace: return D3D12_STENCIL_OP_REPLACE;
	case DepthStencilState::StencilOperation::IncreaseSaturate: return D3D12_STENCIL_OP_INCR_SAT;
	case DepthStencilState::StencilOperation::DecreaseSaturate: return D3D12_STENCIL_OP_DECR_SAT;
	case DepthStencilState::StencilOperation::Invert: return D3D12_STENCIL_OP_INVERT;
	case DepthStencilState::StencilOperation::Increase: return D3D12_STENCIL_OP_INCR;
	case DepthStencilState::StencilOperation::Decrease: return D3D12_STENCIL_OP_DECR;
	}
	return D3D12_STENCIL_OP();
}