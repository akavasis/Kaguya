#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <bitset>

class Device;

class RootSignature
{
public:
	struct Properties
	{
		D3D12_ROOT_SIGNATURE_DESC Desc;
	};

	RootSignature(Device* pDevice, const Properties& Properties);
	~RootSignature();

	inline auto GetD3DRootSignature() const { return m_RootSignature.Get(); }
	inline const auto& GetD3DRootSignatureDesc() const { return m_RootSignatureDesc; }

	std::bitset<32> GetDescriptorTableBitMask(D3D12_DESCRIPTOR_HEAP_TYPE DescriptorHeapType) const;
	UINT GetNumDescriptors(UINT RootParameterIndex) const;
private:
	Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;

	enum : UINT { MaxDescriptorTables = 32 };
	D3D12_ROOT_SIGNATURE_DESC m_RootSignatureDesc;

	// Need to know the number of descriptors per descriptor table.
	// A maximum of 32 descriptor tables are supported (since a 32-bit
	// mask is used to represent the descriptor tables in the root signature.
	UINT m_NumDescriptorsPerTable[MaxDescriptorTables];

	// A bit mask that represents the root parameter indices that are 
	// CBV, UAV, and SRV descriptor tables.
	std::bitset<MaxDescriptorTables> m_DescriptorTableBitMask;
	// A bit mask that represents the root parameter indices that are 
	// descriptor tables for Samplers.
	std::bitset<MaxDescriptorTables> m_SamplerTableBitMask;
};