#pragma once
#include <wrl/client.h>
#include <d3d12.h>

#include "CommandAllocatorPool.h"

class CommandQueue
{
public:
	void Create(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE Type);

	operator auto() { return m_CommandQueue.Get(); }
	auto operator->() { return m_CommandQueue.Get(); }

	ID3D12CommandAllocator* RequestCommandAllocator(UINT64 CompletedValue);
	void DiscardCommandAllocator(UINT64 FenceValue, ID3D12CommandAllocator* pCommandAllocator);
private:
	Microsoft::WRL::ComPtr<ID3D12CommandQueue>	m_CommandQueue;
	CommandAllocatorPool						m_CommandAllocatorPool;
};