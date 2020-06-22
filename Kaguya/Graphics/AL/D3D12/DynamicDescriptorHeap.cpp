#include "pch.h"
#include "DynamicDescriptorHeap.h"
#include "Device.h"
#include "CommandList.h"
#include "RootSignature.h"

DynamicDescriptorHeap::DynamicDescriptorHeap(const Device* pDevice, D3D12_DESCRIPTOR_HEAP_TYPE Type)
	: m_pDevice(pDevice),
	m_Type(Type),
	m_DescriptorIncrementSize(pDevice->GetDescriptorIncrementSize(m_Type)),
	m_pCurrentHeap(nullptr),
	m_CurrentCPUDescriptorHandle(),
	m_CurrentGPUDescriptorHandle(),
	m_NumFreeHandles(0)
{
	m_DescriptorHandleCache = std::make_unique<D3D12_CPU_DESCRIPTOR_HANDLE[]>(NumDescriptorsPerHeap);
	ZeroMemory(&m_DescriptorTableCache, sizeof(m_DescriptorTableCache));
}

void DynamicDescriptorHeap::StageDescriptors(UINT RootParameterIndex, UINT Offset, UINT NumDescriptors, const D3D12_CPU_DESCRIPTOR_HANDLE SrcDescriptor)
{
	assert(RootParameterIndex >= 0 && RootParameterIndex < MaxDescriptorTables && "Root parameter index exceeds the max descriptor table index");
	assert(NumDescriptors < NumDescriptorsPerHeap && "Number of descriptors to be staged exceeds the number of descriptors in the heap");

	DescriptorTableCache& descriptorTableCache = m_DescriptorTableCache[RootParameterIndex];

	assert(Offset + NumDescriptors <= descriptorTableCache.NumDescriptors && "Number of descriptors exceeds the number of descripotrs in the descriptor table");

	D3D12_CPU_DESCRIPTOR_HANDLE* dstDescriptor = descriptorTableCache.BaseDescriptor + Offset;
	for (UINT i = 0; i < NumDescriptors; ++i)
	{
		dstDescriptor[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(SrcDescriptor, i, m_DescriptorIncrementSize);
	}

	m_StaleDescriptorTableBitMask.set(RootParameterIndex, true);
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDraw(CommandList* pCommandList)
{
	CommitStagedDescriptors(pCommandList, &ID3D12GraphicsCommandList::SetGraphicsRootDescriptorTable);
}

void DynamicDescriptorHeap::CommitStagedDescriptorsForDispatch(CommandList* pCommandList)
{
	CommitStagedDescriptors(pCommandList, &ID3D12GraphicsCommandList::SetComputeRootDescriptorTable);
}

void DynamicDescriptorHeap::ParseRootSignature(const RootSignature& RootSignature)
{
	m_StaleDescriptorTableBitMask.reset();

	m_DescriptorTableBitMask = RootSignature.GetDescriptorTableBitMask(m_Type);

	UINT descriptorOffset = 0;
	UINT rootParameterIndex = 0;
	if (!m_DescriptorTableBitMask.any())
		return;
	for (size_t i = 0; i < m_DescriptorTableBitMask.size(); ++i)
	{
		if (m_DescriptorTableBitMask.test(i))
		{
			rootParameterIndex = i;
			if (rootParameterIndex < RootSignature.GetD3DRootSignatureDesc().NumParameters)
			{
				UINT numDescriptors = RootSignature.GetNumDescriptors(rootParameterIndex);

				DescriptorTableCache& descriptorTableCache = m_DescriptorTableCache[rootParameterIndex];
				descriptorTableCache.NumDescriptors = numDescriptors;
				descriptorTableCache.BaseDescriptor = m_DescriptorHandleCache.get() + descriptorOffset;

				descriptorOffset += numDescriptors;
			}
		}
	}

	// Make sure the maximum number of descriptors per descriptor heap has not been exceeded.
	assert(descriptorOffset <= NumDescriptorsPerHeap && "The root signature requires more than the maximum number of descriptors per descriptor heap");
}

void DynamicDescriptorHeap::Reset()
{
	m_AvailableDescriptorHeaps = m_DescriptorHeapPool;
	m_pCurrentHeap = nullptr;
	m_CurrentCPUDescriptorHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	m_CurrentGPUDescriptorHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(D3D12_DEFAULT);
	m_NumFreeHandles = 0;
	m_DescriptorTableBitMask.reset();
	m_StaleDescriptorTableBitMask.reset();

	// Reset the table cache
	for (UINT i = 0; i < MaxDescriptorTables; ++i)
	{
		m_DescriptorTableCache[i].NumDescriptors = 0;
		m_DescriptorTableCache[i].BaseDescriptor = nullptr;
	}
}

ID3D12DescriptorHeap* DynamicDescriptorHeap::QueryDescriptorHeap()
{
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> pHeap;
	if (!m_AvailableDescriptorHeaps.empty())
	{
		pHeap = m_AvailableDescriptorHeaps.front();
		m_AvailableDescriptorHeaps.pop();
	}
	else
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc;
		desc.Type = m_Type;
		desc.NumDescriptors = NumDescriptorsPerHeap;
		desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
		desc.NodeMask = 0;

		m_pDevice->GetD3DDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&pHeap));
		m_DescriptorHeapPool.push(pHeap);
	}
	return pHeap.Get();
}

