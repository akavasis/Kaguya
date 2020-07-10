#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <optional>

class Device;

enum class HeapAliasingResourceCategory
{
	Unknown,
	Buffers,
	NonRTDSTextures,
	RTDSTextures,
	AllResources
};

enum class CPUAccessibleHeapType
{
	Upload,
	Readback
};

class Heap
{
public:
	struct Properties
	{
		UINT64 SizeInBytes; // Handles alignment
		HeapAliasingResourceCategory HeapAliasingResourceCategories;
		std::optional<CPUAccessibleHeapType> CPUAccessibleHeapType;
	};

	Heap() = default;
	Heap(const Device* pDevice, const Properties& Properties);
	~Heap();

	Heap(Heap&&) = default;
	Heap& operator=(Heap&&) = default;

	Heap(const Heap&) = delete;
	Heap& operator=(const Heap&) = delete;

	inline auto GetD3DHeap() const { return m_pHeap.Get(); }
	inline auto GetCPUAccessibleHeapType() const { return m_CPUAccessibleHeapType; }
	inline auto GetSizeInBytesAligned() const { return m_SizeInBytesAligned; }
private:
	Microsoft::WRL::ComPtr<ID3D12Heap> m_pHeap;
	UINT64 m_SizeInBytesAligned = 0;
	HeapAliasingResourceCategory m_HeapAliasingResourceCategories = HeapAliasingResourceCategory::Unknown;
	std::optional<CPUAccessibleHeapType> m_CPUAccessibleHeapType = std::nullopt;
};