#pragma once
#include <optional>

#include "Heap.h"
#include "../../../Core/Utility.h"

class Device;
class Resource;

enum class ResourceStates
{
	Common = 0,
	VertexAndConstantBuffer = 1 << 0,
	IndexBuffer = 1 << 1,
	RenderTarget = 1 << 2,
	UnorderedAccess = 1 << 3,
	DepthWrite = 1 << 4,
	DepthRead = 1 << 5,
	NonPixelShaderResource = 1 << 6,
	PixelShaderResource = 1 << 7,
	StreamOut = 1 << 8,
	IndirectArgument = 1 << 9,
	CopyDest = 1 << 10,
	CopySource = 1 << 11,
	ResolveDest = 1 << 12,
	ResolveSource = 1 << 13,
	RayTracingAccelerationStructure = 1 << 14,
	ShadingRateSource = 1 << 15,
	Present = 0,
};
ENABLE_BITMASK_OPERATORS(ResourceStates);

enum class ResourceBarrierType
{
	Transition,
	Aliasing,
	UAV
};

class ResourceBarrier
{
public:
	ResourceBarrierType Type;
	union
	{
		struct ResourceTransitionBarrier
		{
			Resource* pResource;
			unsigned int Subresource;
			ResourceStates StateBefore;
			ResourceStates StateAfter;
		} Transition;

		struct ResourceAliasingBarrier
		{
			Resource* pResourceBefore;
			Resource* pResourceAfter;
		} Aliasing;

		struct ResourceUAVBarrier
		{
			Resource* pResource;
		} UAV;
	};
};

D3D12_RESOURCE_STATES GetD3DResourceStates(ResourceStates ResourceState);
D3D12_RESOURCE_BARRIER GetD3DResourceBarrier(const ResourceBarrier& ResourceBarrier);

class Resource
{
public:
	enum class Type { Buffer, Texture1D, Texture2D, Texture3D };

	Resource() = default;
	Resource(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource);
	virtual ~Resource() = 0;

	Resource(Resource&&) = default;
	Resource& operator=(Resource&&) = default;

	Resource(const Resource&) = delete;
	Resource& operator=(const Resource&) = delete;

	inline auto GetD3DResource() const { return m_pResource.Get(); }
	inline auto GetGPUVirtualAddress() const { return m_pResource->GetGPUVirtualAddress(); }
	inline auto GetNumSubresources() const { return m_NumSubresources; }

	void Release();
protected:
	class Properties
	{
	public:
		static Resource::Properties Buffer(
			UINT64 Width,
			D3D12_RESOURCE_FLAGS Flags);
		static Resource::Properties Texture(
			Resource::Type Type,
			DXGI_FORMAT Format,
			UINT64 Width,
			UINT Height,
			UINT16 DepthOrArraySize,
			UINT16 MipLevels,
			D3D12_RESOURCE_FLAGS Flags,
			const D3D12_CLEAR_VALUE* pOptimizedClearValue);

		inline const auto& GetD3DResourceDesc() const { return m_Desc; }
		inline auto GetD3DClearValue() const { return m_ClearValue; }
		inline auto GetNumSubresources() const { return m_NumSubresources; }
	private:
		D3D12_RESOURCE_DESC m_Desc;
		std::optional<D3D12_CLEAR_VALUE> m_ClearValue;
		UINT m_NumSubresources;
	};
	Resource(const Device* pDevice, const Resource::Properties& Desc, ResourceStates InitialState);
	Resource(const Device* pDevice, const Resource::Properties& Desc, CPUAccessibleHeapType CPUAccessibleHeapType);
	Resource(const Device* pDevice, const Resource::Properties& Desc, ResourceStates InitialState, const Heap* pHeap, UINT64 HeapOffset);

	Microsoft::WRL::ComPtr<ID3D12Resource> m_pResource;
private:
	std::optional<D3D12_CLEAR_VALUE> m_ClearValue = std::nullopt;
	D3D12_RESOURCE_ALLOCATION_INFO m_ResourceAllocationInfo = {};
	UINT64 m_HeapOffset = 0;
	UINT m_NumSubresources = 0;
};