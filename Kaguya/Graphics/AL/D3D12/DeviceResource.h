#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include "Core/Utility.h"
#include "Heap.h"

class Device;
class DeviceResourceProxy;

struct DeviceResourceAllocationInfo
{
	UINT64 SizeInBytes;
	UINT64 Alignment;
};

class Resource
{
public:
	enum class Type
	{
		Unknown,		// Unknown resource state
		Buffer,			// Buffer. Can be bound to all shader-stages
		Texture1D,		// 1D texture. Can be bound as render-target, shader-resource and UAV
		Texture2D,		// 2D texture. Can be bound as render-target, shader-resource and UAV
		Texture3D,		// 3D texture. Can be bound as render-target, shader-resource and UAV
		TextureCube,	// Cube texture. Can be bound as render-target, shader-resource and UAV
	};

	enum class BindFlags
	{
		None					= 0,		// The resource wont be bound to the pipeline
		RenderTarget			= 1 << 0,	// The resource will be bound as a render-target
		DepthStencil			= 1 << 1,	// The resource will be bound as a depth-stencil
		UnorderedAccess			= 1 << 2,	// The resource will be bound as an UAV
		AccelerationStructure	= 1 << 3,	// The resource will be bound as an acceleration structure
	};

	enum class State
	{
		Unknown					= 0,
		PreInitialized			= 1 << 0,
		Common					= 1 << 1,
		VertexBuffer			= 1 << 2,
		ConstantBuffer			= 1 << 3,
		IndexBuffer				= 1 << 4,
		RenderTarget			= 1 << 5,
		UnorderedAccess			= 1 << 6,
		DepthWrite				= 1 << 7,
		DepthRead				= 1 << 8,
		NonPixelShaderResource	= 1 << 9,
		PixelShaderResource		= 1 << 10,
		StreamOut				= 1 << 11,
		IndirectArgument		= 1 << 12,
		CopyDest				= 1 << 13,
		CopySource				= 1 << 14,
		ResolveDest				= 1 << 15,
		ResolveSource			= 1 << 16,
		AccelerationStructure	= 1 << 17,
		ShadingRateSource		= 1 << 18,
		GenericRead				= 1 << 19,
		Present					= 1 << 20,
		Predication				= 1 << 21
	};

	Resource() = default;
	Resource(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource);
	Resource(const Device* pDevice, DeviceResourceProxy& Proxy);
	Resource(const Device* pDevice, const Heap* pHeap, UINT64 HeapOffset, DeviceResourceProxy& Proxy);
	virtual ~Resource() = 0;

	inline auto GetApiHandle() const { return m_pResource.Get(); }
	inline auto GetType() const { return m_Type; }
	inline auto GetBindFlags() const { return m_BindFlags; }
	inline auto GetMemoryRequested() const { return m_MemoryRequested; }
	inline auto GetSizeInBytes() const { return m_SizeInBytes; }
	inline auto GetAlignment() const { return m_Alignment; }
	inline auto GetHeapOffset() const { return m_HeapOffset; }
	inline auto GetNumSubresources() const { return m_NumSubresources; }

	void SetDebugName(LPCWSTR Name)
	{
		m_pResource->SetName(Name);
	}
protected:
	Microsoft::WRL::ComPtr<ID3D12Resource>	m_pResource;
	Type									m_Type;
	BindFlags								m_BindFlags;
	UINT									m_NumSubresources;
	UINT64									m_MemoryRequested;
	UINT64									m_SizeInBytes;
	UINT64									m_Alignment;
	UINT64									m_HeapOffset;
};

ENABLE_BITMASK_OPERATORS(Resource::BindFlags);
ENABLE_BITMASK_OPERATORS(Resource::State);

D3D12_RESOURCE_DIMENSION GetD3DResourceDimension(Resource::Type Type);
D3D12_RESOURCE_FLAGS GetD3DResourceFlags(Resource::BindFlags Flags);
D3D12_RESOURCE_STATES GetD3DResourceStates(Resource::State State);