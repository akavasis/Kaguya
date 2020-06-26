#include "pch.h"
#include "CommandAllocatorPool.h"
#include "Device.h"

CommandAllocatorPool::CommandAllocatorPool(const Device* pDevice, D3D12_COMMAND_LIST_TYPE Type)
	: m_Device(pDevice),
	m_Type(Type)
{
}

CommandAllocatorPool::~CommandAllocatorPool()
{
	for (auto allocator : m_AllocatorPool)
		allocator->Release();
	m_AllocatorPool.clear();
}

ID3D12CommandAllocator* CommandAllocatorPool::RequestAllocator(UINT64 CompletedFenceValue)
{
	std::scoped_lock lk(m_AllocatorMutex);

	ID3D12CommandAllocator* pAllocator = nullptr;

	if (!m_ReadyAllocators.empty())
	{
		auto iter = m_ReadyAllocators.front();

		// First indicates the fence value of the command allocator that was being used
		// if that value is less than the CompletedFenceValue then that allocator is ready to
		// be used again
		if (iter.first <= CompletedFenceValue)
		{
			pAllocator = iter.second;
			ThrowCOMIfFailed(pAllocator->Reset());
			m_ReadyAllocators.pop();
		}
	}

	// If no allocator's were ready to be reused, create a new one
	if (pAllocator == nullptr)
	{
		ThrowCOMIfFailed(m_Device->GetD3DDevice()->CreateCommandAllocator(m_Type, IID_PPV_ARGS(&pAllocator)));
		wchar_t AllocatorName[32];
		swprintf(AllocatorName, 32, L"CommandAllocator %zu", m_AllocatorPool.size());
		pAllocator->SetName(AllocatorName);
		m_AllocatorPool.push_back(pAllocator);
	}

	return pAllocator;
}

void CommandAllocatorPool::MarkAllocatorAsActive(UINT64 FenceValue, ID3D12CommandAllocator* Allocator)
{
	std::scoped_lock lk(m_AllocatorMutex);
	// That fence value indicates we are free to reset the allocator
	m_ReadyAllocators.push(std::make_pair(FenceValue, Allocator));
}