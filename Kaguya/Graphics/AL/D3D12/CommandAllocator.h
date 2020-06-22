#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <vector>
#include <queue>
#include <mutex>

class Device;

class CommandAllocator
{
public:
	CommandAllocator(const Device* pDevice, D3D12_COMMAND_LIST_TYPE Type);

	inline auto GetD3DCommandAllocator() const { return m_CmdAllocator.Get(); }

	void Reset();
private:
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> m_CmdAllocator;
};

class CommandAllocatorPool
{
public:
	CommandAllocatorPool(const Device* pDevice, D3D12_COMMAND_LIST_TYPE Type);
	~CommandAllocatorPool();

	ID3D12CommandAllocator* RequestAllocator(UINT64 CompletedFenceValue);
	void MarkAllocatorAsActive(UINT64 FenceValue, ID3D12CommandAllocator* Allocator);

	inline size_t Size() { return m_AllocatorPool.size(); }

private:
	const Device* m_Device;
	const D3D12_COMMAND_LIST_TYPE m_Type;

	std::vector<ID3D12CommandAllocator*> m_AllocatorPool;
	std::queue<std::pair<UINT64, ID3D12CommandAllocator*>> m_ReadyAllocators;
	std::mutex m_AllocatorMutex;
};