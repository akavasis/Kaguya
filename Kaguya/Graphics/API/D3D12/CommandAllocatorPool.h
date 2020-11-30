#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>
#include <queue>

#include <Core/Synchronization/CriticalSection.h>

class Device;

class CommandAllocatorPool
{
public:
	CommandAllocatorPool(const Device* pDevice, D3D12_COMMAND_LIST_TYPE Type);
	~CommandAllocatorPool();

	ID3D12CommandAllocator* RequestAllocator(UINT64 CompletedFenceValue);
	void MarkAllocatorAsActive(UINT64 FenceValue, ID3D12CommandAllocator* Allocator);
private:
	const Device* m_Device;
	const D3D12_COMMAND_LIST_TYPE m_Type;

	std::vector<ID3D12CommandAllocator*> m_AllocatorPool;
	std::queue<std::pair<UINT64, ID3D12CommandAllocator*>> m_ReadyAllocators;
	CriticalSection CriticalSection;
};