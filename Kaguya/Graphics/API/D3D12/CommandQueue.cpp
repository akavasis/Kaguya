#include "pch.h"
#include "CommandQueue.h"
#include "Device.h"

CommandQueue::CommandQueue(Device* pDevice, D3D12_COMMAND_LIST_TYPE Type)
	: m_CommandAllocatorPool(pDevice, Type)
{
	D3D12_COMMAND_QUEUE_DESC Desc	= {};
	Desc.Type						= Type;
	Desc.Priority					= D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	Desc.Flags						= D3D12_COMMAND_QUEUE_FLAG_NONE;
	Desc.NodeMask					= 0;
	ThrowCOMIfFailed(pDevice->GetApiHandle()->CreateCommandQueue(&Desc, IID_PPV_ARGS(&m_ApiHandle)));

	ThrowCOMIfFailed(pDevice->GetApiHandle()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence)));
	m_FenceCompletionEvent.create();
	m_FenceValue = 0;
}

CommandQueue::~CommandQueue()
{

}

UINT64 CommandQueue::Signal()
{
	// Signals the command queue to update the pFence's internal value on the GPU side
	// once commands have been finished for execution for that frame
	UINT64 FenceToWaitFor = ++m_FenceValue;
	m_ApiHandle->Signal(m_pFence.Get(), FenceToWaitFor);
	return FenceToWaitFor;
}

void CommandQueue::Flush()
{
	WaitForFenceValue(m_FenceValue);
}

bool CommandQueue::IsFenceValueReached(UINT64 FenceValue)
{
	// Retrives current value for the fence and compare it against the argument, it will be updated by the ID3D12CommandQueue::Signal
	// once it has been signaled
	const UINT64 CompletedValue = m_pFence->GetCompletedValue();
	return CompletedValue >= FenceValue;
}

void CommandQueue::WaitForFenceValue(UINT64 FenceValue)
{
	if (IsFenceValueReached(FenceValue))
		return;
	// If the command queue's fence value has not reached argument FenceValue (e.g. is still processing its commands submitted by the CPU)
	// then we create an event that will stall any CPU processing and once argument FenceValue have been reached
	
	// Signal the Completion event when the fence's internal value has reached the function argument value
	ThrowCOMIfFailed(m_pFence->SetEventOnCompletion(FenceValue, m_FenceCompletionEvent.get()));
	m_FenceCompletionEvent.wait();
}

void CommandQueue::WaitForIdle()
{
	// NOTE: There is a known debug layer bug which can cause problems if you call IDXGISwapChain::Present without rendering.
	WaitForFenceValue(Signal());
}

void CommandQueue::Wait(const CommandQueue& CommandQueue)
{
	m_ApiHandle->Wait(CommandQueue.m_pFence.Get(), CommandQueue.m_FenceValue);
}

ID3D12CommandAllocator* CommandQueue::RequestAllocator()
{
	UINT64 CompletedFence = m_pFence->GetCompletedValue();
	return m_CommandAllocatorPool.RequestAllocator(CompletedFence);
}

void CommandQueue::MarkAllocatorAsActive(UINT64 FenceValueForReset, ID3D12CommandAllocator* Allocator)
{
	m_CommandAllocatorPool.MarkAllocatorAsActive(FenceValueForReset, Allocator);
}