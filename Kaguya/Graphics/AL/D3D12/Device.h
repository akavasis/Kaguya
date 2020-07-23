#pragma once
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>

class Device
{
public:
	Device(IDXGIAdapter4* pAdapter);
	~Device();

	inline auto GetD3DDevice() const { return m_pDevice.Get(); }
	inline auto GetDescriptorIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE Type) const { return m_DescriptorHandleIncrementSizeCache[UINT(Type)]; }
private:
	void CheckRS_1_1Support();
	void CheckSM6PlusSupport();
	void CheckRaytracingSupport();

	Microsoft::WRL::ComPtr<ID3D12Device5> m_pDevice;
	UINT m_DescriptorHandleIncrementSizeCache[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
};