void DynamicDescriptorHeap::CommitStagedDescriptors(CommandList* pCommandList, std::function<void(ID3D12GraphicsCommandList*, UINT, D3D12_GPU_DESCRIPTOR_HANDLE)> SetFunction)
{
	// Compute the number of stale descriptors that need to be copied
	// to GPU visible descriptor heap.
	INT numDescriptorsToCommit = static_cast<INT>(m_StaleDescriptorTableBitMask.count());
	if (numDescriptorsToCommit <= 0) return;

	if (!m_pCurrentHeap || m_NumFreeHandles < numDescriptorsToCommit)
	{
		m_pCurrentHeap = QueryDescriptorHeap();
		m_CurrentCPUDescriptorHandle = m_pCurrentHeap->GetCPUDescriptorHandleForHeapStart();
		m_CurrentGPUDescriptorHandle = m_pCurrentHeap->GetGPUDescriptorHandleForHeapStart();
		m_NumFreeHandles = NumDescriptorsPerHeap;

		//pCommandList->SetDescriptorHeap(m_Type, m_pCurrentHeap);

		// When updating the descriptor heap on the command list, all descriptor
		// tables must be (re)recopied to the new descriptor heap (not just
		// the stale descriptor tables).
		m_StaleDescriptorTableBitMask = m_DescriptorTableBitMask;
	}

	if (!m_StaleDescriptorTableBitMask.any()) return;

	UINT rootParameterIndex = 0;
	for (size_t i = 0; i < m_StaleDescriptorTableBitMask.size(); ++i)
	{
		if (m_StaleDescriptorTableBitMask.test(i))
		{
			rootParameterIndex = i;
			UINT numSrcDescriptors = m_DescriptorTableCache[rootParameterIndex].NumDescriptors;
			D3D12_CPU_DESCRIPTOR_HANDLE* pSrvDescriptorHandles = m_DescriptorTableCache[rootParameterIndex].BaseDescriptor;

			auto pD3DDevice = m_pDevice->GetD3DDevice();
			auto pD3DCommandList = pCommandList->GetD3DCommandList();

			pD3DDevice->CopyDescriptors(1, &m_CurrentCPUDescriptorHandle, &numSrcDescriptors, numSrcDescriptors, pSrvDescriptorHandles, nullptr, m_Type);

			// Set the descriptors on the command list using SetFunction
			SetFunction(pD3DCommandList, rootParameterIndex, m_CurrentGPUDescriptorHandle);

			// Offset current CPU and GPU descriptor handles.
			m_CurrentCPUDescriptorHandle.ptr = SIZE_T(INT64(m_CurrentCPUDescriptorHandle.ptr) + INT64(numSrcDescriptors) * INT64(m_DescriptorIncrementSize));
			m_CurrentGPUDescriptorHandle.ptr = UINT64(INT64(m_CurrentGPUDescriptorHandle.ptr) + INT64(numSrcDescriptors) * INT64(m_DescriptorIncrementSize));
			m_NumFreeHandles -= numSrcDescriptors;

			// Flip the stale bit so the descriptor table is not recopied again unless it is updated with a new descriptor.
			m_StaleDescriptorTableBitMask.flip(i);
		}
	}
}