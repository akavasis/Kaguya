#pragma once
#include <wrl/client.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <unordered_set>

#include <D3D12MemAlloc.h>

class Device
{
public:
	~Device();

	void Create(IDXGIAdapter4* pAdapter);

	operator auto() const { return m_Device5.Get(); }
	auto operator->() { return m_Device5.Get(); }

	inline auto Allocator() const { return m_Allocator; }

	bool IsUAVCompatable(DXGI_FORMAT Format) const;
private:
	void CheckRootSignature_1_1Support();
	void CheckShaderModel6PlusSupport();
	void CheckRaytracingSupport();
	void CheckMeshShaderSupport();

	Microsoft::WRL::ComPtr<ID3D12Device5>	m_Device5;
	D3D12MA::Allocator*						m_Allocator = nullptr;
	std::unordered_set<DXGI_FORMAT>			m_UAVSupportedFormats;
};