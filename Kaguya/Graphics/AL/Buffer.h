#pragma once
#include "../../Math/MathLibrary.h"
#include "Resource.h"

class Buffer : public Resource
{
public:
	template<typename T, bool UseConstantBufferAlignment>
	struct Properties
	{
		UINT NumElements;
		UINT Stride = UseConstantBufferAlignment ? Math::AlignUp(sizeof(T), D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) : sizeof(T);
		D3D12_RESOURCE_FLAGS Flags;

		D3D12_RESOURCE_STATES InitialState;
	};

	template<typename T, bool UseConstantBufferAlignment>
	Buffer(const RenderDevice* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties);
	template<typename T, bool UseConstantBufferAlignment>
	Buffer(const RenderDevice* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties, CPUAccessibleHeapType CPUAccessibleHeapType);
	template<typename T, bool UseConstantBufferAlignment>
	Buffer(const RenderDevice* pDevice, const Properties<T, UseConstantBufferAlignment>& Properties, const Heap* pHeap, UINT64 HeapOffset);
	~Buffer() override;

	BYTE* Map();
	void Unmap();
private:
	BYTE* m_pMappedData = nullptr;
	std::optional<CPUAccessibleHeapType> m_CPUAccessibleHeapType = std::nullopt;
};

#include "Buffer.inl"