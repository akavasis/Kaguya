#include "pch.h"
#include "CommandQueue.h"

CommandQueue::CommandQueue(ID3D12Device* pDevice, D3D12_COMMAND_LIST_TYPE Type)
	: m_Type(Type),
	m_CommandAllocatorPool(pDevice, Type)
{
	D3D12_COMMAND_QUEUE_DESC Desc	= {};
	Desc.Type						= Type;
	Desc.Priority					= D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	Desc.Flags						= D3D12_COMMAND_QUEUE_FLAG_NONE;
	Desc.NodeMask					= 0;
	ThrowCOMIfFailed(pDevice->CreateCommandQueue(&Desc, IID_PPV_ARGS(&m_ApiHandle)));
}

ID3D12CommandAllocator* CommandQueue::RequestAllocator(UINT64 CompletedValue)
{
	return m_CommandAllocatorPool.RequestCommandAllocator(CompletedValue);
}

void CommandQueue::DiscardCommandAllocator(UINT64 FenceValue, ID3D12CommandAllocator* Allocator)
{
	m_CommandAllocatorPool.DiscardCommandAllocator(FenceValue, Allocator);
}