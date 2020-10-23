#include "pch.h"
#include "RootSignatureProxy.h"
#include "../D3D12/d3dx12.h"

RootSignatureProxy::RootSignatureProxy()
{
	m_Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;
}

void RootSignatureProxy::AddRootDescriptorTableParameter(const RootDescriptorTable& RootDescriptorTable)
{
	// Add the root parameter to the set of parameters,
	m_Parameters.push_back(RootDescriptorTable.GetD3DRootParameter());
	// The descriptor table descriptor ranges require a pointer to the descriptor ranges. Since new
	// ranges can be dynamically added in the vector, we separately store the index of the range set.
	// The actual address will be solved when generating the actual root signature
	m_DescriptorRangeIndices.push_back(m_DescriptorRanges.size());
	m_DescriptorRanges.push_back(RootDescriptorTable.GetDescriptorRanges());
}

void RootSignatureProxy::AddRootCBVParameter(const RootCBV& RootCBV)
{
	// Add the root parameter to the set of parameters,
	m_Parameters.push_back(RootCBV.GetD3DRootParameter());
	// and indicate that there will be no range
	// location to indicate since this parameter is not part of the heap
	m_DescriptorRangeIndices.push_back(~0);
}

void RootSignatureProxy::AddRootSRVParameter(const RootSRV& RootSRV)
{
	// Add the root parameter to the set of parameters,
	m_Parameters.push_back(RootSRV.GetD3DRootParameter());
	// and indicate that there will be no range
	// location to indicate since this parameter is not part of the heap
	m_DescriptorRangeIndices.push_back(~0);
}

void RootSignatureProxy::AddRootUAVParameter(const RootUAV& RootUAV)
{
	// Add the root parameter to the set of parameters,
	m_Parameters.push_back(RootUAV.GetD3DRootParameter());
	// and indicate that there will be no range
	// location to indicate since this parameter is not part of the heap
	m_DescriptorRangeIndices.push_back(~0);
}

void RootSignatureProxy::AddStaticSampler
(
	UINT ShaderRegister,
	D3D12_FILTER Filter,
	D3D12_TEXTURE_ADDRESS_MODE AddressUVW,
	UINT MaxAnisotropy,
	D3D12_COMPARISON_FUNC ComparisonFunc,
	D3D12_STATIC_BORDER_COLOR BorderColor
)
{
	CD3DX12_STATIC_SAMPLER_DESC StaticSampler = {};
	{
		StaticSampler.Init(ShaderRegister, Filter, AddressUVW, AddressUVW, AddressUVW, 0.0f, MaxAnisotropy, ComparisonFunc, BorderColor);
	}
	m_StaticSamplers.push_back(StaticSampler);
}

void RootSignatureProxy::AllowInputLayout()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
}

void RootSignatureProxy::DenyVSAccess()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
}

void RootSignatureProxy::DenyHSAccess()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
}

void RootSignatureProxy::DenyDSAccess()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
}

void RootSignatureProxy::DenyTessellationShaderAccess()
{
	DenyHSAccess();
	DenyDSAccess();
}

void RootSignatureProxy::DenyGSAccess()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
}

void RootSignatureProxy::DenyPSAccess()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
}

void RootSignatureProxy::AllowStreamOutput()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;
}

void RootSignatureProxy::SetAsLocalRootSignature()
{
	m_Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
}

void RootSignatureProxy::Link()
{
	// Go through all the parameters, and set the actual addresses of the heap range descriptors based
	// on their indices in the range indices vector
	for (size_t i = 0; i < m_Parameters.size(); ++i)
	{
		if (m_Parameters[i].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			m_Parameters[i].DescriptorTable.pDescriptorRanges = m_DescriptorRanges[m_DescriptorRangeIndices[i]].data();
		}
	}
}

D3D12_ROOT_SIGNATURE_DESC1 RootSignatureProxy::BuildD3DDesc()
{
	D3D12_ROOT_SIGNATURE_DESC1 Desc = {};
	{
		Desc.NumParameters		= static_cast<UINT>(m_Parameters.size());
		Desc.pParameters		= m_Parameters.data();
		Desc.NumStaticSamplers	= static_cast<UINT>(m_StaticSamplers.size());
		Desc.pStaticSamplers	= m_StaticSamplers.data();
		Desc.Flags				= m_Flags;
	}
	return Desc;
}