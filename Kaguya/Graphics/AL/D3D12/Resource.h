#pragma once
#include <optional>

#include "Heap.h"

class Device;

class Resource
{
public:
	enum class Type { Unknown, Buffer, Texture1D, Texture2D, Texture3D };

	Resource() = default;
	Resource(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource);
	virtual ~Resource() = 0;

	Resource(Resource&&) noexcept = default;
	Resource& operator=(Resource&&) noexcept = default;

	Resource(const Resource&) = delete;
	Resource& operator=(const Resource&) = delete;

	inline auto GetD3DResource() const { return m_pResource.Get(); }
	inline auto GetInitialState() const { return m_InitialState; }
	inline auto GetNumSubresources() const { return m_NumSubresources; }

	void Release();
protected:
	class Properties
	{
	public:
		static Resource::Properties Buffer(UINT64 SizeInBytes, D3D12_RESOURCE_FLAGS Flags);
		static Resource::Properties Texture(Resource::Type Type, DXGI_FORMAT Format,
			UINT64 Width, UINT Height, UINT16 DepthOrArraySize, UINT16 MipLevels,
			D3D12_RESOURCE_FLAGS Flags, const D3D12_CLEAR_VALUE* pOptimizedClearValue);

		inline const auto& GetD3DResourceDesc() const { return m_Desc; }
		inline auto GetD3DClearValue() const { return m_ClearValue; }
		inline auto GetNumSubresources() const { return m_NumSubresources; }
	private:
		D3D12_RESOURCE_DESC m_Desc;
		std::optional<D3D12_CLEAR_VALUE> m_ClearValue;
		UINT m_NumSubresources;
	};
	Resource(const Device* pDevice, const Resource::Properties& Properties, D3D12_RESOURCE_STATES InitialState);
	Resource(const Device* pDevice, const Resource::Properties& Properties, CPUAccessibleHeapType CPUAccessibleHeapType);
	Resource(const Device* pDevice, const Resource::Properties& Properties, D3D12_RESOURCE_STATES InitialState, const Heap* pHeap, UINT64 HeapOffset);

	Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;
	D3D12_RESOURCE_STATES m_InitialState;
private:
	std::optional<D3D12_CLEAR_VALUE> m_ClearValue = std::nullopt;
	D3D12_RESOURCE_ALLOCATION_INFO m_ResourceAllocationInfo = {};
	UINT64 m_HeapOffset = 0;
	UINT m_NumSubresources = 0;
	D3D12_RESOURCE_FLAGS m_Flags = D3D12_RESOURCE_FLAG_NONE;
};