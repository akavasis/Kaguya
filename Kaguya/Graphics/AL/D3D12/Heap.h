#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include "Core/Utility.h"

class Device;
class HeapProxy;

extern const D3D12_HEAP_PROPERTIES kDefaultHeapProps;
extern const D3D12_HEAP_PROPERTIES kUploadHeapProps;
extern const D3D12_HEAP_PROPERTIES kReadbackHeapProps;

class Heap
{
public:
	enum class Type
	{
		Default,
		Upload,
		Readback
	};

	enum class Flags
	{
		None			= 0,
		Buffers			= 1 << 0,
		NonRTDSTextures = 1 << 1,
		RTDSTextures	= 1 << 2,
		AllResources	= 1 << 3
	};

	Heap() = default;
	Heap(const Device* pDevice, HeapProxy& Proxy);

	inline auto GetApiHandle() const { return m_pHeap.Get(); }
	inline auto GetSizeInBytes() const { return m_SizeInBytes; }
	inline auto GetType() const { return m_Type; }
	inline auto GetFlags() const { return m_Flags; }
private:
	Microsoft::WRL::ComPtr<ID3D12Heap>	m_pHeap;
	UINT64								m_SizeInBytes;
	Type								m_Type;
	Flags								m_Flags;
};

ENABLE_BITMASK_OPERATORS(Heap::Flags);

D3D12_HEAP_FLAGS GetD3DHeapFlags(Heap::Flags Flags);