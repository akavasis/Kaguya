#include "pch.h"
#include "DescriptorHeap.h"
#include "d3dx12.h"

void DescriptorHeap::Create(ID3D12Device* pDevice, std::vector<UINT> Ranges, bool ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	// If you recorded a CPU descriptor handle into the command list (render target or depth stencil) then that descriptor can be reused immediately after the Set call, 
	// if you recorded a GPU descriptor handle into the command list (everything else) then that descriptor cannot be reused until gpu is done referencing them
	m_ShaderVisible = ShaderVisible;

	D3D12_DESCRIPTOR_HEAP_DESC Desc =
	{
		.Type = Type,
		.NumDescriptors = std::accumulate(Ranges.begin(), Ranges.end(), 0u),
		.Flags = m_ShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		.NodeMask = 0
	};

	ThrowIfFailed(pDevice->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&m_DescriptorHeap)));
	m_DescriptorIncrementSize = pDevice->GetDescriptorHandleIncrementSize(Type);

	D3D12_CPU_DESCRIPTOR_HANDLE StartCpuHandle = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE StartGpuHandle = m_ShaderVisible ? m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart() : D3D12_GPU_DESCRIPTOR_HANDLE{};
	m_StartDescriptor = { StartCpuHandle, StartGpuHandle, 0 };

	UINT Index = 0;
	m_DescriptorPartitions.resize(Ranges.size());
	for (std::size_t i = 0; i < Ranges.size(); ++i)
	{
		D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = {};
		D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = {};
		CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted(CpuHandle, StartCpuHandle, Index, m_DescriptorIncrementSize);
		CD3DX12_GPU_DESCRIPTOR_HANDLE::InitOffsetted(GpuHandle, StartGpuHandle, Index, m_DescriptorIncrementSize);

		m_DescriptorPartitions[i].StartDescriptor = { CpuHandle, GpuHandle, Index };
		m_DescriptorPartitions[i].NumDescriptors = Ranges[i];

		Index += Ranges[i];
	}
}

Descriptor DescriptorHeap::GetDescriptorAt(INT PartitionIndex, UINT Index) const
{
	const auto& Partition = GetDescriptorPartitionAt(PartitionIndex);
	D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted(CpuHandle, Partition.StartDescriptor.CpuHandle, Index, m_DescriptorIncrementSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE::InitOffsetted(GpuHandle, Partition.StartDescriptor.GpuHandle, Index, m_DescriptorIncrementSize);

	return { CpuHandle, GpuHandle, Index };
}