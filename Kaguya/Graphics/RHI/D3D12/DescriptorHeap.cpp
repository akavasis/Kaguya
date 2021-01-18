#include "pch.h"
#include "DescriptorHeap.h"
#include "d3dx12.h"

void DescriptorHeap::Create(ID3D12Device* pDevice, UINT NumDescriptors, bool ShaderVisible, D3D12_DESCRIPTOR_HEAP_TYPE Type)
{
	// If you recorded a CPU descriptor handle into the command list (render target or depth stencil) then that descriptor can be reused immediately after the Set call, 
	// if you recorded a GPU descriptor handle into the command list (everything else) then that descriptor cannot be reused until gpu is done referencing them
	m_ShaderVisible = ShaderVisible;

	D3D12_DESCRIPTOR_HEAP_DESC Desc =
	{
		.Type = Type,
		.NumDescriptors = NumDescriptors,
		.Flags = m_ShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE,
		.NodeMask = 0
	};

	ThrowIfFailed(pDevice->CreateDescriptorHeap(&Desc, IID_PPV_ARGS(&m_DescriptorHeap)));
	m_DescriptorIncrementSize = pDevice->GetDescriptorHandleIncrementSize(Type);

	D3D12_CPU_DESCRIPTOR_HANDLE StartCpuHandle = m_DescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE StartGpuHandle = {};
	if (m_ShaderVisible)
		StartGpuHandle = m_DescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	m_StartDescriptor = { StartCpuHandle, StartGpuHandle, 0 };
}

Descriptor DescriptorHeap::operator[](UINT Index) const
{
	D3D12_CPU_DESCRIPTOR_HANDLE CpuHandle = {};
	D3D12_GPU_DESCRIPTOR_HANDLE GpuHandle = {};
	CD3DX12_CPU_DESCRIPTOR_HANDLE::InitOffsetted(CpuHandle, m_StartDescriptor.CpuHandle, Index, m_DescriptorIncrementSize);
	CD3DX12_GPU_DESCRIPTOR_HANDLE::InitOffsetted(GpuHandle, m_StartDescriptor.GpuHandle, Index, m_DescriptorIncrementSize);

	return
	{
		.CpuHandle = CpuHandle,
		.GpuHandle = GpuHandle,
		.Index = Index
	};
}