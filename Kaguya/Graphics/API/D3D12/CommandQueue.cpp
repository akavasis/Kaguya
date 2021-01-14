#include "pch.h"
#include "CommandQueue.h"

void CommandQueue::Create(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE Type)
{
	D3D12_COMMAND_QUEUE_DESC Desc	= {};
	Desc.Type						= Type;
	Desc.Priority					= D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	Desc.Flags						= D3D12_COMMAND_QUEUE_FLAG_NONE;
	Desc.NodeMask					= 0;
	ThrowIfFailed(pDevice->CreateCommandQueue(&Desc, IID_PPV_ARGS(&m_CommandQueue)));
	m_CommandAllocatorPool.Create(pDevice, Type);
}

ID3D12CommandAllocator* CommandQueue::RequestCommandAllocator(UINT64 CompletedValue)
{
	return m_CommandAllocatorPool.RequestCommandAllocator(CompletedValue);
}

void CommandQueue::DiscardCommandAllocator(UINT64 FenceValue, ID3D12CommandAllocator* pCommandAllocator)
{
	m_CommandAllocatorPool.DiscardCommandAllocator(FenceValue, pCommandAllocator);
}