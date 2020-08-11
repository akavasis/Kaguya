#include "pch.h"
#include "DescriptorHeap.h"
#include "Device.h"

DescriptorAllocation::DescriptorAllocation()
	: StartDescriptor(Descriptor()),
	NumDescriptors(0),
	IncrementSize(0),
	PartitonIndex(-1),
	pOwningHeap(nullptr)
{
}

DescriptorAllocation::DescriptorAllocation(Descriptor StartDescriptor, UINT NumDescriptors, UINT IncrementSize, INT PartitonIndex, DescriptorHeap* pOwningHeap)
	: StartDescriptor(StartDescriptor),
	NumDescriptors(NumDescriptors),
	IncrementSize(IncrementSize),
	PartitonIndex(PartitonIndex),
	pOwningHeap(pOwningHeap)
{
}

DescriptorAllocation::~DescriptorAllocation()
{
	if (*this && pOwningHeap)
	{
		pOwningHeap->Free(PartitonIndex, std::move(*this));

		StartDescriptor = Descriptor();
		NumDescriptors = 0;
		IncrementSize = 0;
		PartitonIndex = -1;
		pOwningHeap = nullptr;
	}
}

DescriptorAllocation::DescriptorAllocation(DescriptorAllocation&& rvalue) noexcept
	: DescriptorAllocation(
		rvalue.StartDescriptor,
		rvalue.NumDescriptors,
		rvalue.IncrementSize,
		rvalue.PartitonIndex,
		rvalue.pOwningHeap)
{
	rvalue.StartDescriptor = Descriptor();
	rvalue.NumDescriptors = 0;
	rvalue.IncrementSize = 0;
	rvalue.PartitonIndex = -1;
	rvalue.pOwningHeap = nullptr;
}

DescriptorAllocation& DescriptorAllocation::operator=(DescriptorAllocation&& rvalue) noexcept
{
	if (this != &rvalue)
	{
		this->~DescriptorAllocation();

		StartDescriptor = rvalue.StartDescriptor;
		NumDescriptors = rvalue.NumDescriptors;
		IncrementSize = rvalue.IncrementSize;
		PartitonIndex = rvalue.PartitonIndex;
		pOwningHeap = rvalue.pOwningHeap;

		rvalue.StartDescriptor = Descriptor();
		rvalue.NumDescriptors = 0;
		rvalue.IncrementSize = 0;
		rvalue.PartitonIndex = -1;
		rvalue.pOwningHeap = nullptr;
	}
	return *this;
}

Descriptor DescriptorAllocation::operator[](INT Index) const
{
	assert(Index >= 0 && Index < NumDescriptors && "Descriptor index out of range");
	CD3DX12_CPU_DESCRIPTOR_HANDLE cpuHandle(StartDescriptor.CPUHandle);
	CD3DX12_GPU_DESCRIPTOR_HANDLE gpuHandle(StartDescriptor.GPUHandle);
	return { cpuHandle.Offset(Index, IncrementSize), gpuHandle.Offset(Index, IncrementSize), StartDescriptor.HeapIndex + Index };
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

	D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = m_pDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = {};
	if (m_ShaderVisible)
		GpuHandle = m_pDescriptorHeap->GetGPUDescriptorHandleForHeapStart();

	UINT heapIndex = 0;
	m_DescriptorPartitions.resize(Ranges.size());
	for (std::size_t i = 0; i < Ranges.size(); ++i)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {}; CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted(cpuHandle, CpuHandle, heapIndex, m_DescriptorIncrementSize);
		D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {}; CD3DX12_GPU_DESCRIPTOR_HANDLE::InitOffsetted(gpuHandle, GpuHandle, heapIndex, m_DescriptorIncrementSize);

		Descriptor descriptor = { cpuHandle, gpuHandle, heapIndex };
		m_DescriptorPartitions[i].Allocation = DescriptorAllocation(descriptor, Ranges[i], m_DescriptorIncrementSize, i, this);
		m_DescriptorPartitions[i].Allocator.Reset(Ranges[i]);

		heapIndex += Ranges[i];
	}
}

DescriptorAllocation DescriptorHeap::Allocate(INT PartitionIndex, UINT NumDescriptors)
{
	auto& descriptorPartition = GetDescriptorPartitionAt(PartitionIndex);

	auto pair = descriptorPartition.Allocator.Allocate(NumDescriptors);
	if (!pair)
		return DescriptorAllocation();

	auto [offset, size] = pair.value();

	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = GetCPUHandleAt(PartitionIndex, offset);
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {};
	if (m_ShaderVisible)
		gpuHandle = GetGPUHandleAt(PartitionIndex, offset);

	UINT heapIndex = PartitionIndex * descriptorPartition.Allocation.NumDescriptors + offset;
	Descriptor descriptor = { cpuHandle, gpuHandle, heapIndex };
	return DescriptorAllocation(descriptor, NumDescriptors, m_DescriptorIncrementSize, PartitionIndex, this);
}

void DescriptorHeap::Free(INT PartitionIndex, DescriptorAllocation&& DescriptorAllocation)
{
	if (!DescriptorAllocation)
		return;
	auto& descriptorPartition = GetDescriptorPartitionAt(PartitionIndex);
	std::size_t offset = (DescriptorAllocation.StartDescriptor.CPUHandle.ptr - descriptorPartition.Allocation.StartDescriptor.CPUHandle.ptr) / m_DescriptorIncrementSize;
	std::size_t size = DescriptorAllocation.NumDescriptors;
	descriptorPartition.Allocator.Free(offset, size);
}

D3D12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCPUHandleAt(INT PartitionIndex, INT Index) const
{
	const auto& partition = GetDescriptorPartitionAt(PartitionIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle = {}; CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted(cpuHandle, partition.Allocation.StartDescriptor.CPUHandle, Index, m_DescriptorIncrementSize);
	return cpuHandle;
}

D3D12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGPUHandleAt(INT PartitionIndex, INT Index) const
{
	const auto& partition = GetDescriptorPartitionAt(PartitionIndex);
	D3D12_GPU_DESCRIPTOR_HANDLE gpuHandle = {}; CD3DX12_GPU_DESCRIPTOR_HANDLE::InitOffsetted(gpuHandle, partition.Allocation.StartDescriptor.GPUHandle, Index, m_DescriptorIncrementSize);
	return gpuHandle;
}

CBSRUADescriptorHeap::CBSRUADescriptorHeap(Device* pDevice, UINT NumCBDescriptors, UINT NumSRDescriptors, UINT NumUADescriptors, bool ShaderVisible)
	: DescriptorHeap(pDevice, { NumCBDescriptors, NumSRDescriptors, NumUADescriptors }, ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)
{
}

DescriptorAllocation CBSRUADescriptorHeap::AllocateCBDescriptors(UINT NumDescriptors)
{
	return Allocate(RangeType::ConstantBuffer, NumDescriptors);
}

DescriptorAllocation CBSRUADescriptorHeap::AllocateSRDescriptors(UINT NumDescriptors)
{
	return Allocate(RangeType::ShaderResource, NumDescriptors);
}

DescriptorAllocation CBSRUADescriptorHeap::AllocateUADescriptors(UINT NumDescriptors)
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