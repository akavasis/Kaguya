#pragma once
#include "Resource.h"
#include <cassert>

class Device;
class BufferProxy;

class Buffer : public Resource
{
public:
	enum class CpuAccess
	{
		None,	// The CPU can't access the buffer's content. The buffer can be updated using CommandContext
		Write,	// The buffer can be mapped for CPU writes
		Read	// The buffer can be mapped for CPU reads
	};

	Buffer() = default;
	Buffer(Microsoft::WRL::ComPtr<ID3D12Resource> ExistingID3D12Resource);
	Buffer(const Device* pDevice, BufferProxy& Proxy);
	Buffer(const Device* pDevice, const Heap* pHeap, UINT64 HeapOffset, BufferProxy& Proxy);
	~Buffer() override;

	inline auto GetStride() const { return m_Stride; }
	inline auto GetCpuAccess() const { return m_CpuAccess; }

	BYTE* Map();
	template<typename T>
	T* Map()
	{
		return reinterpret_cast<T*>(Map());
	}
	void Unmap();

	D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddress() const;
	D3D12_GPU_VIRTUAL_ADDRESS GetGpuVirtualAddressAt(INT Index) const;
	template<typename T>
	void Update(INT ElementIndex, const T& Data)
	{
		assert(m_Stride != 0);
		assert(m_pMappedData && "Map() has not been called, invalid ptr");
		memcpy(&m_pMappedData[ElementIndex * m_Stride], &Data, sizeof(T));
	}
private:
	UINT		m_Stride		= 0;
	CpuAccess	m_CpuAccess		= CpuAccess::None;
	BYTE*		m_pMappedData	= nullptr;
};