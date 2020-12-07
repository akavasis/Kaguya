#include "pch.h"
#include "CommandAllocatorPool.h"

CommandAllocatorPool::CommandAllocatorPool(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE Type)
	: m_pDevice(pDevice),
	m_Type(Type)
{

}

CommandAllocatorPool::~CommandAllocatorPool()
{
	for (auto Allocator : m_CommandAllocatorPool)
	{
		Allocator->Release();
	}
	m_CommandAllocatorPool.clear();
}

ID3D12CommandAllocator* CommandAllocatorPool::RequestCommandAllocator(UINT64 CompletedFenceValue)
{
	ScopedCriticalSection SCS(CriticalSection);

	ID3D12CommandAllocator* pCommandAllocator = nullptr;

	if (!m_ReadyCommandAllocators.empty())
	{
		auto iter = m_ReadyCommandAllocators.front();

		// First indicates the fence value of the command allocator that was being used
		// if that value is less than the CompletedFenceValue then that allocator is ready to
		// be used again
		if (iter.first <= CompletedFenceValue)
		{
			pCommandAllocator = iter.second;
			ThrowCOMIfFailed(pCommandAllocator->Reset());
			m_ReadyCommandAllocators.pop();
		}
	}

	// If no allocator's were ready to be reused, create a new one
	if (!pCommandAllocator)
	{
		ThrowCOMIfFailed(m_pDevice->CreateCommandAllocator(m_Type, IID_PPV_ARGS(&pCommandAllocator)));
		wchar_t AllocatorName[32];
		swprintf(AllocatorName, 32, L"CommandAllocator %zu", m_CommandAllocatorPool.size());
		pCommandAllocator->SetName(AllocatorName);
		m_CommandAllocatorPool.push_back(pCommandAllocator);
	}

	return pCommandAllocator;
}

void CommandAllocatorPool::DiscardCommandAllocator(UINT64 FenceValue, ID3D12CommandAllocator* pCommandAllocator)
{
	if (!pCommandAllocator)
	{
		return;
	}

	ScopedCriticalSection SCS(CriticalSection);
	// That fence value indicates we are free to reset the allocator
	m_ReadyCommandAllocators.push(std::make_pair(FenceValue, pCommandAllocator));
}