#pragma once
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <unordered_set>
#include "D3D12AbstractionLayer.h"

class Device
{
public:
	void Create(IDXGIAdapter4* pAdapter);

	inline auto GetApiHandle() const { return m_ApiHandle.Get(); }
	inline auto GetDescriptorIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE Type) const { return m_DescriptorHandleIncrementSizeCache[UINT(Type)]; }

	bool IsUAVCompatable(DXGI_FORMAT Format) const;
private:
	void CheckRootSignature_1_1Support();
	void CheckShaderModel6PlusSupport();
	void CheckRaytracingSupport();
	void CheckMeshShaderSupport();

	Microsoft::WRL::ComPtr<ID3D12Device5>	m_ApiHandle;
	UINT									m_DescriptorHandleIncrementSizeCache[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
	std::unordered_set<DXGI_FORMAT>			m_UAVSupportedFormats;
};