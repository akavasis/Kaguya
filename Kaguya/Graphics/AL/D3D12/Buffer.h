#pragma once
#include "Resource.h"
#include "../../../Math/MathLibrary.h"

class Buffer : public Resource
{
public:
	template<typename T, bool UseConstantBufferAlignment>
	struct Properties
	{
		UINT NumElements;
		UINT Stride = UseConstantBufferAlignment ? Math::AlignUp<UINT>(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) : sizeof(T);
		D3D12_RESOURCE_FLAGS Flags;

		// Ignored if this is created with CPUAccessibleHeapType
		D3D12_RESOURCE_STATES InitialState;
	};

	Buffer() = default;
	template<typename T, bool UseConstantBufferAlignment>
	Buffer(const Device* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties);
	template<typename T, bool UseConstantBufferAlignment>
	Buffer(const Device* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties, CPUAccessibleHeapType CPUAccessibleHeapType, const void* pData /*pData is optional for upload vb and ib data*/);
	template<typename T, bool UseConstantBufferAlignment>
	Buffer(const Device* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties, const Heap* pHeap, UINT64 HeapOffset);
	~Buffer() override;

	Buffer(Buffer&&) = default;
	Buffer& operator=(Buffer&&) = default;

	Buffer(const Buffer&) = delete;
	Buffer& operator=(const Buffer&) = delete;

	inline auto GetNumElements() const { return m_NumElements; }
	inline auto GetStride() const { return m_Stride; }

	BYTE* Map();
	void Unmap();

	D3D12_GPU_VIRTUAL_ADDRESS GetBufferLocationAt(INT Index)
	{
		assert(Index >= 0 && Index < m_NumElements && "Index is out of range in Buffer");
		return UINT64(INT64(m_pResource->GetGPUVirtualAddress()) + INT64(Index) * INT64(m_Stride));
	}
	template<typename T>
	void Update(UINT ElementIndex, const T& Data);
private:
	BYTE* m_pMappedData = nullptr;
	UINT m_NumElements = 0;
	UINT m_Stride = 0;
	std::optional<CPUAccessibleHeapType> m_CPUAccessibleHeapType = std::nullopt;
};

#include "Buffer.inl"