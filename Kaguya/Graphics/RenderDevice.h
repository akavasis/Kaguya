#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <unordered_map>

#include "RenderHandle.h"

class RenderDevice
{
public:
	RenderDevice(IUnknown* pAdapter, BOOL GPUBasedValidation);

	inline auto GetD3DDevice() const { return m_pDevice.Get(); }
	inline UINT GetDescriptorIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE Type) const { m_DescriptorHandleIncrementSizeCache[UINT(Type)]; }
private:
	Microsoft::WRL::ComPtr<ID3D12Device> m_pDevice;
	UINT m_DescriptorHandleIncrementSizeCache[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];
};