#include "pch.h"
#include "RootSignature.h"
#include "Device.h"
#include "../Proxy/RootSignatureProxy.h"

RootSignature::RootSignature(const Device* pDevice, RootSignatureProxy& Proxy)
{
	Proxy.Link();

	UINT numParameters = static_cast<UINT>(Proxy.m_Parameters.size());
	D3D12_ROOT_PARAMETER1* pParameters = numParameters > 0 ? new D3D12_ROOT_PARAMETER1[numParameters] : nullptr;

	for (UINT i = 0; i < numParameters; ++i)
	{
		const D3D12_ROOT_PARAMETER1& rootParameter = Proxy.m_Parameters[i];
		pParameters[i] = rootParameter;

		if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			UINT numDescriptorRanges = rootParameter.DescriptorTable.NumDescriptorRanges;
			D3D12_DESCRIPTOR_RANGE1* pDescriptorRanges = numDescriptorRanges > 0 ? new D3D12_DESCRIPTOR_RANGE1[numDescriptorRanges] : nullptr;

			memcpy(pDescriptorRanges, rootParameter.DescriptorTable.pDescriptorRanges, sizeof(decltype(*pDescriptorRanges)) * numDescriptorRanges);

			pParameters[i].DescriptorTable.NumDescriptorRanges = numDescriptorRanges;
			pParameters[i].DescriptorTable.pDescriptorRanges = pDescriptorRanges;
		}
	}

	m_RootSignatureDesc.NumParameters = numParameters;
	m_RootSignatureDesc.pParameters = pParameters;

	UINT numStaticSamplers = Proxy.m_StaticSamplers.size();
	D3D12_STATIC_SAMPLER_DESC* pStaticSamplers = numStaticSamplers > 0 ? new D3D12_STATIC_SAMPLER_DESC[numStaticSamplers] : nullptr;

	if (pStaticSamplers)
	{
		memcpy(pStaticSamplers, Proxy.m_StaticSamplers.data(), sizeof(D3D12_STATIC_SAMPLER_DESC) * numStaticSamplers);
	}

	m_RootSignatureDesc.NumStaticSamplers = numStaticSamplers;
	m_RootSignatureDesc.pStaticSamplers = pStaticSamplers;

	m_RootSignatureDesc.Flags = Proxy.m_Flags;

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {};
	desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	desc.Desc_1_1 = Proxy.BuildD3DDesc();

	// Serialize the root signature.
	Microsoft::WRL::ComPtr<ID3DBlob> pSerializedRootSignatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> pErrorBlob;
	HRESULT hr = ::D3D12SerializeVersionedRootSignature(&desc, &pSerializedRootSignatureBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		const char* pError = pErrorBlob ? reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()) : "";
		CORE_ERROR("{} Failed: {}", "D3D12SerializeVersionedRootSignature", pError);
	}

	// Create the root signature.
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateRootSignature(0, pSerializedRootSignatureBlob->GetBufferPointer(),
		pSerializedRootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
}

RootSignature::~RootSignature()
{
	for (UINT i = 0; i < m_RootSignatureDesc.NumParameters; ++i)
	{
		const auto& rootParameter = m_RootSignatureDesc.pParameters[i];
		if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			delete[] rootParameter.DescriptorTable.pDescriptorRanges;
		}
	}

	delete[] m_RootSignatureDesc.pParameters;
	delete[] m_RootSignatureDesc.pStaticSamplers;
}

RootSignature::RootSignature(RootSignature&& rvalue) noexcept
{
	*this = std::move(rvalue);
}

RootSignature& RootSignature::operator=(RootSignature&& rvalue) noexcept
{
	m_RootSignature = std::move(rvalue.m_RootSignature);
	m_RootSignatureDesc = std::move(rvalue.m_RootSignatureDesc);
	ZeroMemory(&rvalue, sizeof(rvalue));
	return *this;
}

RootParameter::RootParameter(Type Type)
{
	switch (Type)
	{
	case RootParameter::Type::DescriptorTable:	m_RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; break;
	case RootParameter::Type::Constants:		m_RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS; break;
	case RootParameter::Type::CBV:				m_RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; break;
	case RootParameter::Type::SRV:				m_RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV; break;
	case RootParameter::Type::UAV:				m_RootParameter.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV; break;
	}
	m_RootParameter.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
}

RootDescriptorTable::RootDescriptorTable()
	: RootParameter(RootParameter::Type::DescriptorTable)
{
}

void RootDescriptorTable::AddDescriptorRange(DescriptorRange::Type Type, const DescriptorRange& DescriptorRange)
{
	D3D12_DESCRIPTOR_RANGE1 range = {};
	switch (Type)
	{
	case DescriptorRange::Type::SRV: range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; break;
	case DescriptorRange::Type::UAV: range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; break;
	case DescriptorRange::Type::CBV: range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; break;
	case DescriptorRange::Type::Sampler: range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; break;
	}
	range.NumDescriptors = DescriptorRange.NumDescriptors;
	range.BaseShaderRegister = DescriptorRange.BaseShaderRegister;
	range.RegisterSpace = DescriptorRange.RegisterSpace;
	range.Flags = DescriptorRange.Flags;
	range.OffsetInDescriptorsFromTableStart = DescriptorRange.OffsetInDescriptorsFromTableStart;

	m_DescriptorRanges.push_back(range);

	m_RootParameter.DescriptorTable.NumDescriptorRanges = m_DescriptorRanges.size();
	m_RootParameter.DescriptorTable.pDescriptorRanges = m_DescriptorRanges.data();
}