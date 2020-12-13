#pragma once
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>
#include <unordered_set>

class Device
{
public:
	void Create(IDXGIAdapter4* pAdapter);

	inline auto GetApiHandle() const { return m_ApiHandle.Get(); }

	bool IsUAVCompatable(DXGI_FORMAT Format) const;
private:
	void CheckRootSignature_1_1Support();
	void CheckShaderModel6PlusSupport();
	void CheckRaytracingSupport();
	void CheckMeshShaderSupport();

	Microsoft::WRL::ComPtr<ID3D12Device5>	m_ApiHandle;
	std::unordered_set<DXGI_FORMAT>			m_UAVSupportedFormats;
};