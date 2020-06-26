#include "pch.h"
#include "RootSignature.h"
#include "Device.h"
using Microsoft::WRL::ComPtr;

RootSignature::RootSignature(Device* pDevice, const Properties& Properties)
{
	auto& rootSignatureDesc = Properties.Desc;
	UINT numParameters = rootSignatureDesc.NumParameters;
	D3D12_ROOT_PARAMETER* pParameters = numParameters > 0 ? new D3D12_ROOT_PARAMETER[numParameters] : nullptr;

	for (UINT i = 0; i < numParameters; ++i)
	{
		const D3D12_ROOT_PARAMETER& rootParameter = rootSignatureDesc.pParameters[i];
		pParameters[i] = rootParameter;

		if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			UINT numDescriptorRanges = rootParameter.DescriptorTable.NumDescriptorRanges;
			D3D12_DESCRIPTOR_RANGE* pDescriptorRanges = numDescriptorRanges > 0 ? new D3D12_DESCRIPTOR_RANGE[numDescriptorRanges] : nullptr;

			memcpy(pDescriptorRanges, rootParameter.DescriptorTable.pDescriptorRanges,
				sizeof(D3D12_DESCRIPTOR_RANGE) * numDescriptorRanges);

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
	ComPtr<ID3DBlob> pSerializedRootSignatureBlob;
	ComPtr<ID3DBlob> pErrorBlob;

	ThrowCOMIfFailed(D3D12SerializeRootSignature(&m_RootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &pSerializedRootSignatureBlob, &pErrorBlob));

	if (pErrorBlob)
	{
		::OutputDebugString((LPCWSTR)pErrorBlob->GetBufferPointer());
	}

	// Create the root signature.
	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateRootSignature(0, pSerializedRootSignatureBlob->GetBufferPointer(),
		pSerializedRootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_RootSignature)));
}

RootSignature::~RootSignature()
{
	for (UINT i = 0; i < m_RootSignatureDesc.NumParameters; ++i)
	{
		const D3D12_ROOT_PARAMETER& rootParameter = m_RootSignatureDesc.pParameters[i];
		if (rootParameter.ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
		{
			delete[] rootParameter.DescriptorTable.pDescriptorRanges;
		}
	}

	delete[] m_RootSignatureDesc.pParameters;
	m_RootSignatureDesc.pParameters = nullptr;
	m_RootSignatureDesc.NumParameters = 0;

	delete[] m_RootSignatureDesc.pStaticSamplers;
	m_RootSignatureDesc.pStaticSamplers = nullptr;
	m_RootSignatureDesc.NumStaticSamplers = 0;
}

std::bitset<RootSignature::MaxDescriptorTables> RootSignature::GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType) const
{
	switch (DescriptorHeapType)
	{
	case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:
		return m_DescriptorTableBitMask;
	case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:
		return m_SamplerTableBitMask;
	}
	return 0;
}

UINT RootSignature::GetNumDescriptors(UINT RootParameterIndex) const
{
	assert(RootParameterIndex < MaxDescriptorTables);
	return m_NumDescriptorsPerTable[RootParameterIndex];
}