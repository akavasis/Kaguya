#pragma once
#include <d3d12.h>
#include <wrl/client.h>

class Device;

class Fence
{
public:
	Fence(Device* pDevice);

	UINT64 IncrementExpectedValue();
	UINT64 GetCompletedValue() const;
	void StallHostUntilCompletion();
	bool IsCompleted() const;

	void Reset();
private:
	Microsoft::WRL::ComPtr<ID3D12Fence1> m_pFence;
	HANDLE								 m_EventHandle;
	UINT64								 m_ExpectedValue;
};