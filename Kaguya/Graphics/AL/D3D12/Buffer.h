#pragma once
#include "Resource.h"
#include "Math/MathLibrary.h"

class Buffer : public Resource
{
public:
	struct Properties
	{
		UINT64 SizeInBytes;
		UINT Stride;
		D3D12_RESOURCE_FLAGS Flags;

		// Ignored if this is created with CPUAccessibleHeapType
		D3D12_RESOURCE_STATES InitialState;
	};

	Buffer() = default;
	Buffer(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource);
	Buffer(const Device* pDevice, const Properties& Properties);
	Buffer(const Device* pDevice, const Properties& Properties, CPUAccessibleHeapType CPUAccessibleHeapType, const void* pData /*pData is optional for upload vb and ib data*/);
	Buffer(const Device* pDevice, const Properties& Properties, const Heap* pHeap, UINT64 HeapOffset);
	~Buffer() override;

	Buffer(Buffer&&) = default;
	Buffer& operator=(Buffer&&) = default;

	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;

	inline auto GetSizeInBytes() const { return m_SizeInBytes; }
	inline auto GetStride() const { return m_Stride; }
	inline auto GetCPUAccessibleHeapType() const { return m_CPUAccessibleHeapType; }

	BYTE* Map();
	void Unmap();

	D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const;
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddressAt(INT Index) const;
	template<typename T>
	void Update(INT ElementIndex, const T& Data);
private:
	BYTE* m_pMappedData = nullptr;
	UINT m_SizeInBytes = 0;
	UINT m_Stride = 0;
	std::optional<CPUAccessibleHeapType> m_CPUAccessibleHeapType = std::nullopt;
};

#include "Buffer.inl"