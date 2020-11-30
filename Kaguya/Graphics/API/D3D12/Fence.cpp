#include "pch.h"
#include "Fence.h"
#include "Device.h"

Fence::Fence(Device* pDevice)
{
	ThrowCOMIfFailed(pDevice->GetApiHandle()->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(m_pFence.ReleaseAndGetAddressOf())));
	m_EventHandle	= ::CreateEvent(nullptr, false, false, nullptr);
	m_ExpectedValue = 0;
}

UINT64 Fence::IncrementExpectedValue()
{
	return ++m_ExpectedValue;
}

UINT64 Fence::GetCompletedValue() const
{
	return m_pFence->GetCompletedValue();
}

void Fence::StallHostUntilCompletion()
{
	if (IsCompleted())
		return;

	// Fire an event that will be signaled to the m_EventHandle
	// when the internal pFence value has reached current fence.
	ThrowCOMIfFailed(m_pFence->SetEventOnCompletion(m_ExpectedValue, m_EventHandle));
	// Wait until the GPU hits current fence event is fired.
	// If dwMilliseconds is INFINITE, the function will return only when the handle object is signaled by the fence
	// WaitForSingleObject essentially stalls host
	::WaitForSingleObject(m_EventHandle, INFINITE);
}

bool Fence::IsCompleted() const
{
	return GetCompletedValue() >= m_ExpectedValue;
}

void Fence::Reset()
{
	// This should be called when the completed value reaches UINT64_MAX, because 
	// a device removal will occur once fence's internal value reaches UINT64_MAX
	m_ExpectedValue = 0;
	m_pFence->Signal(m_ExpectedValue);
}