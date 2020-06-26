#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include "DescriptorAllocator.h"

class Device
{
public:
	Device(IUnknown* pAdapter);

	inline auto GetD3DDevice() const { return m_pDevice.Get(); }
	inline UINT GetDescriptorIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE Type) const { return m_DescriptorHandleIncrementSizeCache[UINT(Type)]; }

	template<typename D3D12_XXX_VIEW_DESC> DescriptorAllocator& GetDescriptorAllocator();
	template<> [[nodiscard]] DescriptorAllocator& GetDescriptorAllocator<D3D12_CONSTANT_BUFFER_VIEW_DESC>() { return m_CBVAllocator; }
	template<> [[nodiscard]] DescriptorAllocator& GetDescriptorAllocator<D3D12_SHADER_RESOURCE_VIEW_DESC>() { return m_SRVAllocator; }
	template<> [[nodiscard]] DescriptorAllocator& GetDescriptorAllocator<D3D12_UNORDERED_ACCESS_VIEW_DESC>() { return m_UAVAllocator; }
	template<> [[nodiscard]] DescriptorAllocator& GetDescriptorAllocator<D3D12_RENDER_TARGET_VIEW_DESC>() { return m_RTVAllocator; }
	template<> [[nodiscard]] DescriptorAllocator& GetDescriptorAllocator<D3D12_DEPTH_STENCIL_VIEW_DESC>() { return m_DSVAllocator; }
	[[nodiscard]] inline DescriptorAllocator& GetSamplerDescriptorAllocator() { return m_SamplerAllocator; }
private:
	Microsoft::WRL::ComPtr<ID3D12Device> m_pDevice;
	UINT m_DescriptorHandleIncrementSizeCache[D3D12_DESCRIPTOR_HEAP_TYPE_NUM_TYPES];

	DescriptorAllocator m_CBVAllocator;
	DescriptorAllocator m_SRVAllocator;
	DescriptorAllocator m_UAVAllocator;
	DescriptorAllocator m_RTVAllocator;
	DescriptorAllocator m_DSVAllocator;
	DescriptorAllocator m_SamplerAllocator;
};