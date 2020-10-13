#include "pch.h"
#include "RootSignature.h"
#include "Device.h"
#include "../Proxy/RootSignatureProxy.h"

//----------------------------------------------------------------------------------------------------
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

//----------------------------------------------------------------------------------------------------
RootDescriptorTable::RootDescriptorTable()
	: RootParameter(RootParameter::Type::DescriptorTable)
{
}

void RootDescriptorTable::AddDescriptorRange(DescriptorRange::Type Type, const DescriptorRange& DescriptorRange)
{
	D3D12_DESCRIPTOR_RANGE1 range = {};
	switch (Type)
	{
	case DescriptorRange::Type::SRV:		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; break;
	case DescriptorRange::Type::UAV:		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV; break;
	case DescriptorRange::Type::CBV:		range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV; break;
	case DescriptorRange::Type::Sampler:	range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER; break;
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

//----------------------------------------------------------------------------------------------------
RootSignature::RootSignature(const Device* pDevice, RootSignatureProxy& Proxy)
	: NumParameters(Proxy.m_Parameters.size()),
	NumStaticSamplers(Proxy.m_StaticSamplers.size())
{
	Proxy.Link();

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC VersionedRootSignatureDesc	= {};
	VersionedRootSignatureDesc.Version								= D3D_ROOT_SIGNATURE_VERSION_1_1;
	VersionedRootSignatureDesc.Desc_1_1								= Proxy.BuildD3DDesc();

	// Serialize the root signature.
	Microsoft::WRL::ComPtr<ID3DBlob> pSerializedRootSignatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> pErrorBlob;
	HRESULT hr = ::D3D12SerializeVersionedRootSignature(&VersionedRootSignatureDesc, &pSerializedRootSignatureBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		const char* pError = pErrorBlob ? reinterpret_cast<const char*>(pErrorBlob->GetBufferPointer()) : "";
		CORE_ERROR("{} Failed: {}", "D3D12SerializeVersionedRootSignature", pError);
	}

	// Create the root signature.
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateRootSignature(0, pSerializedRootSignatureBlob->GetBufferPointer(),
		pSerializedRootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
}