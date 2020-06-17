#pragma once
#include <optional>
#include "Device.h"

enum class HeapAliasingResourceCategory
{
	Buffers, NonRTDSTextures, RTDSTextures, AllResources
};

enum class CPUAccessibleHeapType
{
	Upload, Readback
};

class Heap
{
public:
	Heap(const Device* pDevice, UINT64 SizeInBytes, HeapAliasingResourceCategory HeapAliasingResourceCategories, std::optional<CPUAccessibleHeapType> CPUAccessibleHeapType);
	~Heap();

	inline auto GetD3DHeap() const { return m_pHeap.Get(); }
	inline auto GetCPUAccessibleHeapType() const { return m_CPUAccessibleHeapType; }
	inline auto GetSizeInBytesAligned() const { return m_SizeInBytesAligned; }
private:
	Microsoft::WRL::ComPtr<ID3D12Heap> m_pHeap;
	UINT64 m_SizeInBytesAligned;
	HeapAliasingResourceCategory m_HeapAliasingResourceCategories;
	std::optional<CPUAccessibleHeapType> m_CPUAccessibleHeapType;
};