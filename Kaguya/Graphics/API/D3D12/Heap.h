#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class Heap
{
public:
	Heap() = default;
	Heap(ID3D12Device* pDevice, const D3D12_HEAP_DESC& Desc);

	inline auto GetApiHandle() const { return m_ApiHandle.Get(); }
private:
	Microsoft::WRL::ComPtr<ID3D12Heap> m_ApiHandle;
};