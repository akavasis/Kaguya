#pragma once

#include <wrl/client.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <unordered_set>

class Device
{
public:
	operator auto() const { return m_Device5.Get(); }

	void Create(IDXGIAdapter4* pAdapter);

	inline auto GetApiHandle() const { return m_Device5.Get(); }

	bool IsUAVCompatable(DXGI_FORMAT Format) const;
private:
	void CheckRootSignature_1_1Support();
	void CheckShaderModel6PlusSupport();
	void CheckRaytracingSupport();
	void CheckMeshShaderSupport();

	Microsoft::WRL::ComPtr<ID3D12Device5>	m_Device5;
	std::unordered_set<DXGI_FORMAT>			m_UAVSupportedFormats;
};