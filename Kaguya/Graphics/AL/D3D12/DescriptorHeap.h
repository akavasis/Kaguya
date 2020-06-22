#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <map>
#include <optional>

#include "Descriptor.h"

class Device;

class DescriptorHeap
{
public:
	DescriptorHeap(Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE Type, UINT NumDescriptors);

	// Returns std::null_opt if m_NumFreeHandles < NumDescriptors
	std::optional<Descriptor> Allocate(UINT NumDescriptors, UINT DescriptorIncrementSize);
	void Free(const Descriptor& Descriptor, UINT DescriptorIncrementSize);
private:
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_pDescriptorHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_BaseDescriptorHandle;
	INT m_NumFreeHandles;

	using Offset = UINT;
	using Size = UINT;

	struct FreeBlocksByOffsetPoolIter;
	struct FreeBlocksBySizePoolIter;
	using FreeBlocksByOffsetPool = std::map<Offset, FreeBlocksBySizePoolIter>;
	using FreeBlocksBySizePool = std::multimap<Size, FreeBlocksByOffsetPoolIter>;

	struct FreeBlocksByOffsetPoolIter
	{
		FreeBlocksByOffsetPool::iterator Iterator;
		FreeBlocksByOffsetPoolIter(FreeBlocksByOffsetPool::iterator iter) : Iterator(iter) {}
	};

	struct FreeBlocksBySizePoolIter
	{
		FreeBlocksBySizePool::iterator Iterator;
		FreeBlocksBySizePoolIter(FreeBlocksBySizePool::iterator iter) : Iterator(iter) {}
	};

	FreeBlocksByOffsetPool m_FreeBlocksByOffsetPool;
	FreeBlocksBySizePool m_SizePool;

	void AllocateFreeBlock(Offset offset, Size size);
	void FreeBlock(Offset offset, Size size);
};