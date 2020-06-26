#pragma once
#include <d3d12.h>
#include <wrl/client.h>

#include <memory>
#include <functional>
#include <queue>
#include <bitset>

class Device;
class RootSignature;

class DynamicDescriptorHeap
{
public:
	DynamicDescriptorHeap(const Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE Type);

	void StageDescriptors(UINT RootParameterIndex, UINT Offset, UINT NumDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor);

	void CommitStagedDescriptorsForDraw(ID3D12GraphicsCommandList* pCommandList);
	void CommitStagedDescriptorsForDispatch(ID3D12GraphicsCommandList* pCommandList);

	void ParseRootSignature(const RootSignature* RootSignature);

	void Reset();
private:

	ID3D12DescriptorHeap* QueryDescriptorHeap();
	void CommitStagedDescriptors(ID3D12GraphicsCommandList* pCommandList, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> SetFunction);

	enum : UINT { NumDescriptorsPerHeap = 2048 };
	enum : UINT { MaxDescriptorTables = 32 };
	const Device* m_pDevice;
	const D3D12_DESCRIPTOR_HEAP_TYPE m_Type;
	const UINT m_DescriptorIncrementSize;
	ID3D12DescriptorHeap* m_pCurrentHeap;
	D3D12_CPU_DESCRIPTOR_HANDLE m_CurrentCPUDescriptorHandle;
	D3D12_GPU_DESCRIPTOR_HANDLE m_CurrentGPUDescriptorHandle;
	INT m_NumFreeHandles;

	// Each bit in the bit mask represents the index in the root signature
	// that contains a descriptor table.
	std::bitset<MaxDescriptorTables> m_DescriptorTableBitMask;
	// Each bit set in the bit mask represents a descriptor table
	// in the root signature that has changed since the last time the 
	// descriptors were copied.
	std::bitset<MaxDescriptorTables> m_StaleDescriptorTableBitMask;

	std::queue<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> m_DescriptorHeapPool;
	std::queue<Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>> m_AvailableDescriptorHeaps;

	// A structure that represents a descriptor table entry in the root signature.
	// Descriptor handle cache per descriptor table.
	struct DescriptorTableCache
	{
		UINT NumDescriptors;
		// The pointer to the descriptor in the descriptor handle cache.
		D3D12_CPU_DESCRIPTOR_HANDLE* BaseDescriptor;
	} m_DescriptorTableCache[MaxDescriptorTables];
	std::unique_ptr<D3D12_CPU_DESCRIPTOR_HANDLE[]> m_DescriptorHandleCache;
};