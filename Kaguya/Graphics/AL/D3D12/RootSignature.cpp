#include "pch.h"
#include "RootSignature.h"
#include "Device.h"

void RootSignature::Properties::AllowInputLayout()
{
	Desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
}

void RootSignature::Properties::DenyVSAccess()
{
	Desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_VERTEX_SHADER_ROOT_ACCESS;
}

void RootSignature::Properties::DenyHSAccess()
{
	Desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS;
}

void RootSignature::Properties::DenyDSAccess()
{
	Desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS;
}

void RootSignature::Properties::DenyGSAccess()
{
	Desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;
}

void RootSignature::Properties::DenyPSAccess()
{
	Desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
}

void RootSignature::Properties::AllowStreamOutput()
{
	Desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_ALLOW_STREAM_OUTPUT;
}

void RootSignature::Properties::SetAsLocalRootSignature()
{
	Desc.Flags |= D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;
}

RootSignature::RootSignature(Device* pDevice, const Properties& Properties)
{
	for (UINT i = 0; i < MaxDescriptorTables; ++i)
		m_NumDescriptorsPerTable[i] = 0;

	auto& rootSignatureDesc = Properties.Desc;
	UINT numParameters = rootSignatureDesc.NumParameters;
	D3D12_ROOT_PARAMETER1* pParameters = numParameters > 0 ? new D3D12_ROOT_PARAMETER1[numParameters] : nullptr;

	for (UINT i = 0; i < numParameters; ++i)
	{
		const D3D12_ROOT_PARAMETER1& rootParameter = rootSignatureDesc.pParameters[i];
		pParameters[i] = rootParameter;

		if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			UINT numDescriptorRanges = rootParameter.DescriptorTable.NumDescriptorRanges;
			D3D12_DESCRIPTOR_RANGE1* pDescriptorRanges = numDescriptorRanges > 0 ? new D3D12_DESCRIPTOR_RANGE1[numDescriptorRanges] : nullptr;

			memcpy(pDescriptorRanges, rootParameter.DescriptorTable.pDescriptorRanges, sizeof(decltype(*pDescriptorRanges)) * numDescriptorRanges);

			pParameters[i].DescriptorTable.NumDescriptorRanges = numDescriptorRanges;
			pParameters[i].DescriptorTable.pDescriptorRanges = pDescriptorRanges;

			// Set the bit mask depending on the type of descriptor table.
			if (numDescriptorRanges > 0)
			{
				switch (pDescriptorRanges[0].RangeType)
				{
				case D3D12_DESCRIPTOR_RANGE_TYPE_CBV:
				case D3D12_DESCRIPTOR_RANGE_TYPE_SRV:
				case D3D12_DESCRIPTOR_RANGE_TYPE_UAV:
					m_DescriptorTableBitMask.set(i, true);
					break;
				case D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER:
					m_SamplerTableBitMask.set(i, true);
					break;
				}
			}

			// Count the number of descriptors in the descriptor table.
			for (UINT j = 0; j < numDescriptorRanges; ++j)
			{
				if (pDescriptorRanges[j].NumDescriptors == UINT_MAX)
					m_NumDescriptorsPerTable[i] = pDescriptorRanges[j].NumDescriptors;
				else
					m_NumDescriptorsPerTable[i] += pDescriptorRanges[j].NumDescriptors;
			}
		}
	}

	m_RootSignatureDesc.NumParameters = numParameters;
	m_RootSignatureDesc.pParameters = pParameters;

	UINT numStaticSamplers = rootSignatureDesc.NumStaticSamplers;
	D3D12_STATIC_SAMPLER_DESC* pStaticSamplers = numStaticSamplers > 0 ? new D3D12_STATIC_SAMPLER_DESC[numStaticSamplers] : nullptr;

	if (pStaticSamplers)
	{
		memcpy(pStaticSamplers, rootSignatureDesc.pStaticSamplers, sizeof(D3D12_STATIC_SAMPLER_DESC) * numStaticSamplers);
	}

	m_RootSignatureDesc.NumStaticSamplers = numStaticSamplers;
	m_RootSignatureDesc.pStaticSamplers = pStaticSamplers;

	m_RootSignatureDesc.Flags = rootSignatureDesc.Flags;

	// Serialize the root signature.
	Microsoft::WRL::ComPtr<ID3DBlob> pSerializedRootSignatureBlob;
	Microsoft::WRL::ComPtr<ID3DBlob> pErrorBlob;

	D3D12_VERSIONED_ROOT_SIGNATURE_DESC desc = {};
	desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	desc.Desc_1_1 = m_RootSignatureDesc;

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
	for (UINT i = 0; i < MaxDescriptorTables; ++i)
		m_NumDescriptorsPerTable[i] = std::move(rvalue.m_NumDescriptorsPerTable[i]);
	m_DescriptorTableBitMask = std::move(rvalue.m_DescriptorTableBitMask);
	m_SamplerTableBitMask = std::move(rvalue.m_SamplerTableBitMask);

	ZeroMemory(&rvalue, sizeof(rvalue));
	return *this;
}

UINT RootSignature::GetNumDescriptorsAt(UINT RootParameterIndex) const
{
	assert(RootParameterIndex < MaxDescriptorTables);
	return m_NumDescriptorsPerTable[RootParameterIndex];
}