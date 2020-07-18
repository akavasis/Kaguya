#include "pch.h"
#include "DescriptorHeap.h"
#include "Device.h"

Descriptor DescriptorAllocation::operator[](INT Index) const
{
	assert(Index >= 0 && Index < NumDescriptors && "Descriptor index out of range");
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(StartDescriptor.CPUHandle);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(StartDescriptor.GPUHandle);
	return { cpuHandle.Offset(Index, StartDescriptor.IncrementSize), gpuHandle.Offset(Index, StartDescriptor.IncrementSize), StartDescriptor.HeapIndex + Index, StartDescriptor.IncrementSize };
}

DescriptorHeap::DescriptorHeap(Device* pDevice, std::vector<UINT> Ranges, bool ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	// If you recorded a CPU descriptor handle into the command list (render target or depth stencil) then that descriptor can be reused immediately after the Set call, 
	// if you recorded a GPU descriptor handle into the command list (everything else) then that descriptor cannot be reused until gpu is done referencing them
	m_ShaderVisible = ShaderVisible;

	D3D12_DESCRIPTOR_HEAP_DESC desc;
	desc.Type = Type;
	desc.NumDescriptors = std::accumulate(Ranges.begin(), Ranges.end(), 0);
	desc.Flags = m_ShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	desc.NodeMask = 0;

	ThrowCOMIfFailed(pDevice->GetD3DDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pDescriptorHeap)));
	m_DescriptorIncrementSize = pDevice->GetDescriptorIncrementSize(Type);

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE();
	if (m_ShaderVisible)
	{
		gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	}

	m_DescriptorPartitions.resize(Ranges.size());
	for (std::size_t i = 0; i < Ranges.size(); ++i)
	{
		UINT offset = i == 0 ? 0 : Ranges[i - 1];
		m_DescriptorPartitions[i].RangeBlockInfo.StartCPUDescriptorHandle = cpuHandle.Offset(offset, m_DescriptorIncrementSize);
		m_DescriptorPartitions[i].RangeBlockInfo.StartGPUDescriptorHandle = gpuHandle.Offset(offset, m_DescriptorIncrementSize);
		m_DescriptorPartitions[i].RangeBlockInfo.Capacity = Ranges[i];
		m_DescriptorPartitions[i].Allocator.Reset(Ranges[i]);
	}
}

std::optional<DescriptorAllocation> DescriptorHeap::Allocate(INT PartitionIndex, UINT NumDescriptors)
{
	auto& descriptorPartition = GetDescriptorPartitionAt(PartitionIndex);

	auto pair = descriptorPartition.Allocator.Allocate(NumDescriptors);
	if (!pair)
		return std::nullopt;

	auto [offset, size] = pair.value();

	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(GetCPUHandleAt(PartitionIndex, offset));
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE();
	if (m_ShaderVisible)
	{
		gpuHandle = CD3DX12_GPU_DESCRIPTOR_HANDLE(GetGPUHandleAt(PartitionIndex, offset));
	}
	UINT heapIndex = PartitionIndex * descriptorPartition.RangeBlockInfo.Capacity + offset;
	Descriptor descriptor = { cpuHandle, gpuHandle, heapIndex, m_DescriptorIncrementSize };
	DescriptorAllocation descriptorAllocation;
	descriptorAllocation.StartDescriptor = descriptor;
	descriptorAllocation.NumDescriptors = NumDescriptors;
	descriptorAllocation.pOwningHeap = this;
	return descriptorAllocation;
}

void DescriptorHeap::Free(INT PartitionIndex, DescriptorAllocation& DescriptorAllocation)
{
	auto& descriptorPartition = GetDescriptorPartitionAt(PartitionIndex);
	std::size_t offset = (DescriptorAllocation.StartDescriptor.CPUHandle.ptr - descriptorPartition.RangeBlockInfo.StartCPUDescriptorHandle.ptr) / m_DescriptorIncrementSize;
	std::size_t size = DescriptorAllocation.NumDescriptors;
	descriptorPartition.Allocator.Free(offset, size);
	DescriptorAllocation = {};
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCPUHandleAt(INT PartitionIndex, INT Index) const
{
	auto& partition = GetDescriptorPartitionAt(PartitionIndex);
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(partition.RangeBlockInfo.StartCPUDescriptorHandle);
	return cpuHandle.Offset(Index, m_DescriptorIncrementSize);
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGPUHandleAt(INT PartitionIndex, INT Index) const
{
	auto& partition = GetDescriptorPartitionAt(PartitionIndex);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(partition.RangeBlockInfo.StartGPUDescriptorHandle);
	return gpuHandle.Offset(Index, m_DescriptorIncrementSize);
}

CBSRUADescriptorHeap::CBSRUADescriptorHeap(Device* pDevice, UINT NumCBDescriptors, UINT NumSRDescriptors, UINT NumUADescriptors, bool ShaderVisible)
	: DescriptorHeap(pDevice, { NumCBDescriptors, NumSRDescriptors, NumUADescriptors }, ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
{
}

std::optional<DescriptorAllocation> CBSRUADescriptorHeap::AllocateCBDescriptors(UINT NumDescriptors)
{
	return Allocate(RangeType::ConstantBuffer, NumDescriptors);
}

std::optional<DescriptorAllocation> CBSRUADescriptorHeap::AllocateSRDescriptors(UINT NumDescriptors)
{
	return Allocate(RangeType::ShaderResource, NumDescriptors);
}

std::optional<DescriptorAllocation> CBSRUADescriptorHeap::AllocateUADescriptors(UINT NumDescriptors)
{
	return Allocate(RangeType::UnorderedAccess, NumDescriptors);
}

SamplerDescriptorHeap::SamplerDescriptorHeap(Device* pDevice, std::vector<UINT> Ranges, bool ShaderVisible)
	: DescriptorHeap(pDevice, Ranges, ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER)
{
}

RenderTargetDescriptorHeap::RenderTargetDescriptorHeap(Device* pDevice, std::vector<UINT> Ranges)
	: DescriptorHeap(pDevice, Ranges, false, D3D12_DESCRIPTOR_HEAP_TYPE_RTV)
{
}

DepthStencilDescriptorHeap::DepthStencilDescriptorHeap(Device* pDevice, std::vector<UINT> Ranges)
	: DescriptorHeap(pDevice, Ranges, false, D3D12_DESCRIPTOR_HEAP_TYPE_DSV)
{
}