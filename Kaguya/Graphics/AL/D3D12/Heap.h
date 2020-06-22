#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <optional>

class Device;

enum class HeapAliasingResourceCategory
{
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
		UINT64 SizeInBytes;
		HeapAliasingResourceCategory HeapAliasingResourceCategories;
		std::optional<CPUAccessibleHeapType> CPUAccessibleHeapType;
	};

	Heap(const Device* pDevice, const Properties& Properties);
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