#include "pch.h"
#include "CommandQueue.h"
#include "Device.h"

CommandQueue::CommandQueue(Device* pDevice, D3D12_COMMAND_LIST_TYPE Type)
	: m_FenceValue(0),
	m_AllocatorPool(pDevice, Type)
{
	auto pD3DDevice = pDevice->GetD3DDevice();

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc;
	commandQueueDesc.Type = Type;
	commandQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	commandQueueDesc.NodeMask = 0;
	ThrowCOMIfFailed(pD3DDevice->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&m_pCommandQueue)));
	ThrowCOMIfFailed(hr = pD3DDevice->CreateFence(m_FenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pFence)));
	m_EventHandle = ::CreateEvent(nullptr, false, false, nullptr);
}

CommandQueue::~CommandQueue()
{
	WaitForIdle();
	::CloseHandle(m_EventHandle);
}

UINT64 CommandQueue::Signal()
{
	// Signals the command queue to update the pFence's internal value on the GPU side
	// once commands have been finished for execution for that frame
	UINT64 fenceToWaitFor = ++m_FenceValue;
	m_pCommandQueue->Signal(m_pFence.Get(), fenceToWaitFor);
	return fenceToWaitFor;
}

void CommandQueue::Flush()
{
	WaitForFenceValue(m_FenceValue);
}

bool CommandQueue::IsFenceValueReached(UINT64 FenceValue)
{
	// Retrives current value for the fence and compare it against the argument, it will be updated by the ID3D12CommandQueue::Signal
	// once it has been signaled
	const UINT64 completedValue = m_pFence->GetCompletedValue();
	return completedValue >= FenceValue;
}

void CommandQueue::WaitForFenceValue(UINT64 FenceValue)
{
	if (IsFenceValueReached(FenceValue))
		return;
	// If the command queue's fence value has not reached argument FenceValue (e.g. is still processing its commands submitted by the CPU)
	// then we create an event that will stall any CPU processing and once argument FenceValue have been reached
	
	// Fire an event that will be signaled to the m_EventHandle
	// when the internal pFence value has reached current fence.
	ThrowCOMIfFailed(m_pFence->SetEventOnCompletion(FenceValue, m_EventHandle));
	// Wait until the GPU hits current fence event is fired.
	// If dwMilliseconds is INFINITE, the function will return only when the handle object is signaled by the fence
	// WaitForSingleObject essentially stalls CPU
	::WaitForSingleObject(m_EventHandle, INFINITE);
}

void CommandQueue::WaitForIdle()
{
	// NOTE: There is a known debug layer bug which can cause problems if you call IDXGISwapChain::Present without rendering.
	WaitForFenceValue(Signal());
}

ID3D12CommandAllocator* CommandQueue::RequestAllocator()
{
	UINT64 CompletedFence = m_pFence->GetCompletedValue();
	return m_AllocatorPool.RequestAllocator(CompletedFence);
}

void CommandQueue::MarkAllocatorAsActive(UINT64 FenceValueForReset, ID3D12CommandAllocator* Allocator)
{
	m_AllocatorPool.MarkAllocatorAsActive(FenceValueForReset, Allocator);
}