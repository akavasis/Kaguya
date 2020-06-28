#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <mutex>

#include "CommandAllocatorPool.h"

class CommandList;
class ResourceStateTracker;

class CommandQueue
{
public:
	CommandQueue(Device* pDevice, D3D12_COMMAND_LIST_TYPE Type);
	~CommandQueue();

	inline auto GetD3DCommandQueue() const { return m_pCommandQueue.Get(); }

	UINT64 Signal();
	void Flush();
	bool IsFenceValueReached(UINT64 FenceValue);
	void WaitForFenceValue(UINT64 FenceValue);
	void WaitForIdle();

	ID3D12CommandAllocator* RequestAllocator();
	void MarkAllocatorAsActive(UINT64 FenceValue, ID3D12CommandAllocator* Allocator);
private:
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_pCommandQueue;
	Microsoft::WRL::ComPtr<ID3D12Fence1> m_pFence;
	HANDLE m_EventHandle;
	UINT64 m_FenceValue;
	std::mutex m_Mutex;

	CommandAllocatorPool m_AllocatorPool;
};